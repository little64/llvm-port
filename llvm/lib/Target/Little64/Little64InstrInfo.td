//===- Little64InstrInfo.td - Little64 Instruction Definitions -*-tablegen-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Operand definitions
//===----------------------------------------------------------------------===//

// 8-bit immediate (LDI).
def uimm8 : Operand<i64> {
  let PrintMethod = "printOperand";
}

// Generic branch target (basic-block reference, OtherVT).
// Used by the BRCC pseudo instruction operand list.
def brtarget : Operand<OtherVT>;

// 2-bit offset field (×2 bytes → 0, 2, 4, 6).
def lsreg_offset : Operand<i64> {
  let PrintMethod   = "printLSRegOffset";
  let EncoderMethod = "getLSRegOffsetEncoding";
  let DecoderMethod = "decodeLSRegOffset";
}

// 6-bit signed PC-relative offset (instruction units).
def pcrel6 : Operand<OtherVT> {
  let PrintMethod   = "printPCRelOperand";
  let EncoderMethod = "getPCRelImmOpValue";
  let OperandType   = "OPERAND_PCREL";
  let DecoderMethod = "decodePCRel6";
}

// 10-bit signed PC-relative offset (instruction units) for JUMP.*.
def pcrel10 : Operand<OtherVT> {
  let PrintMethod   = "printPCRelOperand";
  let EncoderMethod = "getPCRelImmOpValue";
  let OperandType   = "OPERAND_PCREL";
  let DecoderMethod = "decodePCRel10";
}

// 13-bit signed PC-relative offset (instruction units) for unconditional JUMP.
def pcrel13 : Operand<OtherVT> {
  let PrintMethod   = "printPCRelOperand";
  let EncoderMethod = "getPCRelImmOpValue";
  let OperandType   = "OPERAND_PCREL";
  let DecoderMethod = "decodePCRel13";
}

def Little64MemOperand : AsmOperandClass {
  let Name = "Mem";
  let RenderMethod = "addMemOperands";
}

// Register-indirect memory operand [Rs1 + offset].
// Sub-operand names must match Format00 field names (Rs1, Offset) so
// tablegen can slice the EncoderMethod result into the right bit fields.
def mem_ri : Operand<i64> {
  let PrintMethod     = "printMemOperand";
  let MIOperandInfo   = (ops GPR:$Rs1, i64imm:$Offset);
  let EncoderMethod   = "getMemRIEncoding";
  let DecoderMethod   = "decodeMemRI";
  let ParserMatchClass = Little64MemOperand;
}

//===----------------------------------------------------------------------===//
// Selection DAG node types (custom)
//===----------------------------------------------------------------------===//

// Call-sequence type profiles (needed for ADJCALLSTACKDOWN/UP pseudos).
// Little-64 is a pure 64-bit target so amounts are i64.
def SDT_CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i64>, SDTCisVT<1, i64>]>;
def SDT_CallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i64>, SDTCisVT<1, i64>]>;

def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_CallSeqStart,
                           [SDNPHasChain, SDNPOutGlue]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_CallSeqEnd,
                           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

// Wrapper: wraps a TargetGlobalAddress/ExternalSym for materialization.
def SDT_Little64Wrapper : SDTypeProfile<1, 1,
    [SDTCisSameAs<0, 1>, SDTCisPtrTy<0>]>;
def Little64Wrapper : SDNode<"Little64ISD::Wrapper", SDT_Little64Wrapper, []>;
def Little64WrapperGOT : SDNode<"Little64ISD::WrapperGOT", SDT_Little64Wrapper, []>;

// ReadTP: reads the thread pointer from the special register bank.
def SDT_Little64ReadTP : SDTypeProfile<1, 0, [SDTCisPtrTy<0>]>;
def Little64ReadTP : SDNode<"Little64ISD::ReadTP", SDT_Little64ReadTP, []>;

def SDT_Little64Call : SDTypeProfile<0, -1, [SDTCisVT<0, iPTR>]>;
def Little64Call     : SDNode<"Little64ISD::CALL", SDT_Little64Call,
                              [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                               SDNPVariadic]>;
