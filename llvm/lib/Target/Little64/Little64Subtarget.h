//===-- Little64Subtarget.h - Define Subtarget for the Little64 --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LITTLE64_LITTLE64SUBTARGET_H
#define LLVM_LIB_TARGET_LITTLE64_LITTLE64SUBTARGET_H

#include "Little64FrameLowering.h"
#include "Little64ISelLowering.h"
#include "Little64InstrInfo.h"
#include "Little64SelectionDAGInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/CodeGen/LibcallLoweringInfo.h"
#include "llvm/Target/TargetMachine.h"

#define GET_SUBTARGETINFO_HEADER
#include "Little64GenSubtargetInfo.inc"

namespace llvm {

class Little64Subtarget : public Little64GenSubtargetInfo {
public:
  Little64Subtarget(const Triple &TargetTriple, StringRef Cpu,
                    StringRef FeatureString, const TargetMachine &TM,
                    const TargetOptions &Options, CodeModel::Model CodeModel,
                    CodeGenOptLevel OptLevel);

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  Little64Subtarget &initializeSubtargetDependencies(StringRef CPU,
                                                      StringRef FS);

  const Little64InstrInfo *getInstrInfo() const override { return &InstrInfo; }

  const TargetFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }

  const Little64RegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }

  const Little64TargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }

  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }

  void initLibcallLoweringInfo(LibcallLoweringInfo &Info) const override;

private:
  Little64InstrInfo          InstrInfo;
  Little64FrameLowering      FrameLowering;
  Little64TargetLowering     TLInfo;
  Little64SelectionDAGInfo   TSInfo;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LITTLE64_LITTLE64SUBTARGET_H
