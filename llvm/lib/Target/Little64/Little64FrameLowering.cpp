//===-- Little64FrameLowering.cpp - Little64 Frame Lowering ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64FrameLowering.h"
#include "Little64InstrInfo.h"
#include "Little64RegisterInfo.h"
#include "Little64Subtarget.h"
#include "llvm/CodeGen/CFIInstBuilder.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

bool Little64FrameLowering::hasFPImpl(const MachineFunction &MF) const {
  // Use a frame pointer if the function has variable-size allocas or the
  // target frame option forces it.
  return MF.getFrameInfo().hasVarSizedObjects() ||
         MF.getTarget().Options.DisableFramePointerElim(MF);
}

void Little64FrameLowering::emitPrologue(MachineFunction &MF,
                                          MachineBasicBlock &MBB) const {
  if (MF.getFunction().hasFnAttribute(Attribute::Naked))
    return;

  MachineFrameInfo &MFI = MF.getFrameInfo();
  const Little64InstrInfo &TII =
      *static_cast<const Little64InstrInfo *>(MF.getSubtarget().getInstrInfo());
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  uint64_t StackSize = MFI.getStackSize();
  const bool HasFP = hasFP(MF);

  const bool EmitCFI = true;

  // 1. Save FP (R11) if used.
  // LR (R14) save/restore is CSR-driven via PEI.
  if (HasFP) {
    BuildMI(MBB, MBBI, DL, TII.get(Little64::PUSH), Little64::R13)
        .addReg(Little64::R11)
        .addReg(Little64::R13);

    if (EmitCFI) {
      CFIInstBuilder(MBB, MBBI, MachineInstr::FrameSetup)
          .buildDefCFAOffset(8);
      CFIInstBuilder(MBB, MBBI, MachineInstr::FrameSetup)
          .buildOffset(Little64::R11, -8);
    }

  }

  // 2. Adjust SP for local variables.
  if (StackSize != 0) {
    // Use R12 as scratch (permanently reserved, never live across this point).
    BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI), Little64::R12)
        .addImm(StackSize & 0xFF);
    if (StackSize > 0xFF) {
      BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI_S1), Little64::R12)
          .addImm((StackSize >> 8) & 0xFF)
          .addReg(Little64::R12);
    }
    if (StackSize > 0xFFFF) {
      BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI_S2), Little64::R12)
          .addImm((StackSize >> 16) & 0xFF)
          .addReg(Little64::R12);
    }
    if (StackSize > 0xFFFFFF) {
      BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI_S3), Little64::R12)
          .addImm((StackSize >> 24) & 0xFF)
          .addReg(Little64::R12);
    }
    BuildMI(MBB, MBBI, DL, TII.get(Little64::SUB), Little64::R13)
        .addReg(Little64::R12)
        .addReg(Little64::R13);

    if (EmitCFI && !HasFP)
      CFIInstBuilder(MBB, MBBI, MachineInstr::FrameSetup)
          .buildDefCFAOffset(StackSize);
  }

  // 3. Establish FP after local allocation so FP-based frame indices
  // refer to the current frame's local area.
  if (HasFP) {
    BuildMI(MBB, MBBI, DL, TII.get(Little64::MOVErr), Little64::R11)
        .addReg(Little64::R13)
        .addImm(0);

    if (EmitCFI) {
      const uint64_t SavedRegsSize = 8;
      CFIInstBuilder(MBB, MBBI, MachineInstr::FrameSetup)
          .buildDefCFA(Little64::R11, StackSize + SavedRegsSize);
    }
  }

}