def Little64Ret      : SDNode<"Little64ISD::RET_FLAG",
                              SDTypeProfile<0, -1, []>,
                              [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

// Conditional branch node: (chain, dest_bb, cc_imm, lhs, rhs).
def SDT_Little64BRCC : SDTypeProfile<0, 4, [
  SDTCisVT<0, OtherVT>,
  SDTCisVT<1, i64>,
  SDTCisInt<2>,
  SDTCisSameAs<2, 3>
]>;
def Little64BRCC : SDNode<"Little64ISD::BRCC", SDT_Little64BRCC,
                          [SDNPHasChain]>;

// Integer setcc value node: (cc_imm, lhs, rhs) -> i64 {0,1}.
def SDT_Little64SETCC : SDTypeProfile<1, 3, [
  SDTCisVT<0, i64>,      // result value type
  SDTCisVT<1, i64>,      // condition code immediate
  SDTCisInt<2>,          // lhs register
  SDTCisSameAs<2, 3>     // rhs same integer type as lhs
]>;
def Little64SETCC : SDNode<"Little64ISD::SETCC", SDT_Little64SETCC, []>;

// Integer select node: (cc_imm, lhs, rhs, true_val, false_val) -> i64
// Selects between true_val and false_val based on condition (lhs, rhs, cc).
def SDT_Little64SETCC_SELECT : SDTypeProfile<1, 5, [
  SDTCisVT<0, i64>,      // result value type
  SDTCisVT<1, i64>,      // condition code immediate
  SDTCisInt<2>,          // lhs register (comparison operand)
  SDTCisSameAs<2, 3>,    // rhs same integer type as lhs
  SDTCisVT<4, i64>,      // true value
  SDTCisVT<5, i64>       // false value
]>;
def Little64SETCC_SELECT : SDNode<"Little64ISD::SETCC_SELECT", SDT_Little64SETCC_SELECT, []>;

// Complex pattern for [Rs1 + small_offset] addressing (offset in {0,2,4,6}).
def ADDRri : ComplexPattern<iPTR, 2, "SelectAddrRI", [frameindex]>;

// 4-bit immediate for GP immediate-shift instructions (encodable range 0..15).
def shamt4 : PatLeaf<(imm), [{
  return (uint64_t)N->getZExtValue() < 16;
}]>;

//===----------------------------------------------------------------------===//
// Format 00 — LS Register instructions
//===----------------------------------------------------------------------===//

// 64-bit load: Rd = MEM64[Rs1 + Offset*2]
def LOAD : Format00<0, (outs GPR:$Rd), (ins mem_ri:$addr),
    "LOAD\t$addr, $Rd",
    [(set GPR:$Rd, (load ADDRri:$addr))]> {
  bits<6> addr;
  let Rs1    = addr{3-0};  // base register (bits [3:0] of EncoderMethod result)
  let Offset = addr{5-4};  // offset field  (bits [5:4] of EncoderMethod result)
}

// 64-bit store: MEM64[Rs1 + Offset*2] = Rd
def STORE : Format00<1, (outs), (ins GPR:$Rd, mem_ri:$addr),
    "STORE\t$addr, $Rd",
    [(store GPR:$Rd, ADDRri:$addr)]> {
  bits<6> addr;
  let Rs1    = addr{3-0};
  let Offset = addr{5-4};
}

// PUSH Rs1: SP -= 8; MEM64[SP] = Rs1.
let mayStore = 1 in
def PUSH : Format00<2, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$Rd_in),
    "PUSH\t$Rs1",
    []> {
  let Constraints = "$Rd_in = $Rd";
  let Offset = 0;
}

// POP Rd: Rs1 = MEM64[Rd]; Rd += 8.  Rd is the stack pointer.
let mayLoad = 1 in
def POP : Format00<3, (outs GPR:$Rs1, GPR:$Rd), (ins GPR:$Rd_in),
    "POP\t$Rs1",
    []> {
  let Constraints = "$Rd_in = $Rd";
  let Offset = 0;
}

// MOVE Rd = Rs1 + Offset*2  (address calculation, no memory access).
def MOVErr : Format00<4, (outs GPR:$Rd), (ins GPR:$Rs1, lsreg_offset:$Offset),
  "MOVE\t$Rs1, $Rd$Offset",
    []>;

