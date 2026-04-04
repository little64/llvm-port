//===-- Little64InstPrinter.cpp - Convert Little64 MCInst to asm syntax ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64InstPrinter.h"
#include "MCTargetDesc/Little64MCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Include the auto-generated portion of the assembly writer.
#define PRINT_ALIAS_INSTR
#include "Little64GenAsmWriter.inc"

void Little64InstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << getRegisterName(Reg);
}

void Little64InstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                    StringRef Annot,
                                    const MCSubtargetInfo &STI,
                                    raw_ostream &O) {
  if (!printAliasInstr(MI, Address, O))
    printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void Little64InstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                       raw_ostream &O,
                                       const char *Modifier) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    O << getRegisterName(Op.getReg());
    return;
  }
  if (Op.isImm()) {
    O << Op.getImm();
    return;
  }
  assert(Op.isExpr() && "Unknown operand type");
  MAI.printExpr(O, *Op.getExpr());
}

void Little64InstPrinter::printLSRegOffset(const MCInst *MI, int OpNo,
                                           raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  assert(Op.isImm() && "Expected immediate lsreg offset");
  int64_t OffBytes = Op.getImm();
  if (OffBytes != 0)
    O << '+' << OffBytes;
}

void Little64InstPrinter::printMemOperand(const MCInst *MI, int OpNo,
                                          raw_ostream &O) {
  // Format: [Rs1+offset]
  const MCOperand &BaseReg = MI->getOperand(OpNo);
  const MCOperand &Offset  = MI->getOperand(OpNo + 1);
  O << '[' << getRegisterName(BaseReg.getReg());
  int64_t Off = Offset.getImm();
  if (Off != 0)
    O << '+' << Off;
  O << ']';
}

void Little64InstPrinter::printPCRelOperand(const MCInst *MI, uint64_t Address,
                                            int OpNo, raw_ostream &O) {
  // Format: @offset  (or @label via expression)
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm()) {
    int64_t Imm = Op.getImm();
    if (Imm >= 0)
      O << '+' << Imm;
    else
      O << Imm;
  } else {
    assert(Op.isExpr() && "Expected expression for PC-relative operand");
    MAI.printExpr(O, *Op.getExpr());
  }
}
