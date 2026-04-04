//===-- Little64RegisterInfo.cpp - Little64 Register Information ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64RegisterInfo.h"
#include "Little64.h"
#include "Little64Subtarget.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/IR/Function.h"

#define GET_REGINFO_TARGET_DESC
#include "Little64GenRegisterInfo.inc"

using namespace llvm;

Little64RegisterInfo::Little64RegisterInfo()
    : Little64GenRegisterInfo(Little64::R14 /*return address register*/,
                               0, 0, Little64::R15 /*PC*/) {}

const MCPhysReg *
Little64RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_Little64_SaveList;
}

const uint32_t *
Little64RegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                            CallingConv::ID) const {
  return CSR_Little64_RegMask;
}

BitVector
Little64RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  // R0  — always zero, writes discarded
  markSuperRegs(Reserved, Little64::R0);
  // R13 — stack pointer
  markSuperRegs(Reserved, Little64::R13);
  // R14 — link register
  markSuperRegs(Reserved, Little64::R14);
  // R15 — program counter
  markSuperRegs(Reserved, Little64::R15);
  // FLAGS — architectural condition register (Z/C/S)
  markSuperRegs(Reserved, Little64::FLAGS);
  // R12 — backend scratch register (frame/call lowering helpers)
  markSuperRegs(Reserved, Little64::R12);
  // R11 — frame pointer (when a frame pointer is used)
  if (MF.getSubtarget().getFrameLowering()->hasFP(MF))
    markSuperRegs(Reserved, Little64::R11);
  return Reserved;
}

bool Little64RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                                int SPAdj, unsigned FIOperandNum,
                                                RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  Register BaseReg;
  int64_t Offset =
      TFI->getFrameIndexReference(MF, FrameIndex, BaseReg).getFixed();
  Offset += MI.getOperand(FIOperandNum + 1).getImm();

  // In FP mode, FP is established after pushing the old FP value.
  // Incoming stack arguments are modeled as fixed objects at non-negative
  // offsets from the entry SP and therefore need to skip that saved-FP slot.
  // Without this bias, fixed incoming args can alias saved R11 at -O0,
  // which breaks vararg loop bounds/count loads.
  MachineFrameInfo &MFI = MF.getFrameInfo();
  if (TFI->hasFP(MF) && MFI.isFixedObjectIndex(FrameIndex) &&
      MFI.getObjectOffset(FrameIndex) >= 0)
    Offset += 8;

  // For non-fixed frame objects, FP is established after local stack
  // allocation, so references already target the current frame.

  // Little-64 LS-REG instructions only allow offsets 0, 2, 4, 6 (field × 2).
  // If the offset fits in {0, 2, 4, 6}, encode directly.
  if (Offset >= 0 && Offset <= 6 && (Offset % 2) == 0) {
    MI.getOperand(FIOperandNum).ChangeToRegister(BaseReg, /*isDef=*/false);
    MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
    return false;
  }

  // Otherwise, synthesise the full address in a scratch register.
  // R12 is permanently reserved as the backend scratch register, so we can
  // always use it here without going through the scavenger. This avoids the
  // circular-dependency problem where the scavenger would itself need a
  // scratch register to address its own spill slot in large frames.
  Register ScratchReg = Little64::R12;

  // Emit: ScratchReg = BaseReg + Offset  (using LDI + ADD sequence)
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();

  // Load offset into scratch via LDI sequence (up to 32-bit offset for now).
  // For simplicity, assume offset fits in 32 bits.
  BuildMI(*MI.getParent(), II, DL, TII.get(Little64::LDI), ScratchReg)
      .addImm(Offset & 0xFF);
  if (Offset > 0xFF || Offset < 0) {
    BuildMI(*MI.getParent(), II, DL, TII.get(Little64::LDI_S1), ScratchReg)
        .addImm((Offset >> 8) & 0xFF)
        .addReg(ScratchReg);
  }
  if ((Offset >> 16) & 0xFF) {
    BuildMI(*MI.getParent(), II, DL, TII.get(Little64::LDI_S2), ScratchReg)
        .addImm((Offset >> 16) & 0xFF)
        .addReg(ScratchReg);
  }
  if ((Offset >> 24) & 0xFF) {
    BuildMI(*MI.getParent(), II, DL, TII.get(Little64::LDI_S3), ScratchReg)
        .addImm((Offset >> 24) & 0xFF)
        .addReg(ScratchReg);
  }
  // ScratchReg = ScratchReg + BaseReg
  BuildMI(*MI.getParent(), II, DL, TII.get(Little64::ADD), ScratchReg)
      .addReg(BaseReg)
      .addReg(ScratchReg);

  // Now use ScratchReg as the base with offset 0.
  MI.getOperand(FIOperandNum).ChangeToRegister(ScratchReg, /*isDef=*/false,
                                               /*isImp=*/false,
                                               /*isKill=*/true);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(0);
  return false;
}

Register
Little64RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
  return TFI->hasFP(MF) ? Little64::R11 : Little64::R13;
}
