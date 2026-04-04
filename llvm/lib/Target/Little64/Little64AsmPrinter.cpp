//===-- Little64AsmPrinter.cpp - Little64 LLVM Assembly Printer -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64.h"
#include "Little64MCInstLower.h"
#include "Little64TargetMachine.h"
#include "MCTargetDesc/Little64InstPrinter.h"
#include "MCTargetDesc/Little64MCTargetDesc.h"
#include "TargetInfo/Little64TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

#define DEBUG_TYPE "asm-printer"

using namespace llvm;

namespace {
class Little64AsmPrinter : public AsmPrinter {
public:
  explicit Little64AsmPrinter(TargetMachine &TM,
                               std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  StringRef getPassName() const override {
    return "Little64 Assembly Printer";
  }

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override;

  void emitInstruction(const MachineInstr *MI) override;

public:
  static char ID;
};
} // end anonymous namespace

bool Little64AsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                         const char *ExtraCode,
                                         raw_ostream &OS) {
  if (ExtraCode && ExtraCode[0])
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS);

  const MachineOperand &MO = MI->getOperand(OpNo);
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    OS << Little64InstPrinter::getRegisterName(MO.getReg());
    return false;
  case MachineOperand::MO_Immediate:
    OS << MO.getImm();
    return false;
  default:
    break;
  }

  return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS);
}

void Little64AsmPrinter::emitInstruction(const MachineInstr *MI) {
  Little64_MC::verifyInstructionPredicates(MI->getOpcode(),
                                           getSubtargetInfo().getFeatureBits());

  // Handle pseudos inline.
  switch (MI->getOpcode()) {
  case Little64::CALL:
    llvm_unreachable("CALL should have been expanded by Little64ExpandPseudos");

  case Little64::RET:
    llvm_unreachable("RET should have been expanded by Little64ExpandPseudos");

  case Little64::LDI64: {
    // LDI64 should have been expanded before assembly printing.
    llvm_unreachable("LDI64 with immediate should have been expanded by ExpandPseudos pass");
    return;
  }

  case Little64::LDI64_RELOC_DATA: {
    // Emit the 8-byte literal slot. For symbol references, emit zeros and
    // manually create a relocation via the MCStreamer.
    const MachineOperand &ValueOp = MI->getOperand(0);

    if (ValueOp.isImm()) {
      // Immediate: emit directly
      OutStreamer->emitIntValue(ValueOp.getImm(), 8);
      return;
    }

    // Symbol reference: we need to create a relocation.
    // The trick is to emit the data via a method that the MCStreamer
    // recognizes as needing a relocation.
    const MCExpr *ValueExpr = nullptr;
    if (ValueOp.isGlobal()) {
      MCSymbol *Sym = getSymbol(ValueOp.getGlobal());
      ValueExpr = MCSymbolRefExpr::create(Sym, OutContext);
      if (ValueOp.getOffset())
        ValueExpr = MCBinaryExpr::createAdd(
            ValueExpr, MCConstantExpr::create(ValueOp.getOffset(), OutContext),
            OutContext);
    } else if (ValueOp.isSymbol()) {
      MCSymbol *Sym = GetExternalSymbolSymbol(ValueOp.getSymbolName());
      ValueExpr = MCSymbolRefExpr::create(Sym, OutContext);
    } else if (ValueOp.isBlockAddress()) {
      MCSymbol *Sym = GetBlockAddressSymbol(ValueOp.getBlockAddress());
      ValueExpr = MCSymbolRefExpr::create(Sym, OutContext);
    } else if (ValueOp.isMBB()) {
      // Used by insertIndirectBranch for branch relaxation trampolines.
      MCSymbol *Sym = ValueOp.getMBB()->getSymbol();
      ValueExpr = MCSymbolRefExpr::create(Sym, OutContext);
    } else if (ValueOp.isJTI()) {
      // Jump table: emit the address of the jump table.
      MCSymbol *Sym = GetJTISymbol(ValueOp.getIndex());
      ValueExpr = MCSymbolRefExpr::create(Sym, OutContext);
    } else {
      errs() << "LDI64_RELOC_DATA: operand type " << ValueOp.getType() << "\n";
      llvm_unreachable("LDI64_RELOC_DATA: unsupported operand type");
    }

    // Emit 8 bytes with the symbol expression.
    // Use a workaround: emit as assembly directives that generate relocations.
    // Apply the @got or @tprel specifier when the operand carries the flag.
    if (ValueOp.getTargetFlags() == Little64II::MO_GOT)
      ValueExpr = MCSpecifierExpr::create(ValueExpr, ELF::R_LITTLE64_GOT64,
                                          OutContext);
    else if (ValueOp.getTargetFlags() == Little64II::MO_TPREL)
      ValueExpr = MCSpecifierExpr::create(ValueExpr, ELF::R_LITTLE64_TLS_TPREL64,
                                          OutContext);
    OutStreamer->emitValue(ValueExpr, 8);
    return;
  }

  default:
    break;
  }

  // Regular instruction: lower MachineInstr → MCInst and emit.
  Little64MCInstLower MCInstLowering(OutContext, *this);
  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  OutStreamer->emitInstruction(TmpInst, getSubtargetInfo());
}

char Little64AsmPrinter::ID = 0;

INITIALIZE_PASS(Little64AsmPrinter, "little64-asm-printer",
                "Little64 Assembly Printer", false, false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLittle64AsmPrinter() {
  RegisterAsmPrinter<Little64AsmPrinter> X(getTheLittle64Target());
}
