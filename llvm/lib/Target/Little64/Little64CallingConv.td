//===- Little64CallingConv.td - Little64 Calling Convention -*-tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Calling convention for the Little-64 ISA.
//
// Arguments 0-4 → R10, R9, R8, R7, R6 (in that order)
// Arguments 5+  → stack (oldest pushed first, 8-byte aligned slots)
// Return value  → R1 (low 64 bits), R2 (high 64 bits for 128-bit pairs)
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Argument / return value calling conventions
//===----------------------------------------------------------------------===//

def CC_Little64 : CallingConv<[
  // Promote i1/i8/i16/i32 to i64 before assignment.
  CCIfType<[i1, i8, i16, i32], CCPromoteToType<i64>>,

  // Arguments 0-4 passed in R10, R9, R8, R7, R6.
  CCIfType<[i64], CCAssignToReg<[R10, R9, R8, R7, R6]>>,

  // Remaining arguments go on the stack, 8-byte aligned 8-byte slots.
  CCIfType<[i64], CCAssignToStack<8, 8>>
]>;

def RetCC_Little64 : CallingConv<[
  // Promote small integer return types.
  CCIfType<[i1, i8, i16, i32], CCPromoteToType<i64>>,

  // Single return value in R1; pair in R1 (lo) + R2 (hi).
  CCIfType<[i64], CCAssignToReg<[R1, R2]>>
]>;

//===----------------------------------------------------------------------===//
// Callee-saved registers
//
// R11 is the callee-saved GPR (often used as FP).
// R14 (LR) is callee-saved and preserved by normal PEI spill/restore paths.
//===----------------------------------------------------------------------===//
def CSR_Little64 : CalleeSavedRegs<(add R11, R14)>;

//===----------------------------------------------------------------------===//
// Vararg calling convention: all arguments on the stack.
//
// Vararg functions use an all-stack convention so that fixed and variadic
// arguments form a contiguous region in memory, making the void* va_list
// pointer arithmetic in va_arg correct without a separate register save area.
//===----------------------------------------------------------------------===//
def CC_Little64_VarArg : CallingConv<[
  CCIfType<[i1, i8, i16, i32], CCPromoteToType<i64>>,
  CCIfType<[i64], CCAssignToStack<8, 8>>
]>;
