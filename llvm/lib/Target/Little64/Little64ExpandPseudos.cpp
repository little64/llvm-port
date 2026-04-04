//===-- Little64ExpandPseudos.cpp - Expand Little64 Pseudo Instructions ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass expands Little64 pseudo-instructions into real machine
// instructions before final emission.
//
// LDI64 expansion:
//   - Sign-extendable 32-bit values: expand to LDI / LDI.S1 / LDI.S2 / LDI.S3
//   - Other 64-bit values and symbol-like operands: expand to a literal slot
//     wrapped by JMP @+4 / LDI64_RELOC_DATA / LOAD_PCREL
//
// CALL expansion:
//   - Register target: MOVErr R15,R14,+2 ; MOVErr target,R15,+0
//   - Non-register target: literal-slot materialization into R12, then
//     MOVErr R15,R14,+2 ; MOVErr R12,R15,+0
//
// RET expansion:
//   - JMPr R14
//
// BRCC expansion:
//   - JUMP.cond true_target
//   - optional JMP false_target (when false target is not layout fallthrough)
//
// SETCC_SELECT expansion:
//   - TEST rhs, lhs
//   - CMOV.* tval, fval
//
//===----------------------------------------------------------------------===//

#include "Little64.h"
#include "Little64ExpandPseudos.h"
#include "Little64InstrInfo.h"
#include "Little64Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "little64-expand-pseudos"

class Little64ExpandPseudos : public MachineFunctionPass {
public:
  static char ID;

  enum class ExpansionMode {
    All,
    ControlFlowOnly,
    AddressOnly,
  };

  explicit Little64ExpandPseudos(ExpansionMode Mode = ExpansionMode::All)
      : MachineFunctionPass(ID), Mode(Mode) {}