// User-facing syntax: omit +0 for plain register move.
def : InstAlias<"MOVE\t$Rs1, $Rd", (MOVErr GPR:$Rd, GPR:$Rs1, 0)>;

// Sub-word loads (zero-extended).
def BYTE_LOAD  : Format00<5, (outs GPR:$Rd), (ins mem_ri:$addr),
    "BYTE_LOAD\t$addr, $Rd",
    [(set GPR:$Rd, (zextloadi8  ADDRri:$addr))]> {
  bits<6> addr;
  let Rs1    = addr{3-0};
  let Offset = addr{5-4};
}
def SHORT_LOAD : Format00<7, (outs GPR:$Rd), (ins mem_ri:$addr),
    "SHORT_LOAD\t$addr, $Rd",
    [(set GPR:$Rd, (zextloadi16 ADDRri:$addr))]> {
  bits<6> addr;
  let Rs1    = addr{3-0};
  let Offset = addr{5-4};
}
def WORD_LOAD  : Format00<9, (outs GPR:$Rd), (ins mem_ri:$addr),
    "WORD_LOAD\t$addr, $Rd",
    [(set GPR:$Rd, (zextloadi32 ADDRri:$addr))]> {
  bits<6> addr;
  let Rs1    = addr{3-0};
  let Offset = addr{5-4};
}

// anyext (upper bits undefined) — map to the same zero-extending loads.
def : Pat<(extloadi8  ADDRri:$addr), (BYTE_LOAD  ADDRri:$addr)>;
def : Pat<(extloadi16 ADDRri:$addr), (SHORT_LOAD ADDRri:$addr)>;
def : Pat<(extloadi32 ADDRri:$addr), (WORD_LOAD  ADDRri:$addr)>;

// Sub-word stores.
def BYTE_STORE  : Format00<6, (outs), (ins GPR:$Rd, mem_ri:$addr),
    "BYTE_STORE\t$addr, $Rd",
    [(truncstorei8  GPR:$Rd, ADDRri:$addr)]> {
  bits<6> addr;
  let Rs1    = addr{3-0};
  let Offset = addr{5-4};
}
def SHORT_STORE : Format00<8, (outs), (ins GPR:$Rd, mem_ri:$addr),
    "SHORT_STORE\t$addr, $Rd",
    [(truncstorei16 GPR:$Rd, ADDRri:$addr)]> {
  bits<6> addr;
  let Rs1    = addr{3-0};
  let Offset = addr{5-4};
}
def WORD_STORE  : Format00<10, (outs), (ins GPR:$Rd, mem_ri:$addr),
    "WORD_STORE\t$addr, $Rd",
    [(truncstorei32 GPR:$Rd, ADDRri:$addr)]> {
  bits<6> addr;
  let Rs1    = addr{3-0};
  let Offset = addr{5-4};
}

// Conditional moves (Format 00 JUMP.* with Rd != R15): if condition, Rd = Rs1.
let Uses = [FLAGS] in {
  def CMOV_Z  : Format00<11, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$FalseVal),
      "CMOV.Z\t$Rs1, $Rd", []> {
    let Constraints = "$FalseVal = $Rd";
    let Offset = 0;
  }
  def CMOV_C  : Format00<12, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$FalseVal),
      "CMOV.C\t$Rs1, $Rd", []> {
    let Constraints = "$FalseVal = $Rd";
    let Offset = 0;
  }
  def CMOV_S  : Format00<13, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$FalseVal),
      "CMOV.S\t$Rs1, $Rd", []> {
    let Constraints = "$FalseVal = $Rd";
    let Offset = 0;
  }
  def CMOV_GT : Format00<14, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$FalseVal),
      "CMOV.GT\t$Rs1, $Rd", []> {
    let Constraints = "$FalseVal = $Rd";
    let Offset = 0;
  }
  def CMOV_LT : Format00<15, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$FalseVal),
      "CMOV.LT\t$Rs1, $Rd", []> {
    let Constraints = "$FalseVal = $Rd";
    let Offset = 0;
  }
}

