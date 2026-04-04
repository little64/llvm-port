//===-- Little64InstrInfo.cpp - Little64 Instruction Information ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64InstrInfo.h"
#include "Little64.h"
#include "Little64RegisterInfo.h"
#include "Little64Subtarget.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "Little64GenInstrInfo.inc"

using namespace llvm;

Little64InstrInfo::Little64InstrInfo(const Little64Subtarget &STI)
    : Little64GenInstrInfo(STI, RI, Little64::ADJCALLSTACKDOWN,
                           Little64::ADJCALLSTACKUP),
      RI() {}

void Little64InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I,
                                     const DebugLoc &DL, Register DestReg,
                                     Register SrcReg, bool KillSrc,
                                     bool RenamableDest,
                                     bool RenamableSrc) const {
  // MOVE DestReg = SrcReg  (encoded as MOVErr with offset=0)
  BuildMI(MBB, I, DL, get(Little64::MOVErr), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addImm(0);
}

void Little64InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register SrcReg,
    bool IsKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBB.findDebugLoc(I);

  BuildMI(MBB, I, DL, get(Little64::STORE))
      .addReg(SrcReg, getKillRegState(IsKill))
      .addFrameIndex(FrameIndex)
      .addImm(0);
}

void Little64InstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    unsigned SubReg, MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBB.findDebugLoc(I);

  BuildMI(MBB, I, DL, get(Little64::LOAD), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0);
}

Register Little64InstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                                 int &FrameIndex) const {
  // LOAD Rd, [FI + 0]  — the canonical spill-reload pattern.
  if (MI.getOpcode() == Little64::LOAD &&
      MI.getOperand(1).isFI() && MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }
  return Register();
}

Register Little64InstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                                int &FrameIndex) const {
  // STORE [FI + 0], Rs  — the canonical spill pattern.
  if (MI.getOpcode() == Little64::STORE &&
      MI.getOperand(1).isFI() && MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }
  return Register();
}

bool Little64InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                       MachineBasicBlock *&TBB,
                                       MachineBasicBlock *&FBB,
                                       SmallVectorImpl<MachineOperand> &Cond,
                                       bool AllowModify) const {
  TBB = nullptr;
  FBB = nullptr;

  auto IsCondBrOpc = [](unsigned Opc) {
    return Opc == Little64::JUMP_Z  || Opc == Little64::JUMP_C  ||
           Opc == Little64::JUMP_S  || Opc == Little64::JUMP_GT ||
           Opc == Little64::JUMP_LT;
  };

  // Scan from the end; collect at most 3 terminators.
  MachineBasicBlock::iterator I = MBB.end();
  if (I == MBB.begin())
    return false;
  --I;

  // Helper: advance I backward past debug/non-terminator instructions.
  // Returns false if no terminator is found, or if the first terminator found
  // is a JMP with an immediate operand (a literal-slot skip from LDI64
  // expansion — not a real control-flow branch).
  auto prevTerminator = [&]() -> bool {
    if (I == MBB.begin()) return false;
    --I;
    while (!isUnpredicatedTerminator(*I)) {
      if (I == MBB.begin()) return false;
      --I;
    }
    if (!isUnpredicatedTerminator(*I))
      return false;
    // A JMP with an immediate operand is the "skip" in a literal-slot
    // LDI64 expansion (JMP @+4 / data / LOAD_PCREL), not a branch.
    if (I->getOpcode() == Little64::JMP && I->getOperand(0).isImm())
      return false;
    return true;
  };

  if (!isUnpredicatedTerminator(*I))
    return false;

  // T0 = last terminator.
  MachineInstr *T0 = &*I;

  // T1 = second-to-last terminator (if any).
  MachineInstr *T1 = nullptr;
  if (prevTerminator())
    T1 = &*I;

  // T2 = third-to-last terminator (if any).
  MachineInstr *T2 = nullptr;
  if (T1 && prevTerminator())
    T2 = &*I;

  // Pattern: T2=COND, T1=JMP, T0=JMP  →  T0 is dead code after T1 (unconditional).
  // With AllowModify, delete the dead T0 and treat it as COND+JMP.
  if (T2 && T1 && T0 &&
      IsCondBrOpc(T2->getOpcode()) &&
      T1->getOpcode() == Little64::JMP &&
      T0->getOpcode() == Little64::JMP) {
    if (AllowModify)
      T0->eraseFromParent(); // Remove dead trailing JMP.
    TBB = T2->getOperand(0).getMBB();
    FBB = T1->getOperand(0).getMBB();
    Cond.push_back(MachineOperand::CreateImm(T2->getOpcode()));
    return false;
  }

  // Pattern: T1=COND, T0=JMP  →  two-branch conditional.
  if (T1 && !T2 &&
      IsCondBrOpc(T1->getOpcode()) &&
      T0->getOpcode() == Little64::JMP) {
    TBB = T1->getOperand(0).getMBB();
    FBB = T0->getOperand(0).getMBB();
    Cond.push_back(MachineOperand::CreateImm(T1->getOpcode()));
    return false;
  }

  // Pattern: T0=JMP only (unconditional).
  if (!T1 && T0->getOpcode() == Little64::JMP) {
    TBB = T0->getOperand(0).getMBB();
    return false;
  }

  // Pattern: T0=COND only (conditional, fallthrough implied).
  if (!T1 && IsCondBrOpc(T0->getOpcode())) {
    TBB = T0->getOperand(0).getMBB();
    Cond.push_back(MachineOperand::CreateImm(T0->getOpcode()));
    return false;
  }

  // Cannot analyze.
  return true;
}