  StringRef getPassName() const override {
    switch (Mode) {
    case ExpansionMode::All:
      return "Little64 Pseudo Instruction Expander";
    case ExpansionMode::ControlFlowOnly:
      return "Little64 Control-Flow Pseudo Expander";
    case ExpansionMode::AddressOnly:
      return "Little64 Address Pseudo Expander";
    }
    llvm_unreachable("Unknown Little64 pseudo expansion mode");
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  const Little64InstrInfo *TII = nullptr;
  ExpansionMode Mode;

  bool shouldExpandControlFlow() const {
    return Mode == ExpansionMode::All || Mode == ExpansionMode::ControlFlowOnly;
  }

  bool shouldExpandAddress() const {
    return Mode == ExpansionMode::All || Mode == ExpansionMode::AddressOnly;
  }

  bool expandLDI64(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandLDI64_GOT(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandReadTP(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandCALL(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandRET(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandBRCC(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandSETCCSelect(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
};

char Little64ExpandPseudos::ID = 0;

INITIALIZE_PASS(Little64ExpandPseudos, DEBUG_TYPE,
                "Expand Little64 pseudo-instructions", false, false)

namespace {

struct SetccSelectPlan {
  unsigned CMovOpc = 0;
  Register TESTLhs;
  Register TESTRhs;
  Register CMovSrc;
  Register CMovFalse;
};

static SetccSelectPlan buildSetccSelectPlan(Little64CC::CondCode CC,
                                            Register LHS, Register RHS,
                                            Register TVal, Register FVal) {
  SetccSelectPlan Plan;
  Plan.TESTLhs = LHS;
  Plan.TESTRhs = RHS;
  bool Invert = false;

  switch (CC) {
  case Little64CC::EQ:
    Plan.CMovOpc = Little64::CMOV_Z;
    break;
  case Little64CC::NE:
    Plan.CMovOpc = Little64::CMOV_Z;
    Invert = true;
    break;
  case Little64CC::ULT:
    Plan.CMovOpc = Little64::CMOV_C;
    break;
  case Little64CC::UGE:
    Plan.CMovOpc = Little64::CMOV_C;
    Invert = true;
    break;
  case Little64CC::UGT:
    Plan.CMovOpc = Little64::CMOV_C;
    std::swap(Plan.TESTLhs, Plan.TESTRhs);
    break;
  case Little64CC::ULE:
    Plan.CMovOpc = Little64::CMOV_C;
    Invert = true;
    std::swap(Plan.TESTLhs, Plan.TESTRhs);
    break;
  case Little64CC::LT:
  case Little64CC::GE:
  case Little64CC::GT:
  case Little64CC::LE:
    llvm_unreachable("Signed condition codes should have been lowered to "
                     "unsigned via XOR sign-bit bias before expansion");
  }

  Plan.CMovSrc = Invert ? FVal : TVal;
  Plan.CMovFalse = Invert ? TVal : FVal;
  return Plan;
}

} // namespace

bool Little64ExpandPseudos::expandSETCCSelect(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  Register Dst = MI.getOperand(0).getReg();
  int64_t CC = MI.getOperand(1).getImm();
  Register LHS = MI.getOperand(2).getReg();
  Register RHS = MI.getOperand(3).getReg();
  Register TVal = MI.getOperand(4).getReg();
  Register FVal = MI.getOperand(5).getReg();

  const SetccSelectPlan Chosen =
      buildSetccSelectPlan(static_cast<Little64CC::CondCode>(CC), LHS, RHS,
                           TVal, FVal);

  BuildMI(MBB, MBBI, DL, TII->get(Little64::TEST))
      .addReg(Chosen.TESTRhs)
      .addReg(Chosen.TESTLhs);

  // CMOV is a two-address instruction: if the condition is false the
  // destination register value is preserved. Because this pass runs late
  // (after two-address lowering/regalloc), explicitly materialize the false
  // value into Dst when needed.
  if (Dst != Chosen.CMovFalse) {
    if (Dst == Chosen.CMovSrc) {
      report_fatal_error(
          "SETCC_SELECT expansion requires dst to be tied with false value");
    }
    BuildMI(MBB, MBBI, DL, TII->get(Little64::MOVErr), Dst)
        .addReg(Chosen.CMovFalse)
        .addImm(0);
  }

  BuildMI(MBB, MBBI, DL, TII->get(Chosen.CMovOpc), Dst)
      .addReg(Chosen.CMovSrc)
      .addReg(Dst);

  return true;
}

bool Little64ExpandPseudos::expandLDI64(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();
  Register Rd = MI.getOperand(0).getReg();
  const MachineOperand &ValueOp = MI.getOperand(1);

  if (ValueOp.isImm()) {
    int64_t Imm = ValueOp.getImm();

    // Only the sign-extended 32-bit subset can be built with the byte-wise
    // LDI sequence.
    if (Imm == static_cast<int64_t>(static_cast<int32_t>(Imm))) {
      uint64_t UImm = static_cast<uint64_t>(Imm);
      uint8_t B0 = (UImm >> 0) & 0xFF;
      uint8_t B1 = (UImm >> 8) & 0xFF;
      uint8_t B2 = (UImm >> 16) & 0xFF;
      uint8_t B3 = (UImm >> 24) & 0xFF;

      BuildMI(MBB, MBBI, DL, TII->get(Little64::LDI), Rd).addImm(B0);
      if (B1 || B2 || B3) {
        BuildMI(MBB, MBBI, DL, TII->get(Little64::LDI_S1), Rd)
            .addImm(B1)
            .addReg(Rd);
      }
      if (B2 || B3) {
        BuildMI(MBB, MBBI, DL, TII->get(Little64::LDI_S2), Rd)
            .addImm(B2)
            .addReg(Rd);
      }
      if (B3) {
        BuildMI(MBB, MBBI, DL, TII->get(Little64::LDI_S3), Rd)
            .addImm(B3)
            .addReg(Rd);
      }

      return true;
    }

    // Larger immediates use the same literal-slot sequence as symbols.
  }

  // Literal slot sequence: skip over the 8-byte payload, emit the payload,
  // then load it back with a PC-relative load.
  BuildMI(MBB, MBBI, DL, TII->get(Little64::JMP)).addImm(4);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::LDI64_RELOC_DATA)).add(ValueOp);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::LOAD_PCREL), Rd).addImm(-5);

  return true;
}

bool Little64ExpandPseudos::expandLDI64_GOT(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();
  Register Rd = MI.getOperand(0).getReg();
  const MachineOperand &SymOp = MI.getOperand(1);

  assert(Rd != Little64::R12 &&
         "LDI64_GOT: destination must not be R12 (scratch register)");

  // Literal-slot GOT-indirect sequence (18 bytes):
  //   JMP +4                   ; skip literal slot      (2B)
  //   .quad sym@GOT            ; R_LITTLE64_GOT64       (8B)
  //   MOVE_PCREL R12, [pc-5]   ; R12 = literal addr     (2B)
  //   LOAD Rd, [R12+0]         ; Rd = displacement      (2B)
  //   ADD R12, Rd              ; Rd += R12 = GOT VA     (2B)
  //   LOAD Rd, [Rd+0]          ; Rd = *GOT[sym]         (2B)
  BuildMI(MBB, MBBI, DL, TII->get(Little64::JMP)).addImm(4);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::LDI64_RELOC_DATA)).add(SymOp);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::MOVE_PCREL), Little64::R12)
      .addImm(-5);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::LOAD), Rd)
      .addReg(Little64::R12)
      .addImm(0);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::ADD), Rd)
      .addReg(Little64::R12)
      .addReg(Rd);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::LOAD), Rd)
      .addReg(Rd)
      .addImm(0);

  return true;
}