// Indirect branch: MOVE Rs1, PC  (= Format00 MOVE with Rd=R15, Offset=0).
def JMPr : Format00<4, (outs), (ins GPR:$Rs1),
    "MOVE\t$Rs1, PC",
    [(brind GPR:$Rs1)]> {
  let Rd     = 15; // R15 = PC
  let Offset = 0;
  let isTerminator = 1;
  let isBranch     = 1;
  let isBarrier    = 1;
  let isIndirectBranch = 1;
}

//===----------------------------------------------------------------------===//
// Format 01 — LS PC-Relative instructions
//===----------------------------------------------------------------------===//

// PC-relative 64-bit load.
def LOAD_PCREL : Format01<0, (outs GPR:$Rd), (ins pcrel6:$PCRel),
    "LOAD\t@$PCRel, $Rd", []>;

// PC-relative 64-bit store.
def STORE_PCREL : Format01<1, (outs), (ins GPR:$Rd, pcrel6:$PCRel),
    "STORE\t@$PCRel, $Rd", []>;

// MOVE @label, Rd  — load PC-relative address (not memory contents).
def MOVE_PCREL : Format01<4, (outs GPR:$Rd), (ins pcrel6:$PCRel),
    "MOVE\t@$PCRel, $Rd", []>;

// Conditional branches (JUMP.* PC-relative, 10-bit offset).
// These always target R15 (PC).
let isBranch = 1, isTerminator = 1, Uses = [R15, FLAGS] in {
def JUMP_Z  : Format01Jump<11, (outs), (ins pcrel10:$PCRel),
    "JUMP.Z\t@$PCRel", [(brcond (i64 1), bb:$PCRel)]>;
def JUMP_C  : Format01Jump<12, (outs), (ins pcrel10:$PCRel),
    "JUMP.C\t@$PCRel", []>;
def JUMP_S  : Format01Jump<13, (outs), (ins pcrel10:$PCRel),
    "JUMP.S\t@$PCRel", []>;
def JUMP_GT : Format01Jump<14, (outs), (ins pcrel10:$PCRel),
    "JUMP.GT\t@$PCRel", []>;
def JUMP_LT : Format01Jump<15, (outs), (ins pcrel10:$PCRel),
    "JUMP.LT\t@$PCRel", []>;
}

// Unconditional PC-relative jump: dedicated 111 format with 13-bit offset.
def JMP : FormatUJMP<(outs), (ins pcrel13:$PCRel),
    "JUMP\t@$PCRel",
    [(br bb:$PCRel)]> {
  let isBranch    = 1;
  let isTerminator = 1;
  let isBarrier   = 1;
}

//===----------------------------------------------------------------------===//
// Format 10 — Load Immediate
//===----------------------------------------------------------------------===//

// LDI #imm8, Rd  (SHIFT=0: clears register, then loads bits[7:0])
def LDI : FormatLDI<0, (outs GPR:$Rd), (ins uimm8:$Imm8),
    "LDI\t#$Imm8, $Rd", []>;

// LDI.S1 #imm8, Rd  (SHIFT=1: Rd |= imm8 << 8)
def LDI_S1 : FormatLDI<1, (outs GPR:$Rd), (ins uimm8:$Imm8, GPR:$RdIn),
    "LDI.S1\t#$Imm8, $Rd", []> {
  let Constraints = "$RdIn = $Rd";
}

// LDI.S2 #imm8, Rd  (SHIFT=2: Rd |= imm8 << 16)
def LDI_S2 : FormatLDI<2, (outs GPR:$Rd), (ins uimm8:$Imm8, GPR:$RdIn),
    "LDI.S2\t#$Imm8, $Rd", []> {
  let Constraints = "$RdIn = $Rd";
}

// LDI.S3 #imm8, Rd  (SHIFT=3: Rd |= imm8 << 24; sign-extends from bit31 if bit7=1)
def LDI_S3 : FormatLDI<3, (outs GPR:$Rd), (ins uimm8:$Imm8, GPR:$RdIn),
    "LDI.S3\t#$Imm8, $Rd", []> {
  let Constraints = "$RdIn = $Rd";
}