unsigned Little64InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                          int *BytesRemoved) const {
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;

  while (I != MBB.begin()) {
    --I;
    if (!I->isBranch())
      break;
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }

  if (BytesRemoved)
    *BytesRemoved = Count * 2;
  return Count;
}

unsigned Little64InstrInfo::insertBranch(MachineBasicBlock &MBB,
                                          MachineBasicBlock *TBB,
                                          MachineBasicBlock *FBB,
                                          ArrayRef<MachineOperand> Cond,
                                          const DebugLoc &DL,
                                          int *BytesAdded) const {
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert(Cond.size() <= 1 && "Little64 branch conditions have at most 1 item");

  if (Cond.empty()) {
    BuildMI(&MBB, DL, get(Little64::JMP)).addMBB(TBB);
    if (BytesAdded) *BytesAdded = 2;
    return 1;
  }

  unsigned CondOpc = (unsigned)Cond[0].getImm();
  BuildMI(&MBB, DL, get(CondOpc)).addMBB(TBB);
  if (FBB) {
    BuildMI(&MBB, DL, get(Little64::JMP)).addMBB(FBB);
    if (BytesAdded) *BytesAdded = 4;
    return 2;
  }
  if (BytesAdded) *BytesAdded = 2;
  return 1;
}

unsigned Little64InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  // NOTE: Little64Inst base class sets `let Size = 2`, so getDesc().getSize()
  // returns 2 for ALL instructions including pseudos, unless explicitly
  // overridden with a different `let Size = ...`. We must check known pseudos
  // FIRST, before consulting getDesc().getSize().
  switch (MI.getOpcode()) {
  case Little64::LDI64_RELOC_DATA:
    // `let Size = 8` in the .td file; 8-byte literal slot.
    return 8;
  case Little64::CALL:
    // Register target:      MOVErr(2)+MOVErr(2) = 4 bytes.
    // Global/symbol target: JMP@+4(2)+data(8)+LOAD_PCREL(2)+MOVErr(2)+MOVErr(2) = 16 bytes.
    // Use worst-case (global target) so BranchRelaxation never underestimates.
    if (MI.getNumOperands() > 0 && MI.getOperand(0).isReg())
      return 4;
    return 16;
  case Little64::RET:
    // Expands to one MOVErr instruction.
    return 2;
  case Little64::LDI64:
    // Should be gone after ExpandPseudos, but estimate worst-case.
    return 12; // JMP(2) + LDI64_RELOC_DATA(8) + LOAD_PCREL(2)
  case Little64::LDI64_GOT:
    // GOT-indirect literal slot: JMP(2) + data(8) + MOVE_PCREL(2) + LOAD(2)
    // + ADD(2) + LOAD(2) = 18 bytes.
    return 18;
  case Little64::ReadTP:
    // LDI(2) + LDI.S1(2) + LSR(2) = 6 bytes.
    return 6;
  case Little64::BRCC:
    // Expands to JUMP.cond(2) + optional JMP(2) in pre-emit.
    return 4;
  case Little64::SETCC:
    // LDI64(12) + LDI64(12) + TEST(2) + CMOV(2) = 28 bytes worst-case.
    return 28;
  case Little64::SETCC_SELECT:
    // TEST(2) + CMOV(2) = 4 bytes.
    return 4;
  default:
    // Meta-instructions (DBG_VALUE, KILL, etc.) and other unexpanded pseudos
    // emit no bytes.
    if (MI.isMetaInstruction())
      return 0;

    // Inline assembly emits real instructions that isPseudo() would hide.
    // Use the standard helper which respects MaxInstLength (2 bytes for
    // Little64), skips comments and blank lines, and handles .space.
    if (MI.isInlineAsm()) {
      const MachineFunction *MF = MI.getParent()->getParent();
      return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                                *MF->getTarget().getMCAsmInfo());
    }

    // Check for frame-index operands that might be expanded into up to 5
    // instructions (up to 4 LDI + 1 ADD + original) during PEI.
    for (const MachineOperand &MO : MI.operands()) {
      if (MO.isFI())
        return 12;
    }

    if (MI.isPseudo())
      return 0;

    // All real Little64 instructions are 16-bit = 2 bytes.
    return 2;
  }
}

