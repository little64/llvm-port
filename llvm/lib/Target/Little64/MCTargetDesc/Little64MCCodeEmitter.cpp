//===-- Little64MCCodeEmitter.cpp - Convert Little64 code to machine code -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Little64MCCodeEmitter class — converts MCInst
// objects to 16-bit little-endian binary instruction words.
//
//===----------------------------------------------------------------------===//

#include "Little64MCTargetDesc.h"
#include "Little64FixupKinds.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class Little64MCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;

public:
  Little64MCCodeEmitter(const MCInstrInfo & /*MCII*/, MCContext &Ctx)
      : Ctx(Ctx) {}

  ~Little64MCCodeEmitter() override = default;

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

  // TableGen-generated functions.
  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;
  unsigned getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;
  unsigned getPCRelImmOpValue(const MCInst &MI, unsigned OpNo,
                              SmallVectorImpl<MCFixup> &Fixups,
                              const MCSubtargetInfo &STI) const;
  unsigned getLSRegOffsetEncoding(const MCInst &MI, unsigned OpNo,
                                  SmallVectorImpl<MCFixup> &Fixups,
                                  const MCSubtargetInfo &STI) const;

  // Encode mem_ri:$addr as {OffField[1:0], Rs1[3:0]} (6 bits total).
  // OffField = byte_offset / 2 (so offset 0→0, 2→1, 4→2, 6→3).
  unsigned getMemRIEncoding(const MCInst &MI, unsigned OpNo,
                            SmallVectorImpl<MCFixup> &Fixups,
                            const MCSubtargetInfo &STI) const;
};

} // end anonymous namespace

void Little64MCCodeEmitter::encodeInstruction(
    const MCInst &MI, SmallVectorImpl<char> &CB,
    SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  uint64_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
  // All instructions are 16 bits, little-endian.
  support::endian::write<uint16_t>(CB, static_cast<uint16_t>(Bits),
                                   llvm::endianness::little);
}

unsigned Little64MCCodeEmitter::getMachineOpValue(
    const MCInst &MI, const MCOperand &MO,
    SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  llvm_unreachable("Unhandled expression operand in MC code emitter");
}

unsigned Little64MCCodeEmitter::getPCRelImmOpValue(
    const MCInst &MI, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());

  // Must be an expression — emit a fixup.
  // JUMP uses 13-bit offset, JUMP.* uses 10-bit offset; others use 6-bit offset.
  unsigned Opcode = MI.getOpcode();
  bool IsUnconditionalJump = (Opcode == Little64::JMP);
  bool IsJump = (Opcode == Little64::JUMP_Z || Opcode == Little64::JUMP_C ||
                 Opcode == Little64::JUMP_S || Opcode == Little64::JUMP_GT ||
                 Opcode == Little64::JUMP_LT);
  MCFixupKind Kind = IsUnconditionalJump
      ? static_cast<MCFixupKind>(Little64::FK_Little64_PCREL13)
      : (IsJump
             ? static_cast<MCFixupKind>(Little64::FK_Little64_PCREL10)
             : static_cast<MCFixupKind>(Little64::FK_Little64_PCREL6));
  // Offset of the fixup within the 16-bit instruction word.
  Fixups.push_back(MCFixup::create(0, MO.getExpr(), Kind, true));
  return 0;
}

unsigned Little64MCCodeEmitter::getMemRIEncoding(
    const MCInst &MI, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  // sub-operand 0: base register, sub-operand 1: byte offset (0, 2, 4, or 6).
  const MCOperand &Base = MI.getOperand(OpNo);
  const MCOperand &Off  = MI.getOperand(OpNo + 1);
  unsigned RegEnc  = Ctx.getRegisterInfo()->getEncodingValue(Base.getReg());
  unsigned OffField = 0;
  if (Off.isImm())
    OffField = static_cast<unsigned>(Off.getImm()) / 2; // byte→field
  // Return {OffField[1:0], RegEnc[3:0]} packed into 6 bits.
  return ((OffField & 0x3) << 4) | (RegEnc & 0xF);
}

unsigned Little64MCCodeEmitter::getLSRegOffsetEncoding(
    const MCInst &MI, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  const MCOperand &Off = MI.getOperand(OpNo);
  if (!Off.isImm())
    llvm_unreachable("lsreg offset must be an immediate");

  int64_t OffBytes = Off.getImm();
  if (OffBytes < 0 || OffBytes > 6 || (OffBytes & 1) != 0)
    llvm_unreachable("lsreg offset must be one of {0,2,4,6}");

  return static_cast<unsigned>(OffBytes / 2);
}

#include "Little64GenMCCodeEmitter.inc"

MCCodeEmitter *llvm::createLittle64MCCodeEmitter(const MCInstrInfo &MCII,
                                                  MCContext &Ctx) {
  return new Little64MCCodeEmitter(MCII, Ctx);
}
