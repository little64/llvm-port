//===- Little64RegisterInfo.td - Little64 Register Defs ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Declares the 16 registers of the Little-64 ISA (R0–R15).
//
//===----------------------------------------------------------------------===//

class Little64Reg<bits<4> enc, string name, list<string> altNames = []>
    : Register<name, altNames> {
  field bits<4> Enc = enc;
  let Namespace   = "Little64";
  let HWEncoding  = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, enc};
}

class Little64StatusReg<string name> : Register<name> {
  let Namespace = "Little64";
}

let Namespace = "Little64" in {
  // General-purpose registers.
  def R0  : Little64Reg<0,  "R0">,  DwarfRegNum<[0]>;   // always zero
  def R1  : Little64Reg<1,  "R1">,  DwarfRegNum<[1]>;   // caller-saved / ret lo
  def R2  : Little64Reg<2,  "R2">,  DwarfRegNum<[2]>;   // caller-saved / ret hi
  def R3  : Little64Reg<3,  "R3">,  DwarfRegNum<[3]>;   // caller-saved
  def R4  : Little64Reg<4,  "R4">,  DwarfRegNum<[4]>;   // caller-saved
  def R5  : Little64Reg<5,  "R5">,  DwarfRegNum<[5]>;   // caller-saved
  def R6  : Little64Reg<6,  "R6">,  DwarfRegNum<[6]>;   // caller-saved / arg4
  def R7  : Little64Reg<7,  "R7">,  DwarfRegNum<[7]>;   // caller-saved / arg3
  def R8  : Little64Reg<8,  "R8">,  DwarfRegNum<[8]>;   // caller-saved / arg2
  def R9  : Little64Reg<9,  "R9">,  DwarfRegNum<[9]>;   // caller-saved / arg1
  def R10 : Little64Reg<10, "R10">, DwarfRegNum<[10]>;  // caller-saved / arg0
  def R11 : Little64Reg<11, "R11">, DwarfRegNum<[11]>;  // callee-saved / frame pointer
  def R12 : Little64Reg<12, "R12">, DwarfRegNum<[12]>;  // reserved / backend scratch
  def R13 : Little64Reg<13, "R13", ["SP"]>, DwarfRegNum<[13]>;  // stack pointer
  def R14 : Little64Reg<14, "R14", ["LR"]>, DwarfRegNum<[14]>;  // link register
  def R15 : Little64Reg<15, "R15", ["PC"]>, DwarfRegNum<[15]>;  // program counter

  // Architectural condition flags (Z/C/S), modeled as a dedicated status reg
  // so machine scheduling can preserve compare-then-branch dependencies.
  def FLAGS : Little64StatusReg<"FLAGS">;
}

//===----------------------------------------------------------------------===//
// Register Classes
//===----------------------------------------------------------------------===//

// All 16 registers listed in allocation-preference order.
// R1-R5:   caller-saved, general use / return values
// R6-R10:  caller-saved, argument registers (not preferred for unrelated alloc)
// R11:     callee-saved, used as frame pointer when needed
// R12:     reserved as backend scratch (never allocated by regalloc)
// R0, R13, R14, R15: always reserved (zero / SP / LR / PC)
def GPR : RegisterClass<"Little64", [i64], 64,
    (add R1, R2, R3, R4, R5,           // caller-saved / return values
         R6, R7, R8, R9, R10,          // caller-saved / arg registers
         R11,                          // callee-saved / frame pointer
         R12,                          // reserved / backend scratch
         R0, R13, R14, R15)>;          // zero / SP / LR / PC  (reserved)

// Condition-code register class used for implicit def/use dependencies.
def CCR : RegisterClass<"Little64", [i64], 64, (add FLAGS)> {
  let CopyCost = -1;
  let isAllocatable = 0;
}
