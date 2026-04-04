//===-- Little64FrameLowering.h - Define frame lowering for Little64 ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LITTLE64_LITTLE64FRAMELOWERING_H
#define LLVM_LIB_TARGET_LITTLE64_LITTLE64FRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class Little64Subtarget;

class Little64FrameLowering : public TargetFrameLowering {
public:
  explicit Little64FrameLowering()
      : TargetFrameLowering(TargetFrameLowering::StackGrowsDown,
                            Align(8), // stack alignment: 8 bytes
                            0) {}     // local area offset

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  bool hasFPImpl(const MachineFunction &MF) const override;

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LITTLE64_LITTLE64FRAMELOWERING_H
