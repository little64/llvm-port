//===- Little64Schedule.td - Little64 Scheduling Definitions -*-tablegen--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

def Little64SchedModel : SchedMachineModel {
  let LoadLatency      = 1;
  let MicroOpBufferSize = 0;
  let CompleteModel    = 0;
  let IssueWidth       = 1;
}
