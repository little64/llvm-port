//===- Little64.cpp -------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "Symbols.h"
#include "SyntheticSections.h"
#include "Target.h"
#include "RelocScan.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {

// ---------------------------------------------------------------------------
// Little64 16-bit instruction encoding helpers.
// All instructions are 16-bit little-endian.
// ---------------------------------------------------------------------------

// LS opcode constants (4-bit, shared by format 00 and format 01).
enum : unsigned { LS_LOAD = 0, LS_STORE = 1, LS_MOVE = 4 };

// GP opcode constants (5-bit, format 110).
enum : unsigned { GP_ADD = 0 };

// Register numbers.
enum : unsigned {
  REG_R0 = 0,
  REG_R3 = 3,
  REG_R10 = 10,
  REG_R12 = 12,
  REG_R15 = 15,
};

// Format 111: unconditional jump.  bits[15:13]=111, bits[12:0]=signed offset.
static uint16_t encJMP(int16_t off) { return 0xE000u | (off & 0x1FFF); }

// Format 01 (LS PC-relative):
//   bits[15:14]=01, [13:10]=opc, [9:4]=pcRel(6b signed), [3:0]=Rd.
static uint16_t encF01(unsigned opc, int8_t pcRel, unsigned Rd) {
  return 0x4000u | ((opc & 0xF) << 10) | ((pcRel & 0x3F) << 4) | (Rd & 0xF);
}

// Format 00 (LS register-indirect):
//   bits[15:14]=00, [13:10]=opc, [9:8]=off2, [7:4]=Rs1, [3:0]=Rd.
static uint16_t encF00(unsigned opc, unsigned off2, unsigned Rs1, unsigned Rd) {
  return ((opc & 0xF) << 10) | ((off2 & 0x3) << 8) |
         ((Rs1 & 0xF) << 4) | (Rd & 0xF);
}

// Format 110 (GP ALU): bits[15:13]=110, [12:8]=opc, [7:4]=Rs1, [3:0]=Rd.
static uint16_t encGP(unsigned opc, unsigned Rs1, unsigned Rd) {
  return 0xC000u | ((opc & 0x1F) << 8) | ((Rs1 & 0xF) << 4) | (Rd & 0xF);
}

// Format 10 (LDI): bits[15:14]=10, [13:12]=shift, [11:4]=imm8, [3:0]=Rd.
static uint16_t encLDI(unsigned shift, uint8_t imm, unsigned Rd) {
  return 0x8000u | ((shift & 0x3) << 12) | ((imm & 0xFF) << 4) | (Rd & 0xF);
}

// NOP = LDI #0, R0.
static constexpr uint16_t NOP = 0x8000;

// ---------------------------------------------------------------------------

class Little64 final : public TargetInfo {
public:
  Little64(Ctx &ctx);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  RelType getDynRel(RelType type) const override;
  int64_t getImplicitAddend(const uint8_t *buf, RelType type) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
  void writeGotHeader(uint8_t *buf) const override;
  void writeGotPlt(uint8_t *buf, const Symbol &s) const override;
  void writeIgotPlt(uint8_t *buf, const Symbol &s) const override;
  void writePltHeader(uint8_t *buf) const override;
  void writePlt(uint8_t *buf, const Symbol &sym,
                uint64_t pltEntryAddr) const override;
  template <class ELFT, class RelTy>
  void scanSectionImpl(InputSectionBase &sec, Relocs<RelTy> rels);
  void scanSection(InputSectionBase &sec) override;
};
} // namespace

