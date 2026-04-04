//===-- Little64MachineFunctionInfo.h - Little64 Per-MF Info ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LITTLE64_LITTLE64MACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_LITTLE64_LITTLE64MACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class Little64MachineFunctionInfo : public MachineFunctionInfo {
  // Index of the stack object holding the saved LR (link register).
  int LRSpillFI = 0;
  bool LRSpilled = false;

  // Frame index of the first vararg argument (for VASTART lowering).
  // Only valid when the function is vararg.
  int VarArgsFrameIndex = 0;

public:
  Little64MachineFunctionInfo() = default;
  explicit Little64MachineFunctionInfo(const Function &F,
                                        const TargetSubtargetInfo *STI) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override;

  void setLRSpillFI(int FI) { LRSpillFI = FI; LRSpilled = true; }
  int  getLRSpillFI() const  { return LRSpillFI; }
  bool isLRSpilled() const   { return LRSpilled; }

  void setVarArgsFrameIndex(int FI) { VarArgsFrameIndex = FI; }
  int  getVarArgsFrameIndex() const  { return VarArgsFrameIndex; }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LITTLE64_LITTLE64MACHINEFUNCTIONINFO_H