bool Little64ExpandPseudos::expandReadTP(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();
  Register Rd = MI.getOperand(0).getReg();

  // Read the thread pointer from special register 0x8000 (6 bytes):
  //   LDI R12, #0        ; low byte  = 0x00    (2B)
  //   LDI.S1 R12, #0x80  ; byte 1    = 0x80    (2B)
  //   LSR R12, Rd         ; Rd = TP             (2B)
  BuildMI(MBB, MBBI, DL, TII->get(Little64::LDI), Little64::R12).addImm(0);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::LDI_S1), Little64::R12)
      .addImm(0x80)
      .addReg(Little64::R12);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::LSR), Rd)
      .addReg(Little64::R12);

  return true;
}

bool Little64ExpandPseudos::expandCALL(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();
  const MachineOperand &TargetOp = MI.getOperand(0);

  if (TargetOp.isReg()) {
    BuildMI(MBB, MBBI, DL, TII->get(Little64::MOVErr), Little64::R14)
        .addReg(Little64::R15)
        .addImm(2);
  auto CallMIB = BuildMI(MBB, MBBI, DL, TII->get(Little64::MOVErr), Little64::R15)
        .addReg(TargetOp.getReg(), getKillRegState(TargetOp.isKill()))
        .addImm(0);
  CallMIB.copyImplicitOps(MI);
    return true;
  }

  // Fallback for symbol-like and immediate operands: materialize through a
  // literal slot into R12, then branch indirectly like the register form.
  BuildMI(MBB, MBBI, DL, TII->get(Little64::JMP)).addImm(4);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::LDI64_RELOC_DATA)).add(TargetOp);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::LOAD_PCREL), Little64::R12)
      .addImm(-5);
  BuildMI(MBB, MBBI, DL, TII->get(Little64::MOVErr), Little64::R14)
      .addReg(Little64::R15)
      .addImm(2);
    auto CallMIB = BuildMI(MBB, MBBI, DL, TII->get(Little64::MOVErr), Little64::R15)
      .addReg(Little64::R12)
      .addImm(0);
    CallMIB.copyImplicitOps(MI);
  return true;
}

bool Little64ExpandPseudos::expandRET(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  BuildMI(MBB, MBBI, DL, TII->get(Little64::JMPr)).addReg(Little64::R14);
  return true;
}