Little64::Little64(Ctx &ctx) : TargetInfo(ctx) {
  copyRel = R_LITTLE64_COPY;
  pltRel = R_LITTLE64_JUMP_SLOT;
  relativeRel = R_LITTLE64_RELATIVE;
  symbolicRel = R_LITTLE64_ABS64;
  gotRel = R_LITTLE64_GLOB_DAT;
  tlsModuleIndexRel = R_LITTLE64_TLS_DTPMOD64;
  tlsOffsetRel = R_LITTLE64_TLS_DTPREL64;
  tlsGotRel = R_LITTLE64_TLS_TPREL64;
  iRelativeRel = R_LITTLE64_IRELATIVE;

  gotHeaderEntriesNum = 1;    // .got[0] = _DYNAMIC
  gotPltHeaderEntriesNum = 2; // .got.plt[0] = resolver, [1] = link_map

  pltHeaderSize = 32;
  pltEntrySize = 20;
  ipltEntrySize = 20;
}

RelExpr Little64::getRelExpr(RelType type, const Symbol &s,
                             const uint8_t *loc) const {
  switch (type) {
  case R_LITTLE64_NONE:
    return R_NONE;
  case R_LITTLE64_ABS64:
  case R_LITTLE64_ABS32:
    return R_ABS;
  case R_LITTLE64_PCREL6:
  case R_LITTLE64_PCREL10:
  case R_LITTLE64_PCREL13:
  case R_LITTLE64_PCREL32:
    return R_PC;
  case R_LITTLE64_GOT64:
  case R_LITTLE64_TLS_GOTTPREL:
    return R_GOT_PC;
  case R_LITTLE64_TLS_TPREL64:
    return R_TPREL;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unknown relocation (" << type.v
             << ") against symbol " << &s;
    return R_NONE;
  }
}

RelType Little64::getDynRel(RelType type) const {
  if (type == symbolicRel)
    return type;
  return R_LITTLE64_NONE;
}

int64_t Little64::getImplicitAddend(const uint8_t *buf, RelType type) const {
  switch (type) {
  default:
    InternalErr(ctx, buf) << "cannot read addend for relocation " << type;
    return 0;
  case R_LITTLE64_NONE:
  case R_LITTLE64_JUMP_SLOT:
    return 0;
  case R_LITTLE64_ABS32:
  case R_LITTLE64_PCREL32:
    return SignExtend64<32>(read32le(buf));
  case R_LITTLE64_ABS64:
  case R_LITTLE64_RELATIVE:
  case R_LITTLE64_IRELATIVE:
  case R_LITTLE64_TLS_DTPMOD64:
  case R_LITTLE64_TLS_DTPREL64:
  case R_LITTLE64_TLS_TPREL64:
  case R_LITTLE64_TLS_GOTTPREL:
  case R_LITTLE64_GOT64:
  case R_LITTLE64_GLOB_DAT:
    return read64le(buf);
  case R_LITTLE64_PCREL6: {
    uint16_t insn = read16le(buf);
    return SignExtend64<6>((insn >> 4) & 0x3f);
  }
  case R_LITTLE64_PCREL10: {
    uint16_t insn = read16le(buf);
    return SignExtend64<10>(insn & 0x3ff);
  }
  case R_LITTLE64_PCREL13: {
    uint16_t insn = read16le(buf);
    return SignExtend64<13>(insn & 0x1fff);
  }
  }
}

void Little64::writeGotHeader(uint8_t *buf) const {
  // .got[0] = address of _DYNAMIC.
  write64le(buf, ctx.mainPart->dynamic->getVA());
}

void Little64::writeGotPlt(uint8_t *buf, const Symbol &s) const {
  // Each per-symbol .got.plt entry initially points to PLT[0] (resolver).
  write64le(buf, ctx.in.plt->getVA());
}

void Little64::writeIgotPlt(uint8_t *buf, const Symbol &s) const {
  if (ctx.arg.writeAddends)
    write64le(buf, s.getVA(ctx));
}

void Little64::writePltHeader(uint8_t *buf) const {
  // PLT[0]: resolver stub (32 bytes).
  // Loads .got.plt base via PC-relative literal-slot addressing, then reads
  // resolver (.got.plt[0]) and link_map (.got.plt[1]), jumps to resolver.
  //
  // Clobbers: R3, R10, R12.
  //
  // offset 0:  JMP +4                              skip literal
  // offset 2:  .quad (gotPlt - (plt+2))             displacement
  // offset 10: MOVE_PCREL R3, [pc-5]               R3 = literal addr
  // offset 12: LOAD R12, [R3+0]                    R12 = displacement
  // offset 14: ADD R3, R12                         R3 = &.got.plt[0]
  // offset 16: LOAD R12, [R3+0]                    R12 = resolver
  // offset 18: LDI 8, R10                          R10 = 8
  // offset 20: ADD R3, R10                         R3 = &.got.plt[1]
  // offset 22: LOAD R10, [R3+0]                    R10 = link_map
  // offset 24: JMPr R12                            jump to resolver
  // offset 26..31: NOP (padding)

  uint64_t gotPlt = ctx.in.gotPlt->getVA();
  uint64_t plt = ctx.in.plt->getVA();
  int64_t disp = static_cast<int64_t>(gotPlt) - static_cast<int64_t>(plt + 2);

  write16le(buf + 0, encJMP(4));
  write64le(buf + 2, static_cast<uint64_t>(disp));
  write16le(buf + 10, encF01(LS_MOVE, -5, REG_R3));    // MOVE_PCREL R3,[pc-5]
  write16le(buf + 12, encF00(LS_LOAD, 0, REG_R3, REG_R12));
  write16le(buf + 14, encGP(GP_ADD, REG_R12, REG_R3)); // R3 += R12
  write16le(buf + 16, encF00(LS_LOAD, 0, REG_R3, REG_R12));
  write16le(buf + 18, encLDI(0, 8, REG_R10));
  write16le(buf + 20, encGP(GP_ADD, REG_R10, REG_R3)); // R3 += 8
  write16le(buf + 22, encF00(LS_LOAD, 0, REG_R3, REG_R10));
  write16le(buf + 24, encF00(LS_MOVE, 0, REG_R12, REG_R15)); // JMPr R12
  write16le(buf + 26, NOP);
  write16le(buf + 28, NOP);
  write16le(buf + 30, NOP);
}

void Little64::writePlt(uint8_t *buf, const Symbol &sym,
                        uint64_t pltEntryAddr) const {
  // Per-symbol PLT entry (20 bytes): PC-relative GOT-indirect jump.
  //
  // Clobbers: R3, R12.
  //
  // offset 0:  JMP +4                               skip literal
  // offset 2:  .quad (gotPltEntry - (pltEntry+2))   displacement
  // offset 10: MOVE_PCREL R3, [pc-5]                R3 = literal addr
  // offset 12: LOAD R12, [R3+0]                     R12 = displacement
  // offset 14: ADD R12, R3                          R12 = gotPltEntry addr
  // offset 16: LOAD R12, [R12+0]                    R12 = target function
  // offset 18: JMPr R12                             jump

  uint64_t gotPltEntry = sym.getGotPltVA(ctx);
  int64_t disp =
      static_cast<int64_t>(gotPltEntry) - static_cast<int64_t>(pltEntryAddr + 2);

  write16le(buf + 0, encJMP(4));
  write64le(buf + 2, static_cast<uint64_t>(disp));
  write16le(buf + 10, encF01(LS_MOVE, -5, REG_R3));     // MOVE_PCREL R3,[pc-5]
  write16le(buf + 12, encF00(LS_LOAD, 0, REG_R3, REG_R12));
  write16le(buf + 14, encGP(GP_ADD, REG_R3, REG_R12));  // R12 += R3
  write16le(buf + 16, encF00(LS_LOAD, 0, REG_R12, REG_R12));
  write16le(buf + 18, encF00(LS_MOVE, 0, REG_R12, REG_R15)); // JMPr R12
}

void Little64::relocate(uint8_t *loc, const Relocation &rel,
                        uint64_t val) const {
  switch (rel.type) {
  case R_LITTLE64_NONE:
    break;
  case R_LITTLE64_ABS64:
  case R_LITTLE64_RELATIVE:
  case R_LITTLE64_GLOB_DAT:
  case R_LITTLE64_JUMP_SLOT:
  case R_LITTLE64_GOT64:
  case R_LITTLE64_TLS_DTPMOD64:
  case R_LITTLE64_TLS_DTPREL64:
  case R_LITTLE64_TLS_TPREL64:
  case R_LITTLE64_TLS_GOTTPREL:
    write64le(loc, val);
    break;
  case R_LITTLE64_COPY:
    // Copy relocations are handled by the dynamic linker.
    break;
  case R_LITTLE64_ABS32:
  case R_LITTLE64_PCREL32:
    write32le(loc, val);
    break;
  case R_LITTLE64_PCREL6: {
    int64_t Offset = static_cast<int64_t>(val) / 2;
    checkInt(ctx, loc, Offset, 6, rel);
    uint16_t Insn = read16le(loc);
    Insn = (Insn & 0xfc0f) | ((Offset & 0x3f) << 4);
    write16le(loc, Insn);
    break;
  }
  case R_LITTLE64_PCREL10: {
    int64_t Offset = static_cast<int64_t>(val) / 2;
    checkInt(ctx, loc, Offset, 10, rel);
    uint16_t Insn = read16le(loc);
    Insn = (Insn & 0xfc00) | (Offset & 0x3ff);
    write16le(loc, Insn);
    break;
  }
  case R_LITTLE64_PCREL13: {
    int64_t Offset = static_cast<int64_t>(val) / 2;
    checkInt(ctx, loc, Offset, 13, rel);
    uint16_t Insn = read16le(loc);
    Insn = (Insn & 0xe000) | (Offset & 0x1fff);
    write16le(loc, Insn);
    break;
  }
  default:
    llvm_unreachable("unknown relocation");
  }
}

// ---------------------------------------------------------------------------
// Custom relocation scanner: TLS IE via handleTlsIe, LE via checkTlsLe.
// ---------------------------------------------------------------------------
template <class ELFT, class RelTy>
void Little64::scanSectionImpl(InputSectionBase &sec, Relocs<RelTy> rels) {
  RelocScan rs(ctx, &sec);
  sec.relocations.reserve(rels.size());

  for (auto it = rels.begin(); it != rels.end(); ++it) {
    RelType type = it->getType(false);
    uint32_t symIndex = it->getSymbol(false);
    Symbol &sym = sec.getFile<ELFT>()->getSymbol(symIndex);
    uint64_t offset = it->r_offset;
    if (sym.isUndefined() && symIndex != 0 &&
        rs.maybeReportUndefined(cast<Undefined>(sym), offset))
      continue;
    int64_t addend = rs.getAddend<ELFT>(*it, type);
    RelExpr expr;

    switch (type) {
    case R_LITTLE64_NONE:
      continue;

    // TLS Initial Exec: GOT slot with TP-relative offset.
    case R_LITTLE64_TLS_GOTTPREL:
      rs.handleTlsIe<false>(R_GOT_PC, type, offset, addend, sym);
      continue;

    // TLS Local Exec: direct TP-relative offset.
    case R_LITTLE64_TLS_TPREL64:
      if (rs.checkTlsLe(offset, sym, type))
        continue;
      expr = R_TPREL;
      break;

    // PC-relative fast path.
    case R_LITTLE64_PCREL6:
    case R_LITTLE64_PCREL10:
    case R_LITTLE64_PCREL13:
    case R_LITTLE64_PCREL32:
      rs.processR_PC(type, offset, addend, sym);
      continue;

    // Everything else uses getRelExpr + generic process.
    default:
      expr = getRelExpr(type, sym, sec.content().data() + offset);
      if (expr == R_NONE)
        continue;
      break;
    }

    rs.process(expr, type, offset, sym, addend);
  }
}

void Little64::scanSection(InputSectionBase &sec) {
  elf::scanSection1<Little64, ELF64LE>(*this, sec);
}

void elf::setLittle64TargetInfo(Ctx &ctx) {
  ctx.target.reset(new Little64(ctx));
}
