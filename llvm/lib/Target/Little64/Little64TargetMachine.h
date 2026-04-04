//===-- Little64TargetMachine.h - Define TargetMachine for Little64 -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LITTLE64_LITTLE64TARGETMACHINE_H
#define LLVM_LIB_TARGET_LITTLE64_LITTLE64TARGETMACHINE_H

#include "Little64Subtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include <optional>

namespace llvm {

class Little64TargetMachine : public CodeGenTargetMachineImpl {
  Little64Subtarget Subtarget;
  std::unique_ptr<TargetLoweringObjectFile> TLOF;

public:
  Little64TargetMachine(const Target &TheTarget, const Triple &TargetTriple,
                         StringRef CPU, StringRef FeatureString,
                         const TargetOptions &Options,
                         std::optional<Reloc::Model> RM,
                         std::optional<CodeModel::Model> CM,
                         CodeGenOptLevel OptLevel, bool JIT);

  ~Little64TargetMachine() override;

  const Little64Subtarget *
  getSubtargetImpl(const Function & /*Fn*/) const override {
    return &Subtarget;
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LITTLE64_LITTLE64TARGETMACHINE_H
