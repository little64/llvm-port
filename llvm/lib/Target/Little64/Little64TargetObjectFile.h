//===-- Little64TargetObjectFile.h - Little64 Object File Info --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LITTLE64_LITTLE64TARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_LITTLE64_LITTLE64TARGETOBJECTFILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {

class Little64TargetObjectFile : public TargetLoweringObjectFileELF {
public:
  void Initialize(MCContext &Ctx, const TargetMachine &TM) override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LITTLE64_LITTLE64TARGETOBJECTFILE_H