void Little64FrameLowering::emitEpilogue(MachineFunction &MF,
                                          MachineBasicBlock &MBB) const {
  if (MF.getFunction().hasFnAttribute(Attribute::Naked))
    return;

  MachineFrameInfo &MFI = MF.getFrameInfo();
  const Little64InstrInfo &TII =
      *static_cast<const Little64InstrInfo *>(MF.getSubtarget().getInstrInfo());
  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  DebugLoc DL = MBBI->getDebugLoc();

  uint64_t StackSize = MFI.getStackSize();
  const bool HasFP = hasFP(MF);
  const bool EmitCFI = true;

  // 1. Restore SP.
  // With a frame pointer, the current SP may still be displaced by outgoing
  // call-frame adjustments. Rebase from FP, then skip over the local area so
  // the subsequent POP reads the saved FP slot rather than a local stack slot.
  if (HasFP) {
    BuildMI(MBB, MBBI, DL, TII.get(Little64::MOVErr), Little64::R13)
        .addReg(Little64::R11)
        .addImm(0);

    if (StackSize != 0) {
      BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI), Little64::R12)
          .addImm(StackSize & 0xFF);
      if (StackSize > 0xFF) {
        BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI_S1), Little64::R12)
            .addImm((StackSize >> 8) & 0xFF)
            .addReg(Little64::R12);
      }
      if (StackSize > 0xFFFF) {
        BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI_S2), Little64::R12)
            .addImm((StackSize >> 16) & 0xFF)
            .addReg(Little64::R12);
      }
      if (StackSize > 0xFFFFFF) {
        BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI_S3), Little64::R12)
            .addImm((StackSize >> 24) & 0xFF)
            .addReg(Little64::R12);
      }
      BuildMI(MBB, MBBI, DL, TII.get(Little64::ADD), Little64::R13)
          .addReg(Little64::R12)
          .addReg(Little64::R13);
    }

    if (EmitCFI)
      CFIInstBuilder(MBB, MBBI, MachineInstr::FrameDestroy)
          .buildDefCFA(Little64::R13, 8);
  } else if (StackSize != 0) {
    BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI), Little64::R12)
        .addImm(StackSize & 0xFF);
    if (StackSize > 0xFF) {
      BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI_S1), Little64::R12)
          .addImm((StackSize >> 8) & 0xFF)
          .addReg(Little64::R12);
    }
    if (StackSize > 0xFFFF) {
      BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI_S2), Little64::R12)
          .addImm((StackSize >> 16) & 0xFF)
          .addReg(Little64::R12);
    }
    if (StackSize > 0xFFFFFF) {
      BuildMI(MBB, MBBI, DL, TII.get(Little64::LDI_S3), Little64::R12)
          .addImm((StackSize >> 24) & 0xFF)
          .addReg(Little64::R12);
    }
    BuildMI(MBB, MBBI, DL, TII.get(Little64::ADD), Little64::R13)
        .addReg(Little64::R12)
        .addReg(Little64::R13);

    if (EmitCFI)
      CFIInstBuilder(MBB, MBBI, MachineInstr::FrameDestroy)
          .buildDefCFAOffset(0);
  }

  // 2. Restore FP
  if (HasFP) {
    BuildMI(MBB, MBBI, DL, TII.get(Little64::POP))
        .addReg(Little64::R11, RegState::Define)
        .addReg(Little64::R13, RegState::Define)
        .addReg(Little64::R13);

    if (EmitCFI) {
      CFIInstBuilder CFIBuilder(MBB, MBBI, MachineInstr::FrameDestroy);
      CFIBuilder.buildRestore(Little64::R11);
      CFIBuilder.buildDefCFA(Little64::R13, 0);
    }
  }

  // 3. LR (R14) restore is CSR-driven via PEI.
  if (EmitCFI && !HasFP && StackSize == 0) {
    CFIInstBuilder(MBB, MBBI, MachineInstr::FrameDestroy)
        .buildDefCFAOffset(0);
  }
}

MachineBasicBlock::iterator
Little64FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  const Little64InstrInfo &TII =
      *static_cast<const Little64InstrInfo *>(MF.getSubtarget().getInstrInfo());
  DebugLoc DL = I->getDebugLoc();

  const unsigned Opc = I->getOpcode();
  const int64_t Amount = I->getOperand(0).getImm();

  // Little64 stack slots are 8-byte aligned; adjust outgoing call frame by
  // materializing the byte count into R12 (reserved scratch) and using a
  // single SUB/ADD.
  if (Amount > 0) {
    const uint64_t UAmount = static_cast<uint64_t>(Amount);
    // Materialize the adjustment amount into R12 using LDI byte sequence.
    BuildMI(MBB, I, DL, TII.get(Little64::LDI), Little64::R12)
        .addImm(UAmount & 0xFF);
    if (UAmount > 0xFF) {
      BuildMI(MBB, I, DL, TII.get(Little64::LDI_S1), Little64::R12)
          .addImm((UAmount >> 8) & 0xFF)
          .addReg(Little64::R12);
    }
    if (UAmount > 0xFFFF) {
      BuildMI(MBB, I, DL, TII.get(Little64::LDI_S2), Little64::R12)
          .addImm((UAmount >> 16) & 0xFF)
          .addReg(Little64::R12);
    }
    if (UAmount > 0xFFFFFF) {
      BuildMI(MBB, I, DL, TII.get(Little64::LDI_S3), Little64::R12)
          .addImm((UAmount >> 24) & 0xFF)
          .addReg(Little64::R12);
    }
    if (Opc == Little64::ADJCALLSTACKDOWN) {
      BuildMI(MBB, I, DL, TII.get(Little64::SUB), Little64::R13)
          .addReg(Little64::R12)
          .addReg(Little64::R13);
    } else if (Opc == Little64::ADJCALLSTACKUP) {
      BuildMI(MBB, I, DL, TII.get(Little64::ADD), Little64::R13)
          .addReg(Little64::R12)
          .addReg(Little64::R13);
    }
  }

  return MBB.erase(I);
}
