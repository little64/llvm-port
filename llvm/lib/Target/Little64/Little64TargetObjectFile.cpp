//===-- Little64TargetObjectFile.cpp - Little64 Object File Info ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64TargetObjectFile.h"
#include "Little64TargetMachine.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;

void Little64TargetObjectFile::Initialize(MCContext &Ctx,
                                           const TargetMachine &TM) {
  TargetLoweringObjectFileELF::Initialize(Ctx, TM);
}
