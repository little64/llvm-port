//===-- Little64AsmBackend.cpp - Little64 Assembler Backend ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64MCTargetDesc.h"
#include "Little64FixupKinds.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"

using namespace llvm;

namespace {

class Little64AsmBackend : public MCAsmBackend {
  uint8_t OSABI;
public:
  Little64AsmBackend(uint8_t OSABI)
      : MCAsmBackend(llvm::endianness::little), OSABI(OSABI) {}

  ~Little64AsmBackend() override = default;

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override;

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override;

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    // NOP = LDI #0, R0 = 0b 10 00 00000000 0000 = 0x8000 (little-endian)
    if (Count % 2 != 0)
      return false;
    for (uint64_t I = 0; I < Count; I += 2) {
      OS.write('\x00');
      OS.write('\x80');
    }
    return true;
  }

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createLittle64ELFObjectWriter(OSABI);
  }
};

} // end anonymous namespace

MCFixupKindInfo
Little64AsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  // Name, bit-offset within instruction word, bit-width, flags.
  static const MCFixupKindInfo Infos[Little64::NumTargetFixupKinds] = {
      // FK_Little64_PCREL6:  bits[9:4], 6 bits, PC-relative non-jump
      {"FK_Little64_PCREL6",  4, 6, 0},
      // FK_Little64_PCREL10: bits[9:0], 10 bits, PC-relative JUMP.*
      {"FK_Little64_PCREL10", 0, 10, 0},
      // FK_Little64_PCREL13: bits[12:0], 13 bits, PC-relative JUMP
      {"FK_Little64_PCREL13", 0, 13, 0},
      // FK_Little64_GOT64: 64-bit GOT-relative offset (data slot)
      {"FK_Little64_GOT64", 0, 64, 0},
      // FK_Little64_TLS_TPREL64: 64-bit TLS TP offset (data slot)
      {"FK_Little64_TLS_TPREL64", 0, 64, 0},
  };

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  unsigned Idx = Kind - FirstTargetFixupKind;
  assert(Idx < Little64::NumTargetFixupKinds && "Invalid fixup kind");
  return Infos[Idx];
}

void Little64AsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                                    const MCValue &Target, uint8_t *Data,
                                    uint64_t Value, bool IsResolved) {
  maybeAddReloc(F, Fixup, Target, Value, IsResolved);

  MCFixupKind Kind = Fixup.getKind();

  // The Data pointer provided here is already offset by Fixup.getOffset().
  if (Kind == FK_Data_1 || Kind == FK_Data_2 || Kind == FK_Data_4 ||
      Kind == FK_Data_8 ||
      Kind == (MCFixupKind)Little64::FK_Little64_GOT64 ||
      Kind == (MCFixupKind)Little64::FK_Little64_TLS_TPREL64) {
    unsigned Size;
    if (Kind == (MCFixupKind)Little64::FK_Little64_GOT64 ||
        Kind == (MCFixupKind)Little64::FK_Little64_TLS_TPREL64)
      Size = 8;
    else
      Size = 1 << (Kind - FK_Data_1);
    for (unsigned I = 0; I < Size; ++I)
      Data[I] = (Value >> (8 * I)) & 0xFF;
    return;
  }

  // PC-relative fixups: offset is relative to the next instruction (PC+2).
  // Value is (target - fixup_offset) in bytes. Convert to instruction units and
  // adjust for PC pointing to the next instruction.
  int64_t InstrOffset = (static_cast<int64_t>(Value) - 2) / 2;

  const MCFixupKindInfo &Info = getFixupKindInfo(Kind);
  unsigned BitOffset = Info.TargetOffset;
  unsigned BitWidth  = Info.TargetSize;

  // Range checking: only if resolved.
  if (IsResolved) {
    int64_t MaxOffset = (1LL << (BitWidth - 1)) - 1;
    int64_t MinOffset = -(1LL << (BitWidth - 1));
    if (InstrOffset > MaxOffset || InstrOffset < MinOffset) {
      getContext().reportError(Fixup.getLoc(), "PC-relative offset out of range");
      return;
    }
  }

  if (mc::isRelocation(Kind))
    return;

  uint64_t Mask = ((1ULL << BitWidth) - 1) << BitOffset;

  uint16_t Instr = (static_cast<uint8_t>(Data[0]) |
                    (static_cast<uint8_t>(Data[1]) << 8));

  // Mask InstrOffset to the correct bit width to convert signed to unsigned
  // two's-complement representation (before shifting to avoid undefined behavior).
  uint64_t MaskedOffset = static_cast<uint64_t>(InstrOffset) & ((1ULL << BitWidth) - 1);
  Instr = (Instr & ~static_cast<uint16_t>(Mask)) |
          static_cast<uint16_t>((MaskedOffset << BitOffset) & Mask);

  Data[0] = Instr & 0xFF;
  Data[1] = (Instr >> 8) & 0xFF;
}

MCAsmBackend *llvm::createLittle64AsmBackend(const Target &T,
                                              const MCSubtargetInfo &STI,
                                              const MCRegisterInfo &MRI,
                                              const MCTargetOptions &Options) {
  const llvm::Triple &TT = STI.getTargetTriple();
  uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(TT.getOS());
  return new Little64AsmBackend(OSABI);
}