bool Little64ExpandPseudos::expandBRCC(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  // BRCC now carries explicit compare operands. Emit TEST immediately before
  // branch emission; no compare-token recovery or late instruction motion is
  // required.
  if (MI.getNumOperands() > 3 && MI.getOperand(2).isReg() &&
      MI.getOperand(3).isReg()) {
    const MachineOperand &LHS = MI.getOperand(2);
    const MachineOperand &RHS = MI.getOperand(3);
    BuildMI(MBB, MBBI, DL, TII->get(Little64::TEST))
        .addReg(RHS.getReg(), getKillRegState(RHS.isKill()))
        .addReg(LHS.getReg(), getKillRegState(LHS.isKill()));
  }

  MachineBasicBlock *TrueMBB = MI.getOperand(0).getMBB();
  Little64CC::CondCode CC =
      static_cast<Little64CC::CondCode>(MI.getOperand(1).getImm());

  MachineBasicBlock *FalseMBB = nullptr;
  for (MachineBasicBlock *Succ : MBB.successors()) {
    if (Succ != TrueMBB) {
      FalseMBB = Succ;
      break;
    }
  }

  // Some blocks reach BRCC with incomplete successor metadata (for example,
  // only the taken edge recorded). Recover a usable false path from existing
  // trailing branch terminators or layout fallthrough.
  if (!FalseMBB) {
    for (auto It = std::next(MBBI), End = MBB.end(); It != End; ++It) {
      if (!It->isBranch())
        continue;
      MachineBasicBlock *Dest = nullptr;
      if (It->getOpcode() == Little64::JMP && It->getNumOperands() > 0 &&
          It->getOperand(0).isMBB()) {
        Dest = It->getOperand(0).getMBB();
      } else if (It->isConditionalBranch() && It->getNumOperands() > 0 &&
                 It->getOperand(0).isMBB()) {
        Dest = It->getOperand(0).getMBB();
      }
      if (Dest && Dest != TrueMBB) {
        FalseMBB = Dest;
        break;
      }
    }
  }
  if (!FalseMBB) {
    if (MachineBasicBlock *LayoutSucc = MBB.getNextNode())
      if (LayoutSucc != TrueMBB)
        FalseMBB = LayoutSucc;
  }
  if (!FalseMBB)
    FalseMBB = TrueMBB;

  unsigned JumpOpc;
  MachineBasicBlock *BranchTarget = nullptr;
  MachineBasicBlock *FallthroughTarget = nullptr;

  switch (CC) {
  case Little64CC::EQ:
    JumpOpc = Little64::JUMP_Z;
    BranchTarget = TrueMBB;
    FallthroughTarget = FalseMBB;
    break;
  case Little64CC::NE:
    JumpOpc = Little64::JUMP_Z;
    BranchTarget = FalseMBB;
    FallthroughTarget = TrueMBB;
    break;
  case Little64CC::ULT:
    JumpOpc = Little64::JUMP_C;
    BranchTarget = TrueMBB;
    FallthroughTarget = FalseMBB;
    break;
  case Little64CC::UGE:
    JumpOpc = Little64::JUMP_C;
    BranchTarget = FalseMBB;
    FallthroughTarget = TrueMBB;
    break;
  case Little64CC::UGT:
  case Little64CC::ULE:
    llvm_unreachable("BRCC: UGT/ULE should have been canonicalized before BRCC");
  case Little64CC::LT:
  case Little64CC::GE:
  case Little64CC::GT:
  case Little64CC::LE:
    llvm_unreachable("Signed condition codes should have been lowered to "
                     "unsigned via XOR sign-bit bias before expansion");
  }

  // BRCC is the canonical conditional terminator for this block. Older DAG
  // shapes may still leave trailing branch terminators after BRCC; keep the
  // branch sequence deterministic to avoid CFG/terminator mismatches during
  // BranchRelaxation.
  for (auto It = std::next(MBBI); It != MBB.end();) {
    if (It->isBranch()) {
      It = It->eraseFromParent();
      continue;
    }
    ++It;
  }

  BuildMI(MBB, MBBI, DL, TII->get(JumpOpc)).addMBB(BranchTarget);

  // Only emit the unconditional branch if the fallthrough target is not the
  // layout successor, avoiding a redundant JMP.
  MachineBasicBlock *LayoutSucc = MBB.getNextNode();
  if (FallthroughTarget != LayoutSucc)
    BuildMI(MBB, MBBI, DL, TII->get(Little64::JMP)).addMBB(FallthroughTarget);

  return true;
}

bool Little64ExpandPseudos::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const Little64InstrInfo *>(
      MF.getSubtarget().getInstrInfo());

  bool Changed = false;

  for (auto &MBB : MF) {
    for (auto MBBI = MBB.begin(); MBBI != MBB.end();) {
      MachineInstr &MI = *MBBI;
      bool Expanded = false;

      switch (MI.getOpcode()) {
      case Little64::BRCC:
        if (shouldExpandControlFlow())
          Expanded = expandBRCC(MBB, MBBI);
        break;
      case Little64::SETCC_SELECT:
        if (shouldExpandControlFlow())
          Expanded = expandSETCCSelect(MBB, MBBI);
        break;
      case Little64::LDI64:
        if (shouldExpandAddress())
          Expanded = expandLDI64(MBB, MBBI);
        break;
      case Little64::LDI64_GOT:
        if (shouldExpandAddress())
          Expanded = expandLDI64_GOT(MBB, MBBI);
        break;
      case Little64::ReadTP:
        if (shouldExpandAddress())
          Expanded = expandReadTP(MBB, MBBI);
        break;
      case Little64::CALL:
        if (shouldExpandControlFlow())
          Expanded = expandCALL(MBB, MBBI);
        break;
      case Little64::RET:
        if (shouldExpandControlFlow())
          Expanded = expandRET(MBB, MBBI);
        break;
      default:
        break;
      }

      if (Expanded) {
        auto NextMBBI = std::next(MBBI);
        MBB.erase(MBBI);
        Changed = true;
        MBBI = NextMBBI;
      } else {
        ++MBBI;
      }
    }
  }

  return Changed;
}

FunctionPass *llvm::createLittle64ExpandPseudosPass() {
  return new Little64ExpandPseudos(Little64ExpandPseudos::ExpansionMode::All);
}

FunctionPass *llvm::createLittle64ExpandControlFlowPseudosPass() {
  return new Little64ExpandPseudos(
      Little64ExpandPseudos::ExpansionMode::ControlFlowOnly);
}

FunctionPass *llvm::createLittle64ExpandAddressPseudosPass() {
  return new Little64ExpandPseudos(
      Little64ExpandPseudos::ExpansionMode::AddressOnly);
}