//===----------------------------------------------------------------------===//
// Format 110 — GP ALU
//===----------------------------------------------------------------------===//

let Defs = [R0] in { // Fake use to avoid "R0 is not defined" warnings
}

// Arithmetic
def ADD  : FormatGP<0,  (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$RdIn),
    "ADD\t$Rs1, $Rd",
    [(set GPR:$Rd, (add GPR:$RdIn, GPR:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}
def SUB  : FormatGP<1,  (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$RdIn),
    "SUB\t$Rs1, $Rd",
    [(set GPR:$Rd, (sub GPR:$RdIn, GPR:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}
def TEST : FormatGP<2,  (outs), (ins GPR:$Rs1, GPR:$Rd),
    "TEST\t$Rs1, $Rd", []> {
  let isCompare = 1;
  let Defs = [FLAGS];
}

// Atomic Load-Linked / Store-Conditional
def LLR  : FormatGP<3,  (outs GPR:$Rd), (ins GPR:$Rs1),
    "LLR\t$Rs1, $Rd", []>;
def SCR  : FormatGP<4,  (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$RdIn),
    "SCR\t$Rs1, $Rd", []> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}

// Logical
def AND  : FormatGP<16, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$RdIn),
    "AND\t$Rs1, $Rd",
    [(set GPR:$Rd, (and GPR:$RdIn, GPR:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}
def OR   : FormatGP<17, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$RdIn),
    "OR\t$Rs1, $Rd",
    [(set GPR:$Rd, (or GPR:$RdIn, GPR:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}
def XOR  : FormatGP<18, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$RdIn),
    "XOR\t$Rs1, $Rd",
    [(set GPR:$Rd, (xor GPR:$RdIn, GPR:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}

// Shifts (register shift count)
def SLL  : FormatGP<20, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$RdIn),
    "SLL\t$Rs1, $Rd",
    [(set GPR:$Rd, (shl GPR:$RdIn, GPR:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}
def SRL  : FormatGP<21, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$RdIn),
    "SRL\t$Rs1, $Rd",
    [(set GPR:$Rd, (srl GPR:$RdIn, GPR:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}
def SRA  : FormatGP<22, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$RdIn),
    "SRA\t$Rs1, $Rd",
    [(set GPR:$Rd, (sra GPR:$RdIn, GPR:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}

// Immediate shifts (4-bit literal count in Rs1 field)
def SLLI : FormatGP<23, (outs GPR:$Rd), (ins i64imm:$Rs1, GPR:$RdIn),
    "SLLI\t#$Rs1, $Rd",
    [(set GPR:$Rd, (shl GPR:$RdIn, shamt4:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}
def SRLI : FormatGP<24, (outs GPR:$Rd), (ins i64imm:$Rs1, GPR:$RdIn),
    "SRLI\t#$Rs1, $Rd",
    [(set GPR:$Rd, (srl GPR:$RdIn, shamt4:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}
def SRAI : FormatGP<25, (outs GPR:$Rd), (ins i64imm:$Rs1, GPR:$RdIn),
    "SRAI\t#$Rs1, $Rd",
    [(set GPR:$Rd, (sra GPR:$RdIn, shamt4:$Rs1))]> {
  let Constraints = "$RdIn = $Rd";
  let Defs = [FLAGS];
}

// System call instruction
def SYSCALL : FormatGP<27, (outs), (ins), "SYSCALL", [(trap)]> {
  let Rs1 = 0;
  let Rd  = 0;
}

// Special register access
def LSR  : FormatGP<28, (outs GPR:$Rd), (ins GPR:$Rs1),
    "LSR\t$Rs1, $Rd", []>;
def SSR  : FormatGP<29, (outs), (ins GPR:$Rs1, GPR:$Rd),
    "SSR\t$Rs1, $Rd", []>;

// System instructions
def IRET : FormatGP<30, (outs), (ins), "IRET", []> {
  let Rs1 = 0;
  let Rd  = 0;
  let isReturn     = 1;
  let isTerminator = 1;
  let isBarrier    = 1;
}
def STOP : FormatGP<31, (outs), (ins), "STOP", [(trap)]> {
  let Rs1 = 0;
  let Rd  = 0;
  let isBarrier    = 1;
  let isTerminator = 1;
}

//===----------------------------------------------------------------------===//
// Pseudo instructions (expand during pre-emit pseudo expansion)
//===----------------------------------------------------------------------===//

// CALL pseudo: expands in Little64ExpandPseudos to an LR-based sequence:
//   - materialize return PC into R14 (LR)
//   - jump to target
// No implicit push/pop is performed by the call instruction itself.
//
// Defs lists all registers clobbered at the call site:
//   R1-R10, R12 — caller-saved; may hold junk after the call returns.
//   R14 (LR)    — written by this pseudo with the return address; callee-saved
//                 by convention so the callee's PEI will spill/restore it.
let isCall = 1, Defs = [R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R12, R14, FLAGS], Uses = [R13] in
def CALL : Little64Inst<(outs), (ins GPR:$target),
    "CALL\t$target",
    [(Little64Call GPR:$target)]> {
  let Inst = 0;
  let isPseudo = 1;
}

// RET pseudo: expands in Little64ExpandPseudos to JMPr R14.
let isReturn = 1, isTerminator = 1, isBarrier = 1, Uses = [R14] in
def RET : Little64Inst<(outs), (ins variable_ops),
    "RET",
    [(Little64Ret)]> {
  let Inst = 0;
  let isPseudo = 1;
}

// LDI64 pseudo: expands to byte-wise LDI sequences for sign-extendable 32-bit
// values, or to JMP @+4 / LDI64_RELOC_DATA / LOAD_PCREL @-5 for other values.
// Expansion happens in Little64ExpandPseudos before final emission.
def LDI64 : Little64Inst<(outs GPR:$Rd), (ins i64imm:$imm),
    "LDI64\t${imm}, $Rd",
    [(set GPR:$Rd, (i64 imm:$imm))]> {
  let Inst = 0;
  let isPseudo = 1;
}

// LDI64_RELOC_DATA: Internal pseudo for the 8-byte literal slot used by LDI64
// expansion. The operand may be a constant or a symbol-like expression.
def LDI64_RELOC_DATA : Little64Inst<(outs), (ins i64imm:$value),
    ".quad $value",
    []> {
  let Inst = 0;
  let Size = 8;  // Eight-byte literal slot.
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// Materialize global/external symbol/jump-table addresses via LDI64.
// These are selected from Little64ISD::Wrapper(tglobaladdr/texternalsym/...).
def : Pat<(Little64Wrapper tglobaladdr:$addr),  (LDI64 tglobaladdr:$addr)>;
def : Pat<(Little64Wrapper texternalsym:$addr), (LDI64 texternalsym:$addr)>;
def : Pat<(Little64Wrapper tblockaddress:$addr),(LDI64 tblockaddress:$addr)>;
def : Pat<(Little64Wrapper tjumptable:$addr),   (LDI64 tjumptable:$addr)>;
def : Pat<(Little64Wrapper tglobaltlsaddr:$addr),(LDI64 tglobaltlsaddr:$addr)>;

// LDI64_GOT pseudo: loads a symbol address via GOT-indirect access.
// Expanded in Little64ExpandPseudos to a literal-slot + MOVE_PCREL + LOAD
// + ADD + LOAD sequence. Used in PIC mode for external/global symbols.
// Clobbers R12 (used as scratch for the literal slot address computation).
let Defs = [R12] in
def LDI64_GOT : Little64Inst<(outs GPR:$Rd), (ins i64imm:$sym),
    "LDI64_GOT\t${sym}, $Rd",
    []> {
  let Inst = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// Materialize addresses via GOT-indirect load in PIC mode.
def : Pat<(Little64WrapperGOT tglobaladdr:$addr),  (LDI64_GOT tglobaladdr:$addr)>;
def : Pat<(Little64WrapperGOT texternalsym:$addr), (LDI64_GOT texternalsym:$addr)>;
def : Pat<(Little64WrapperGOT tglobaltlsaddr:$addr), (LDI64_GOT tglobaltlsaddr:$addr)>;

// ReadTP pseudo: reads the thread pointer from special register 0x8000.
// Expanded to LDI+LDI.S1+LSR in Little64ExpandPseudos.
let Defs = [R12] in
def ReadTP : Little64Inst<(outs GPR:$Rd), (ins),
    "READTP\t$Rd",
    [(set GPR:$Rd, (Little64ReadTP))]> {
  let Inst = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// Prefer architectural zero-register copies for i64 zero constants.
// This keeps zero-valued SSA webs anchored to R0 and avoids coalescing with
// mutable temporaries that are only incidentally zero at one point.
def : Pat<(i64 0), (MOVErr R0, 0)>;

// sign_extend_inreg is marked Expand in ISelLowering, so no pattern needed.

// Stack adjustment pseudos required by the calling-convention framework.
// R13 is the stack pointer; both instructions clobber/use it.
let Defs = [R13], Uses = [R13] in {
def ADJCALLSTACKDOWN : Little64Inst<(outs), (ins i64imm:$amt1, i64imm:$amt2),
    "!ADJCALLSTACKDOWN $amt1, $amt2",
    [(callseq_start timm:$amt1, timm:$amt2)]> {
  let Inst = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}
def ADJCALLSTACKUP : Little64Inst<(outs), (ins i64imm:$amt1, i64imm:$amt2),
    "!ADJCALLSTACKUP $amt1, $amt2",
    [(callseq_end timm:$amt1, timm:$amt2)]> {
  let Inst = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}
} // Defs = [R13], Uses = [R13]

// BRCC pseudo: branch on explicit compare operands.
// Expanded to JUMP.* (+ optional JMP fallthrough) in pre-emit expansion.
let isBranch = 1, isTerminator = 1 in
def BRCC : Little64Inst<(outs),
    (ins brtarget:$dest, i64imm:$cc, GPR:$lhs, GPR:$rhs),
    "BRCC\t$cc, $dest",
    [(Little64BRCC bb:$dest, (i64 imm:$cc), GPR:$lhs, GPR:$rhs)]> {
  let Inst = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// SETCC pseudo: materialize a 0/1 value for integer compare predicates.
// Expanded in EmitInstrWithCustomInserter to constant materialization plus a
// SETCC_SELECT pseudo (the compare+select is expanded late pre-emit).
def SETCC : Little64Inst<(outs GPR:$dst), (ins i64imm:$cc, GPR:$lhs, GPR:$rhs),
    "SETCC\t$cc, $lhs, $rhs, $dst",
    [(set GPR:$dst, (Little64SETCC (i64 imm:$cc), GPR:$lhs, GPR:$rhs))]> {
  let Inst = 0;
  let isPseudo = 1;
  let usesCustomInserter = 1;
}

// SETCC_SELECT pseudo: select value based on condition test + CMOV.
// Operands: cc (condition code), lhs, rhs (test operands), t (true val), f (false val)
// Expanded in Little64ExpandPseudos pre-emit pass to: TEST + CMOV sequence.
def SETCC_SELECT : Little64Inst<(outs GPR:$dst),
    (ins i64imm:$cc, GPR:$lhs, GPR:$rhs, GPR:$t, GPR:$f),
    "SETCC_SELECT\t$cc, $lhs, $rhs, $t, $f",
    []> {
  let Constraints = "$f = $dst";
  let Inst = 0;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// ============================================================================
// SELECT lowering via SETCC_SELECT pseudo
// ============================================================================

// ISD::SELECT is Expand → ISD::SELECT_CC.
// ISD::SELECT_CC is Custom → LowerSELECT_CC creates Little64ISD::SETCC_SELECT.
// LowerSELECT_CC unwraps any Little64ISD::SETCC condition to pass the original
// comparison operands directly, enabling a single TEST + CMOV sequence.
//
// This pattern selects Little64ISD::SETCC_SELECT to the SETCC_SELECT pseudo,
// which is then expanded to TEST + CMOV by EmitInstrWithCustomInserter.
def : Pat<(Little64SETCC_SELECT (i64 imm:$cc), GPR:$lhs, GPR:$rhs,
                                GPR:$tval, GPR:$fval),
          (SETCC_SELECT imm:$cc, $lhs, $rhs, $tval, $fval)>;