bool Little64InstrInfo::isBranchOffsetInRange(unsigned BranchOpc,
                                               int64_t BrOffset) const {
  // BrOffset is (target_offset - branch_instruction_offset) in bytes.
  // The hardware encodes the field as (target - (branch_PC + 2)) / 2, because
  // PC is advanced by 2 before dispatch. The assembler applies the same
  // formula: InstrOffset = (BrOffset - 2) / 2.  We must match that exactly so
  // BranchRelaxation inserts trampolines for every branch the assembler rejects.
  // Given 2-byte alignment, (BrOffset - 2) must be even.
  if (((BrOffset - 2) & 1) != 0)
    return false;
  int64_t InstrOffset = (BrOffset - 2) / 2;
  switch (BranchOpc) {
  default:
    llvm_unreachable("Unknown branch opcode");
  case Little64::JMP:
    return isIntN(13, InstrOffset);
  case Little64::JUMP_Z:
  case Little64::JUMP_C:
  case Little64::JUMP_S:
  case Little64::JUMP_GT:
  case Little64::JUMP_LT:
    return isIntN(10, InstrOffset);
  }
}

MachineBasicBlock *
Little64InstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  default:
    llvm_unreachable("Unknown branch opcode");
  case Little64::JMPr:
    // Indirect branch — no static destination.
    return nullptr;
  case Little64::JMP:
    // JMP with an immediate operand is a literal-slot skip (LDI64 expansion),
    // not a real control-flow branch to a basic block.
    if (MI.getOperand(0).isImm())
      return nullptr;
    return MI.getOperand(0).getMBB();
  case Little64::JUMP_Z:
  case Little64::JUMP_C:
  case Little64::JUMP_S:
  case Little64::JUMP_GT:
  case Little64::JUMP_LT:
    return MI.getOperand(0).getMBB();
  }
}

void Little64InstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                              MachineBasicBlock &NewDestBB,
                                              MachineBasicBlock &RestoreBB,
                                              const DebugLoc &DL,
                                              int64_t BrOffset,
                                              RegScavenger *RS) const {
  // Emit a literal-slot indirect jump sequence into R12 (always-reserved
  // scratch), then jump via JMPr. R12 is never live here so no save/restore
  // is needed; RestoreBB is left empty and will be erased by the caller.
  //
  //   JMP @+4               ; skip over the 8-byte address literal  (2 bytes)
  //   LDI64_RELOC_DATA dest ; 8-byte address of NewDestBB           (8 bytes)
  //   LOAD_PCREL R12, @-5   ; load dest address into R12            (2 bytes)
  //   JMPr R12              ; jump to dest                          (2 bytes)
  //                                                         total: 14 bytes
  MachineFunction &MF = *MBB.getParent();
  (void)MF;

  BuildMI(&MBB, DL, get(Little64::JMP)).addImm(4);
  BuildMI(&MBB, DL, get(Little64::LDI64_RELOC_DATA)).addMBB(&NewDestBB);
  BuildMI(&MBB, DL, get(Little64::LOAD_PCREL), Little64::R12).addImm(-5);
  BuildMI(&MBB, DL, get(Little64::JMPr))
      .addReg(Little64::R12, RegState::Kill);
}

bool Little64InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid Little64 branch condition");
  // We have no inverted conditional branch instructions (no JUMP.NZ, etc.),
  // so condition reversal is not possible. Return true to signal failure;
  // the caller will synthesise the inverse using an unconditional jump.
  // TODO: add inverted conditional branch instructions.
  return true;
}
