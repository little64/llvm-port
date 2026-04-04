//===-- Little64TargetMachine.cpp - Define TargetMachine for Little64 -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64TargetMachine.h"
#include "Little64.h"
#include "Little64ExpandPseudos.h"
#include "Little64MachineFunctionInfo.h"
#include "Little64TargetObjectFile.h"
#include "TargetInfo/Little64TargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Target/TargetOptions.h"
#include <optional>

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLittle64Target() {
  RegisterTargetMachine<Little64TargetMachine> X(getTheLittle64Target());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeLittle64CallDebugLocFixPass(PR);
  initializeLittle64ExpandPseudosPass(PR);
  initializeLittle64AsmPrinterPass(PR);
  initializeLittle64DAGToDAGISelLegacyPass(PR);
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

Little64TargetMachine::Little64TargetMachine(
    const Target &T, const Triple &TT, StringRef CPU, StringRef FS,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, TT.computeDataLayout(), TT, CPU, FS, Options,
                               getEffectiveRelocModel(RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      Subtarget(TT, CPU, FS, *this, Options,
                getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(new Little64TargetObjectFile()) {
  initAsmInfo();
}

Little64TargetMachine::~Little64TargetMachine() = default;

MachineFunctionInfo *Little64TargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return MachineFunctionInfo::create<Little64MachineFunctionInfo>(Allocator, F,
                                                                  STI);
}

namespace {
class Little64PassConfig : public TargetPassConfig {
public:
  Little64PassConfig(Little64TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  Little64TargetMachine &getLittle64TargetMachine() const {
    return getTM<Little64TargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  void addPreSched2() override;
  void addPreEmitPass() override;
};
} // namespace

TargetPassConfig *
Little64TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new Little64PassConfig(*this, PM);
}

void Little64PassConfig::addIRPasses() {
  addPass(createAtomicExpandLegacyPass());
  TargetPassConfig::addIRPasses();
}

bool Little64PassConfig::addInstSelector() {
  addPass(createLittle64ISelDag(getLittle64TargetMachine()));
  return false;
}

void Little64PassConfig::addPreSched2() {
  // Retag post-call same-line debug locations while CALL pseudos still exist.
  addPass(createLittle64CallDebugLocFixPass());
  // Expand control-flow pseudos after regalloc/scheduling (mature-backend
  // style) so semantics are fixed before final emission.
  addPass(createLittle64ExpandControlFlowPseudosPass());
}

void Little64PassConfig::addPreEmitPass() {
  // Expand address/literal materialization pseudos immediately before branch
  // distance relaxation so exact final sizes are visible.
  addPass(createLittle64ExpandAddressPseudosPass());
  // Relax out-of-range branches into literal-slot indirect jumps via R12.
  addPass(&BranchRelaxationPassID);
}
