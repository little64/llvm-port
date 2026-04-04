//===-- Little64Disassembler.cpp - Little64 Disassembler ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64.h"
#include "MCTargetDesc/Little64MCTargetDesc.h"
#include "TargetInfo/Little64TargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::MCD;

#define DEBUG_TYPE "little64-disassembler"

using DecodeStatus = MCDisassembler::DecodeStatus;

#define Success MCDisassembler::Success
#define Fail MCDisassembler::Fail
#define SoftFail MCDisassembler::SoftFail

namespace {
class Little64Disassembler : public MCDisassembler {
public:
  Little64Disassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // end anonymous namespace

static MCDisassembler *createLittle64Disassembler(const Target &T,
                                                  const MCSubtargetInfo &STI,
                                                  MCContext &Ctx) {
  return new Little64Disassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLittle64Disassembler() {
  // Register the disassembler.
  TargetRegistry::RegisterMCDisassembler(getTheLittle64Target(),
                                         createLittle64Disassembler);
}

static const uint16_t GPRDecoderTable[] = {
  Little64::R0,  Little64::R1,  Little64::R2,  Little64::R3,
  Little64::R4,  Little64::R5,  Little64::R6,  Little64::R7,
  Little64::R8,  Little64::R9,  Little64::R10, Little64::R11,
  Little64::R12, Little64::R13, Little64::R14, Little64::R15
};

static DecodeStatus DecodeGPRRegisterClass(MCInst &Inst, unsigned RegNo,
                                           uint64_t Address,
                                           const MCDisassembler *Decoder) {
  if (RegNo > 15)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[RegNo]));
  return MCDisassembler::Success;
}

static DecodeStatus decodePCRel6(MCInst &Inst, unsigned Insn, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  int64_t Offset = SignExtend64<6>(Insn);
  Inst.addOperand(MCOperand::createImm(Offset));
  return MCDisassembler::Success;
}

static DecodeStatus decodePCRel10(MCInst &Inst, unsigned Insn, uint64_t Address,
                                  const MCDisassembler *Decoder) {
  int64_t Offset = SignExtend64<10>(Insn);
  Inst.addOperand(MCOperand::createImm(Offset));
  return MCDisassembler::Success;
}

static DecodeStatus decodePCRel13(MCInst &Inst, unsigned Insn, uint64_t Address,
                                  const MCDisassembler *Decoder) {
  int64_t Offset = SignExtend64<13>(Insn);
  Inst.addOperand(MCOperand::createImm(Offset));
  return MCDisassembler::Success;
}

static DecodeStatus decodeMemRI(MCInst &Inst, unsigned Insn, uint64_t Address,
                                const MCDisassembler *Decoder) {
  // Decoder tables may pass either:
  //  - packed mem operand bits: {OffField[1:0], Rs1[3:0]} (6 bits), or
  //  - the full 16-bit instruction word.
  // Handle both encodings so Rs1/Offset are decoded from the correct fields.
  unsigned RegNo;
  unsigned OffField;
  if (Insn <= 0x3f) {
    RegNo = Insn & 0xf;
    OffField = (Insn >> 4) & 0x3;
  } else {
    RegNo = (Insn >> 4) & 0xf;
    OffField = (Insn >> 8) & 0x3;
  }
  int64_t Offset = OffField * 2;

  if (DecodeGPRRegisterClass(Inst, RegNo, Address, Decoder) == MCDisassembler::Fail)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createImm(Offset));
  return MCDisassembler::Success;
}

static DecodeStatus decodeLSRegOffset(MCInst &Inst, unsigned Insn,
                                      uint64_t Address,
                                      const MCDisassembler *Decoder) {
  int64_t OffsetBytes = static_cast<int64_t>(Insn & 0x3) * 2;
  Inst.addOperand(MCOperand::createImm(OffsetBytes));
  return MCDisassembler::Success;
}

#include "Little64GenDisassemblerTables.inc"

DecodeStatus Little64Disassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                                  ArrayRef<uint8_t> Bytes,
                                                  uint64_t Address,
                                                  raw_ostream &CStream) const {
  if (Bytes.size() < 2) {
    Size = 0;
    return Fail;
  }

  uint32_t Insn = support::endian::read16le(Bytes.data());
  DecodeStatus Result =
      decodeInstruction(DecoderTable16, Instr, Insn, Address, this, STI);
  if (Result != Fail) {
    Size = 2;
    return Result;
  }

  Size = 0;
  return Fail;
}
