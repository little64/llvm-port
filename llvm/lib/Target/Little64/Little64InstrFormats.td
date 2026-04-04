//===- Little64InstrFormats.td - Little64 Instruction Formats  -*-tablegen-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// All Little-64 instructions are 16 bits.  Bits [15:14] select the format:
//
//   00  LS Register     (Format00)
//   01  LS PC-Relative  (Format01)
//   10  Load Immediate  (FormatLDI)
//   110 GP ALU          (FormatGP)
//   111 Uncond JUMP     (FormatUJMP)
//
//===----------------------------------------------------------------------===//

// Base class for all Little-64 instructions.
class Little64Inst<dag outs, dag ins, string asmstr, list<dag> pat>
    : Instruction {
  bits<16> Inst;
  let Namespace   = "Little64";
  let Size        = 2;    // 16-bit instructions
  let OutOperandList = outs;
  let InOperandList  = ins;
  let AsmString      = asmstr;
  let Pattern        = pat;
}

//===----------------------------------------------------------------------===//
// Format 00 — LS Register
//   [15:14] = 00
//   [13:10] = OPCODE_LS (4 bits)
//   [9:8]   = OFFSET    (2 bits, byte-offset = field × 2, values 0-6)
//   [7:4]   = Rs1       (4 bits, base register)
//   [3:0]   = Rd        (4 bits, destination / source)
//===----------------------------------------------------------------------===//
class Format00<bits<4> op, dag outs, dag ins, string asmstr, list<dag> pat>
    : Little64Inst<outs, ins, asmstr, pat> {
  bits<4> Rd;
  bits<4> Rs1;
  bits<2> Offset;

  let Inst{15-14} = 0b00;
  let Inst{13-10} = op;
  let Inst{9-8}   = Offset;
  let Inst{7-4}   = Rs1;
  let Inst{3-0}   = Rd;
}

//===----------------------------------------------------------------------===//
// Format 01 — LS PC-Relative (non-JUMP variant)
//   [15:14] = 01
//   [13:10] = OPCODE_LS (4 bits)
//   [9:4]   = PC-REL    (6-bit signed, instruction-unit offset)
//   [3:0]   = Rd        (4 bits)
//===----------------------------------------------------------------------===//
class Format01<bits<4> op, dag outs, dag ins, string asmstr, list<dag> pat>
    : Little64Inst<outs, ins, asmstr, pat> {
  bits<4>  Rd;
  bits<6>  PCRel;

  let Inst{15-14} = 0b01;
  let Inst{13-10} = op;
  let Inst{9-4}   = PCRel;
  let Inst{3-0}   = Rd;
}

//===----------------------------------------------------------------------===//
// Format 01 JUMP — LS PC-Relative JUMP.* variant
//   [15:14] = 01
//   [13:10] = OPCODE_LS (4 bits, values 11-15)
//   [9:0]   = PC-REL    (10-bit signed, instruction-unit offset)
//   No Rd field; destination is always R15 (PC).
//===----------------------------------------------------------------------===//
class Format01Jump<bits<4> op, dag outs, dag ins, string asmstr, list<dag> pat>
    : Little64Inst<outs, ins, asmstr, pat> {
  bits<10> PCRel;

  let Inst{15-14} = 0b01;
  let Inst{13-10} = op;
  let Inst{9-0}   = PCRel;
}

//===----------------------------------------------------------------------===//
// Format 10 — Load Immediate (LDI)
//   [15:14] = 10
//   [13:12] = SHIFT     (2 bits, byte-slot 0-3)
//   [11:4]  = IMM8      (8-bit immediate)
//   [3:0]   = Rd        (4 bits)
//===----------------------------------------------------------------------===//
class FormatLDI<bits<2> shift, dag outs, dag ins, string asmstr, list<dag> pat>
    : Little64Inst<outs, ins, asmstr, pat> {
  bits<4> Rd;
  bits<8> Imm8;

  let Inst{15-14} = 0b10;
  let Inst{13-12} = shift;
  let Inst{11-4}  = Imm8;
  let Inst{3-0}   = Rd;
}

//===----------------------------------------------------------------------===//
// Format 110 — GP ALU
//   [15:13] = 110
//   [12:8]  = OPCODE_GP (5 bits)
//   [7:4]   = Rs1       (4 bits: register index or 4-bit immediate)
//   [3:0]   = Rd        (4 bits)
//===----------------------------------------------------------------------===//
class FormatGP<bits<5> op, dag outs, dag ins, string asmstr, list<dag> pat>
    : Little64Inst<outs, ins, asmstr, pat> {
  bits<4> Rd;
  bits<4> Rs1;

  let Inst{15-13} = 0b110;
  let Inst{12-8}  = op;
  let Inst{7-4}   = Rs1;
  let Inst{3-0}   = Rd;
}

//===----------------------------------------------------------------------===//
// Format 111 — Unconditional PC-Relative JUMP
//   [15:13] = 111
//   [12:0]  = PC-REL (13-bit signed, instruction-unit offset)
//===----------------------------------------------------------------------===//
class FormatUJMP<dag outs, dag ins, string asmstr, list<dag> pat>
    : Little64Inst<outs, ins, asmstr, pat> {
  bits<13> PCRel;

  let Inst{15-13} = 0b111;
  let Inst{12-0}  = PCRel;
}
