//===-- Little64CallDebugLocFix.cpp - Retag post-call debug locations -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

using namespace llvm;

#define DEBUG_TYPE "little64-call-debugloc-fix"

namespace {
class Little64CallDebugLocFix : public MachineFunctionPass {
public:
  static char ID;

  Little64CallDebugLocFix() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Little64 Call DebugLoc Fix";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;

    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        if (!MI.isCall()) {
          continue;
        }

        const DebugLoc CallDL = MI.getDebugLoc();
        if (!CallDL || CallDL.getLine() == 0) {
          continue;
        }

        SmallVector<MachineInstr *, 8> SameLineAfterCall;

        for (MachineBasicBlock::iterator It = std::next(MI.getIterator());
             It != MBB.end(); ++It) {
          MachineInstr &NextMI = *It;
          if (NextMI.isMetaInstruction()) {
            continue;
          }

          const DebugLoc NextDL = NextMI.getDebugLoc();
          if (!NextDL || NextDL.getLine() == 0) {
            SameLineAfterCall.push_back(&NextMI);
            continue;
          }

          if (NextDL.getLine() == CallDL.getLine()) {
            SameLineAfterCall.push_back(&NextMI);
            continue;
          }

          for (MachineInstr *FixMI : SameLineAfterCall) {
            FixMI->setDebugLoc(NextDL);
            Changed = true;
          }
          break;
        }
      }
    }

    return Changed;
  }
};
} // namespace

char Little64CallDebugLocFix::ID = 0;

INITIALIZE_PASS(Little64CallDebugLocFix, DEBUG_TYPE,
                "Retag post-call same-line instructions", false, false)

FunctionPass *llvm::createLittle64CallDebugLocFixPass() {
  return new Little64CallDebugLocFix();
}
