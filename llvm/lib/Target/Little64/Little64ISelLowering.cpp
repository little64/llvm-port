//===-- Little64ISelLowering.cpp - Little64 DAG Lowering ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Little64ISelLowering.h"
#include "Little64.h"
#include "Little64InstrInfo.h"
#include "Little64MachineFunctionInfo.h"
#include "Little64Subtarget.h"
#include "Little64TargetMachine.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_CALLINGCONV_INC
#include "Little64GenCallingConv.inc"

Little64TargetLowering::Little64TargetLowering(const TargetMachine &TM,
                                                const Little64Subtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  // Set up the register class.
  addRegisterClass(MVT::i64, &Little64::GPRRegClass);

  computeRegisterProperties(STI.getRegisterInfo());

  // Stack pointer register.
  setStackPointerRegisterToSaveRestore(Little64::R13);

  // Set minimum function alignment.
  setMinFunctionAlignment(Align(2));

  // Scheduling preference.
  setSchedulingPreference(Sched::RegPressure);

  //===--------------------------------------------------------------------===//
  // Operations not supported natively → expand to libcalls or sequences
  //===--------------------------------------------------------------------===//

  // No hardware multiply / divide / remainder.
  for (auto Op : {ISD::MUL, ISD::MULHS, ISD::MULHU, ISD::SMUL_LOHI,
                  ISD::UMUL_LOHI, ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM})
    setOperationAction(Op, MVT::i64, Expand);

  // No hardware shifts beyond 15 bits for immediate form; use register form
  // for all shifts — mark as Legal (handled via SLL/SRL/SRA patterns).
  setOperationAction(ISD::SHL_PARTS, MVT::i64, Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i64, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i64, Expand);

  // No FP hardware.
  for (auto VT : {MVT::f32, MVT::f64}) {
    for (auto Op : {ISD::FADD, ISD::FSUB, ISD::FMUL, ISD::FDIV, ISD::FREM,
                    ISD::FNEG, ISD::FABS, ISD::FSQRT, ISD::FSIN, ISD::FCOS})
      setOperationAction(Op, VT, Expand);
    setOperationAction(ISD::FP_TO_SINT, VT, Expand);
    setOperationAction(ISD::FP_TO_UINT, VT, Expand);
    setOperationAction(ISD::SINT_TO_FP, VT, Expand);
    setOperationAction(ISD::UINT_TO_FP, VT, Expand);
  }

  // No atomics.
  setMaxAtomicSizeInBitsSupported(0);

  // Custom lowering for constants that don't fit in 8 bits.
  setOperationAction(ISD::Constant,        MVT::i64, Custom);
  setOperationAction(ISD::GlobalAddress,   MVT::i64, Custom);
  setOperationAction(ISD::GlobalTLSAddress, MVT::i64, Custom);
  // ExternalSymbol (libcalls, asm refs) and JumpTable addresses use the same
  // literal-pool materialization path as GlobalAddress.
  setOperationAction(ISD::ExternalSymbol,  MVT::i64, Custom);
  setOperationAction(ISD::JumpTable,       MVT::i64, Custom);

  // SELECT: expand to SELECT_CC during legalization (default behavior).
  // SELECT_CC: custom-lower to SETCC_SELECT pseudo (to avoid infinite expansion loop).
  // SETCC_SELECT is expanded late in the pre-emit pseudo expansion pass.
  setOperationAction(ISD::SELECT,    MVT::i64, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i64, Custom);
  setOperationAction(ISD::SETCC,     MVT::i64, Custom);

  // BR_CC: custom-lower to Little64ISD::BRCC pseudo.
  setOperationAction(ISD::BR_CC, MVT::i64, Custom);

  // BRCOND action is keyed by condition operand type (not MVT::Other).
  // Mark both i1 and legalized i64 forms custom to prevent generic
  // BRCOND->BR_CC expansion churn.
  setOperationAction(ISD::BRCOND, MVT::i1, Custom);
  setOperationAction(ISD::BRCOND, MVT::i64, Custom);

  // Jump tables: custom-lower to an indirect branch through the table.
  setOperationAction(ISD::BR_JT, MVT::Other, Custom);

  // Sign-extend in register: expand via shift-left + arithmetic-shift-right.
  for (auto VT : {MVT::i1, MVT::i8, MVT::i16, MVT::i32})
    setOperationAction(ISD::SIGN_EXTEND_INREG, VT, Expand);

  // Sign-extending loads: expand to zero-extend + sign-extend-inreg for
  // byte/half/word memory operations. Keep EXTLOAD legal there, and promote
  // i1 extloads so they do not reach i64/i1 illegal selection paths.
  for (auto MemVT : {MVT::i8, MVT::i16, MVT::i32}) {
    setLoadExtAction(ISD::SEXTLOAD, MVT::i64, MemVT, Expand);
    setLoadExtAction(ISD::EXTLOAD,  MVT::i64, MemVT, Legal);
  }
  setLoadExtAction(ISD::SEXTLOAD, MVT::i64, MVT::i1, Promote);
  setLoadExtAction(ISD::EXTLOAD,  MVT::i64, MVT::i1, Promote);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i64, MVT::i1, Promote);

  // Rotate: expand to shift sequences.
  setOperationAction(ISD::ROTL, MVT::i64, Expand);
  setOperationAction(ISD::ROTR, MVT::i64, Expand);

  // Byte-swap: expand to shift/OR sequences.
  setOperationAction(ISD::BSWAP, MVT::i32, Expand);
  setOperationAction(ISD::BSWAP, MVT::i64, Expand);

  // Combined divide/remainder: expand to separate div and rem ops,
  // which are then each lowered to their individual libcalls.
  setOperationAction(ISD::SDIVREM, MVT::i64, Expand);
  setOperationAction(ISD::UDIVREM, MVT::i64, Expand);

  // Varargs: VASTART is custom-lowered to store the first-vararg frame address;
  // VAARG and VAEND use generic expansion (pointer arithmetic / no-op).
  // VACOPY expands to a load+store of the va_list pointer (which is a single
  // pointer on Little64).
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG,   MVT::Other, Expand);
  setOperationAction(ISD::VAEND,   MVT::Other, Expand);
  setOperationAction(ISD::VACOPY,  MVT::Other, Expand);

  // Bit-counting: no hardware support — expand to shift/compare sequences.
  setOperationAction(ISD::CTLZ,  MVT::i64, Expand);
  setOperationAction(ISD::CTTZ,  MVT::i64, Expand);
  setOperationAction(ISD::CTPOP, MVT::i64, Expand);

  // Return/frame address intrinsics: implementations exist in LowerOperation
  // but must be registered here so Custom dispatch is invoked.
  setOperationAction(ISD::RETURNADDR, MVT::i64, Custom);
  setOperationAction(ISD::FRAMEADDR,  MVT::i64, Custom);

  // Dynamic stack allocation and save/restore: expand to SP arithmetic.
  // DYNAMIC_STACKALLOC → subtract aligned size from R13, return new R13.
  // STACKSAVE/STACKRESTORE → copy R13 to/from a GPR.
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i64,   Expand);
  setOperationAction(ISD::STACKSAVE,          MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE,       MVT::Other, Expand);

  // Block addresses: custom-lowered through Little64ISD::Wrapper → LDI64,
  // matching the same path used for GlobalAddress / ExternalSymbol.
  setOperationAction(ISD::BlockAddress, MVT::i64, Custom);

  // Boolean values produced by SETCC / CMOV are strict 0-or-1 integers.
  // Declaring this lets DAG combines fold patterns like (zext (setcc ...)) → setcc
  // and avoids spurious sign/zero-extend nodes around boolean results.
  setBooleanContents(ZeroOrOneBooleanContent);
}

const char *Little64TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case Little64ISD::CALL:          return "Little64ISD::CALL";
  case Little64ISD::RET_FLAG:      return "Little64ISD::RET_FLAG";
  case Little64ISD::BRCC:          return "Little64ISD::BRCC";
  case Little64ISD::SETCC:         return "Little64ISD::SETCC";
  case Little64ISD::SETCC_SELECT:  return "Little64ISD::SETCC_SELECT";
  case Little64ISD::Wrapper:       return "Little64ISD::Wrapper";
  case Little64ISD::WrapperGOT:    return "Little64ISD::WrapperGOT";
  case Little64ISD::ReadTP:        return "Little64ISD::ReadTP";
  default: return nullptr;
  }
}

SDValue Little64TargetLowering::LowerOperation(SDValue Op,
                                                SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::Constant:
    return LowerConstant(Op, DAG);
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::GlobalTLSAddress:
    return LowerGlobalTLSAddress(Op, DAG);
  case ISD::BR_CC:
    return LowerBR_CC(Op, DAG);
  case ISD::BRCOND:
    return LowerBRCOND(Op, DAG);
  case ISD::SETCC:
    return LowerSETCC(Op, DAG);
  case ISD::SELECT_CC:
    return LowerSELECT_CC(Op, DAG);
  case ISD::ExternalSymbol:
    return LowerExternalSymbol(Op, DAG);
  case ISD::JumpTable:
    return LowerJumpTable(Op, DAG);
  case ISD::BR_JT:
    return LowerBR_JT(Op, DAG);
  case ISD::VASTART:
    return LowerVASTART(Op, DAG);
  case ISD::BlockAddress:
    return LowerBlockAddress(Op, DAG);
  case ISD::RETURNADDR:
    return LowerRETURNADDR(Op, DAG);
  case ISD::FRAMEADDR:
    return LowerFRAMEADDR(Op, DAG);
  default:
    llvm_unreachable("Unimplemented Little64 custom lowering");
  }
}

SDValue Little64TargetLowering::LowerBR_JT(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain     = Op.getOperand(0);
  SDValue Table     = Op.getOperand(1); // JumpTable node
  SDValue Index     = Op.getOperand(2); // jump-table entry index
  EVT PtrVT         = getPointerTy(DAG.getDataLayout());
  SDValue IndexPtr  = DAG.getZExtOrTrunc(Index, DL, PtrVT);

  unsigned JTEncoding = getJumpTableEncoding();

  if (JTEncoding == MachineJumpTableInfo::EK_LabelDifference32) {
    // PIC: entries are 4-byte signed deltas (target - table_base).
    // Scale index by 4.
    IndexPtr = DAG.getNode(ISD::SHL, DL, PtrVT, IndexPtr,
                           DAG.getConstant(2, DL, PtrVT));
    SDValue Addr = DAG.getNode(ISD::ADD, DL, PtrVT, Table, IndexPtr);
    // Load 4-byte delta and sign-extend to pointer width.
    SDValue Delta = DAG.getExtLoad(ISD::SEXTLOAD, DL, PtrVT, Chain, Addr,
                                   MachinePointerInfo(), MVT::i32);
    // Target = table_base + delta.
    SDValue Target = DAG.getNode(ISD::ADD, DL, PtrVT, Table, Delta);
    return DAG.getNode(ISD::BRIND, DL, MVT::Other, Delta.getValue(1), Target);
  }

  // Non-PIC: entries are absolute 8-byte addresses.
  const uint64_t EntrySize = DAG.getDataLayout().getPointerSize();
  if (EntrySize == 8) {
    IndexPtr = DAG.getNode(ISD::SHL, DL, PtrVT, IndexPtr,
                           DAG.getConstant(3, DL, PtrVT));
  } else if (EntrySize > 1) {
    IndexPtr = DAG.getNode(ISD::MUL, DL, PtrVT, IndexPtr,
                           DAG.getConstant(EntrySize, DL, PtrVT));
  }

  // Compute the address of the target entry: Table + (Index * EntrySize).
  SDValue Addr = DAG.getNode(ISD::ADD, DL, PtrVT, Table, IndexPtr);

  // Load the jump target from the table.
  SDValue Target = DAG.getLoad(PtrVT, DL, Chain, Addr, MachinePointerInfo());

  // Indirect branch to the loaded target.
  return DAG.getNode(ISD::BRIND, DL, MVT::Other, Target.getValue(1), Target);
}

SDValue Little64TargetLowering::LowerConstant(SDValue Op,
                                               SelectionDAG &DAG) const {
  // Constants are left for target-specific materialization. The final machine
  // code path will either build them with LDI instructions or use the literal
  // slot expansion handled by Little64ExpandPseudos.
  return Op; // Will be handled by isel patterns / materializer.
}

SDValue Little64TargetLowering::LowerGlobalAddress(SDValue Op,
                                                    SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  const GlobalAddressSDNode *GA = cast<GlobalAddressSDNode>(Op);

  // In PIC mode, load the symbol address via a GOT-indirect load.
  if (isPositionIndependent()) {
    SDValue Addr = DAG.getTargetGlobalAddress(GA->getGlobal(), DL, PtrVT,
                                               GA->getOffset(),
                                               Little64II::MO_GOT);
    return DAG.getNode(Little64ISD::WrapperGOT, DL, PtrVT, Addr);
  }

  SDValue Addr = DAG.getTargetGlobalAddress(GA->getGlobal(), DL, PtrVT,
                                             GA->getOffset());
  return DAG.getNode(Little64ISD::Wrapper, DL, PtrVT, Addr);
}

SDValue Little64TargetLowering::LowerGlobalTLSAddress(SDValue Op,
                                                       SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  const GlobalAddressSDNode *GA = cast<GlobalAddressSDNode>(Op);

  SDValue Tp = DAG.getNode(Little64ISD::ReadTP, DL, PtrVT);

  if (isPositionIndependent()) {
    // TLS Initial Exec: load the TP-relative offset from the GOT, then
    // add it to the thread pointer.
    SDValue TpOff = DAG.getTargetGlobalAddress(GA->getGlobal(), DL, PtrVT,
                                                GA->getOffset(),
                                                Little64II::MO_GOTTPREL);
    SDValue GotLoad = DAG.getNode(Little64ISD::WrapperGOT, DL, PtrVT, TpOff);
    return DAG.getNode(ISD::ADD, DL, PtrVT, Tp, GotLoad);
  }

  // TLS Local Exec: materialize the TP-relative offset directly, read the
  // thread pointer, and add them.
  SDValue TpOff = DAG.getTargetGlobalAddress(GA->getGlobal(), DL, PtrVT,
                                              GA->getOffset(),
                                              Little64II::MO_TPREL);
  SDValue Offset = DAG.getNode(Little64ISD::Wrapper, DL, PtrVT, TpOff);
  return DAG.getNode(ISD::ADD, DL, PtrVT, Tp, Offset);
}

SDValue Little64TargetLowering::LowerExternalSymbol(SDValue Op,
                                                     SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  const ExternalSymbolSDNode *ES = cast<ExternalSymbolSDNode>(Op);
  // Some inline-asm sideeffect barriers can flow through with an unnamed
  // external symbol. Model these as null pointers instead of creating an
  // empty symbol name that would trip later mangling/printing asserts.
  const char *SymName = ES->getSymbol();
  if (!SymName || SymName[0] == '\0')
    return DAG.getConstant(0, DL, PtrVT);
  // In PIC mode, external symbols must go through the GOT.
  if (isPositionIndependent()) {
    SDValue Sym = DAG.getTargetExternalSymbol(SymName, PtrVT,
                                               Little64II::MO_GOT);
    return DAG.getNode(Little64ISD::WrapperGOT, DL, PtrVT, Sym);
  }

  SDValue Sym = DAG.getTargetExternalSymbol(SymName, PtrVT);
  return DAG.getNode(Little64ISD::Wrapper, DL, PtrVT, Sym);
}

SDValue Little64TargetLowering::LowerJumpTable(SDValue Op,
                                                SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  const JumpTableSDNode *JT = cast<JumpTableSDNode>(Op);
  SDValue Addr = DAG.getTargetJumpTable(JT->getIndex(), PtrVT);
  return DAG.getNode(Little64ISD::Wrapper, DL, PtrVT, Addr);
}

SDValue Little64TargetLowering::LowerBlockAddress(SDValue Op,
                                                   SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  const BlockAddressSDNode *BA = cast<BlockAddressSDNode>(Op);
  SDValue Addr = DAG.getTargetBlockAddress(BA->getBlockAddress(), PtrVT,
                                            BA->getOffset());
  return DAG.getNode(Little64ISD::Wrapper, DL, PtrVT, Addr);
}

//===----------------------------------------------------------------------===//
// Calling convention implementation — LowerFormalArguments
//===----------------------------------------------------------------------===//

SDValue Little64TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  // Vararg functions use an all-stack CC so all arguments form a contiguous
  // region in memory, which the void* va_list pointer can traverse linearly.
  if (IsVarArg)
    CCInfo.AnalyzeFormalArguments(Ins, CC_Little64_VarArg);
  else
    CCInfo.AnalyzeFormalArguments(Ins, CC_Little64);

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];

    if (VA.isRegLoc() && !IsVarArg) {
      EVT RegVT = VA.getLocVT();
      const TargetRegisterClass *RC = &Little64::GPRRegClass;
      Register VReg = RegInfo.createVirtualRegister(RC);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, RegVT);

      // Truncate or extend if necessary.
      if (VA.getLocInfo() == CCValAssign::SExt)
        ArgValue = DAG.getNode(ISD::AssertSext, DL, RegVT, ArgValue,
                               DAG.getValueType(VA.getValVT()));
      else if (VA.getLocInfo() == CCValAssign::ZExt)
        ArgValue = DAG.getNode(ISD::AssertZext, DL, RegVT, ArgValue,
                               DAG.getValueType(VA.getValVT()));

      if (VA.getLocInfo() != CCValAssign::Full)
        ArgValue = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), ArgValue);

      InVals.push_back(ArgValue);
    } else {
      // Stack argument.
      assert(VA.isMemLoc() && "Expected memory location for stack arg");
      unsigned Offset = VA.getLocMemOffset();
      unsigned ValSize = VA.getValVT().getSizeInBits() / 8;

      MachineFrameInfo &MFI = MF.getFrameInfo();
      int FI = MFI.CreateFixedObject(ValSize, Offset, /*IsImmutable=*/true);
      SDValue FIPtr = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
      SDValue Load = DAG.getLoad(VA.getValVT(), DL, Chain, FIPtr,
                                 MachinePointerInfo::getFixedStack(MF, FI));
      InVals.push_back(Load);
    }
  }

  // Vararg: record the frame index of the first vararg so LowerVASTART can
  // materialize the va_list pointer. The first vararg lives at the next stack
  // slot after all the fixed arguments.
  if (IsVarArg) {
    unsigned VarArgOffset = CCInfo.getStackSize();
    auto *FuncInfo = MF.getInfo<Little64MachineFunctionInfo>();
    int FI = MF.getFrameInfo().CreateFixedObject(8, VarArgOffset,
                                                  /*IsImmutable=*/true);
    FuncInfo->setVarArgsFrameIndex(FI);
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
// LowerVASTART
//===----------------------------------------------------------------------===//

SDValue Little64TargetLowering::LowerVASTART(SDValue Op,
                                               SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  auto *FuncInfo = MF.getInfo<Little64MachineFunctionInfo>();
  SDLoc DL(Op);
  EVT PtrVT = getPointerTy(DAG.getDataLayout());

  // Create a pointer to the first vararg argument on the stack.
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(), PtrVT);

  // Store that pointer into the va_list location (Op.getOperand(1)).
  // The third operand (Op.getOperand(2)) is a SrcValueSDNode carrying the
  // IR Value* for alias analysis.
  return DAG.getStore(
      Op.getOperand(0), DL, FI, Op.getOperand(1),
      MachinePointerInfo(
          cast<SrcValueSDNode>(Op.getOperand(2))->getValue()));
}

//===----------------------------------------------------------------------===//
// LowerRETURNADDR / LowerFRAMEADDR
//===----------------------------------------------------------------------===//

SDValue Little64TargetLowering::LowerRETURNADDR(SDValue Op,
                                                  SelectionDAG &DAG) const {
  // Only level 0 (current frame) is supported. Higher levels would require
  // walking the call chain.
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MFI.setReturnAddressIsTaken(true);

  if (cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue() != 0) {
    // For non-zero levels, return undef rather than crash.
    return DAG.getUNDEF(Op.getValueType());
  }

  SDLoc DL(Op);
  EVT VT = Op.getValueType();
  // R14 is the link register holding the return address.
  Register R14 = MF.addLiveIn(Little64::R14, &Little64::GPRRegClass);
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL, R14, VT);
}

SDValue Little64TargetLowering::LowerFRAMEADDR(SDValue Op,
                                                 SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MFI.setFrameAddressIsTaken(true);

  if (cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue() != 0)
    return DAG.getUNDEF(Op.getValueType());

  SDLoc DL(Op);
  EVT VT = Op.getValueType();
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
  Register FrameReg = TFI->hasFP(MF) ? Little64::R11 : Little64::R13;
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL,
                            MF.addLiveIn(FrameReg, &Little64::GPRRegClass),
                            VT);
}

//===----------------------------------------------------------------------===//
// LowerCall
//===----------------------------------------------------------------------===//

SDValue
Little64TargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                   SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  bool &IsTailCall = CLI.IsTailCall;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;
  MachineFunction &MF = DAG.getMachineFunction();

  // Tail calls are not supported yet.
  IsTailCall = false;

  // Analyze outgoing arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  // Use all-stack CC for vararg calls to match LowerFormalArguments.
  if (IsVarArg)
    CCInfo.AnalyzeCallOperands(Outs, CC_Little64_VarArg);
  else
    CCInfo.AnalyzeCallOperands(Outs, CC_Little64);

  // Determine total stack bytes needed.
  unsigned NumBytes = CCInfo.getStackSize();

  // Reserve stack space.
  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[i];

    // Promote if needed.
    switch (VA.getLocInfo()) {
    default: llvm_unreachable("Unknown loc info");
    case CCValAssign::Full: break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, DL, VA.getLocVT(), Arg);
      break;
    }

    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
    } else {
      // Stack argument.
      assert(VA.isMemLoc());
      SDValue StackPtr = DAG.getCopyFromReg(Chain, DL, Little64::R13,
                                            getPointerTy(DAG.getDataLayout()));
      SDValue PtrOff = DAG.getIntPtrConstant(VA.getLocMemOffset(), DL);
      SDValue Address = DAG.getNode(ISD::ADD, DL,
                                    getPointerTy(DAG.getDataLayout()),
                                    StackPtr, PtrOff);
      MemOpChains.push_back(DAG.getStore(Chain, DL, Arg, Address,
                                         MachinePointerInfo()));
    }
  }

  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  // Build a sequence of copy-to-reg nodes for call argument registers.
  SDValue InFlag;
  for (auto &P : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, P.first, P.second, InFlag);
    InFlag = Chain.getValue(1);
  }

  // The target node for the call.
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (auto &P : RegsToPass)
    Ops.push_back(DAG.getRegister(P.first, P.second.getValueType()));

  // Add argument registers as live-in.
  const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(MF, CallConv);
  assert(Mask && "Missing call preserved mask for calling convention");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(Little64ISD::CALL, DL, NodeTys, Ops);
  InFlag = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, InFlag, DL);
  InFlag = Chain.getValue(1);

  // Copy return values out of their physical registers.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(Ins, RetCC_Little64);

  for (unsigned i = 0, e = RVLocs.size(); i != e; ++i) {
    CCValAssign &VA = RVLocs[i];
    SDValue RetValue =
        DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), InFlag);
    Chain = RetValue.getValue(1);
    InFlag = RetValue.getValue(2);
    InVals.push_back(RetValue);
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
// LowerReturn
//===----------------------------------------------------------------------===//

SDValue Little64TargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();

  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_Little64);

  SDValue Flag;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  for (unsigned i = 0, e = RVLocs.size(); i != e; ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers");

    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[i], Flag);
    Flag = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  RetOps[0] = Chain;
  if (Flag.getNode())
    RetOps.push_back(Flag);

  return DAG.getNode(Little64ISD::RET_FLAG, DL, MVT::Other, RetOps);
}

bool Little64TargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    LLVMContext &Context, const Type *RetTy) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_Little64);
}

//===----------------------------------------------------------------------===//
// LowerBR_CC — conditional branch
//===----------------------------------------------------------------------===//

static Little64CC::CondCode
mapCondCode(ISD::CondCode CC) {
  switch (CC) {
  default: report_fatal_error("BR_CC: unsupported condition code");
  case ISD::SETEQ:  return Little64CC::EQ;
  case ISD::SETNE:  return Little64CC::NE;
  case ISD::SETLT:  return Little64CC::LT;
  case ISD::SETLE:  return Little64CC::LE;
  case ISD::SETGT:  return Little64CC::GT;
  case ISD::SETGE:  return Little64CC::GE;
  case ISD::SETULT: return Little64CC::ULT;
  case ISD::SETULE: return Little64CC::ULE;
  case ISD::SETUGT: return Little64CC::UGT;
  case ISD::SETUGE: return Little64CC::UGE;
  }
}

static bool isSignedCondCode(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETLT:
  case ISD::SETLE:
  case ISD::SETGT:
  case ISD::SETGE:
    return true;
  default:
    return false;
  }
}

static ISD::CondCode mapSignedToUnsignedCondCode(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETLT:
    return ISD::SETULT;
  case ISD::SETLE:
    return ISD::SETULE;
  case ISD::SETGT:
    return ISD::SETUGT;
  case ISD::SETGE:
    return ISD::SETUGE;
  default:
    llvm_unreachable("Expected signed condition code");
  }
}

static void lowerSignedCompareViaUnsigned(ISD::CondCode &CC, SDValue &LHS,
                                          SDValue &RHS, const SDLoc &DL,
                                          SelectionDAG &DAG) {
  if (!isSignedCondCode(CC))
    return;

  EVT VT = LHS.getValueType();
  if (!VT.isInteger())
    return;

  unsigned Bits = VT.getScalarSizeInBits();
  if (Bits == 0)
    return;

  SDValue Bias = DAG.getConstant(APInt::getSignMask(Bits), DL, VT);
  LHS = DAG.getNode(ISD::XOR, DL, VT, LHS, Bias);
  RHS = DAG.getNode(ISD::XOR, DL, VT, RHS, Bias);
  CC = mapSignedToUnsignedCondCode(CC);
}

static void canonicalizeBranchCondForCmp(Little64CC::CondCode &CC,
                                         SDValue &LHS, SDValue &RHS) {
  switch (CC) {
  case Little64CC::UGT:
    // A > B unsigned  ≡  B < A unsigned.
    std::swap(LHS, RHS);
    CC = Little64CC::ULT;
    return;
  case Little64CC::ULE:
    // A <= B unsigned  ≡  B >= A unsigned.
    std::swap(LHS, RHS);
    CC = Little64CC::UGE;
    return;
  default:
    return;
  }
}

SDValue Little64TargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain  = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS    = Op.getOperand(2);
  SDValue RHS    = Op.getOperand(3);
  SDValue DestBB = Op.getOperand(4);
  SDLoc DL(Op);

  // Signed compares require overflow-aware ordering. Little64 only exposes
  // sign/carry flags, so materialize signed ordering by biasing both operands
  // with the sign bit and using the unsigned relation.
  lowerSignedCompareViaUnsigned(CC, LHS, RHS, DL, DAG);

  // Replace constant zero with the hardware zero register R0 so the BRCC
  // pseudo always gets two GPR operands.
  auto materializeIfZero = [&](SDValue V) -> SDValue {
    if (auto *C = dyn_cast<ConstantSDNode>(V))
      if (C->isZero())
        return DAG.getRegister(Little64::R0, MVT::i64);
    return V;
  };
  LHS = materializeIfZero(LHS);
  RHS = materializeIfZero(RHS);
  LHS = DAG.getZExtOrTrunc(LHS, DL, MVT::i64);
  RHS = DAG.getZExtOrTrunc(RHS, DL, MVT::i64);

  Little64CC::CondCode TCC = mapCondCode(CC);
  canonicalizeBranchCondForCmp(TCC, LHS, RHS);
  SDValue Ops[] = {Chain, DestBB, DAG.getConstant(TCC, DL, MVT::i64), LHS,
                   RHS};
  return DAG.getNode(Little64ISD::BRCC, DL, MVT::Other, Ops);
}

SDValue Little64TargetLowering::LowerBRCOND(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDValue Chain  = Op.getOperand(0);
  SDValue Cond   = Op.getOperand(1);
  SDValue DestBB = Op.getOperand(2);
  SDLoc DL(Op);

  auto materializeIfZero = [&](SDValue V) -> SDValue {
    if (auto *C = dyn_cast<ConstantSDNode>(V))
      if (C->isZero())
        return DAG.getRegister(Little64::R0, MVT::i64);
    return V;
  };

  // If BRCOND is branching on a compare result, lower it directly to BRCC.
  // This avoids generating SETCC materialization code (TEST+CMOV) that can
  // force extra reloads and accidentally clobber flags between compare and use.
  if (Cond.getOpcode() == Little64ISD::SETCC) {
    Little64CC::CondCode TCC =
        static_cast<Little64CC::CondCode>(
            cast<ConstantSDNode>(Cond.getOperand(0))->getSExtValue());
    SDValue LHS = materializeIfZero(Cond.getOperand(1));
    SDValue RHS = materializeIfZero(Cond.getOperand(2));
    LHS = DAG.getZExtOrTrunc(LHS, DL, MVT::i64);
    RHS = DAG.getZExtOrTrunc(RHS, DL, MVT::i64);
    canonicalizeBranchCondForCmp(TCC, LHS, RHS);
    SDValue Ops[] = {Chain, DestBB, DAG.getConstant(TCC, DL, MVT::i64), LHS,
                     RHS};
    return DAG.getNode(Little64ISD::BRCC, DL, MVT::Other, Ops);
  }

  if (Cond.getOpcode() == ISD::SETCC) {
    SDValue LHS = Cond.getOperand(0);
    SDValue RHS = Cond.getOperand(1);

    // Only fold integer SETCC into BRCC; FP comparisons are softened later
    // and will re-enter this path with integer operands.
    if (LHS.getValueType().isInteger()) {
      ISD::CondCode CC = cast<CondCodeSDNode>(Cond.getOperand(2))->get();

      lowerSignedCompareViaUnsigned(CC, LHS, RHS, DL, DAG);
      LHS = materializeIfZero(LHS);
      RHS = materializeIfZero(RHS);
      LHS = DAG.getZExtOrTrunc(LHS, DL, MVT::i64);
      RHS = DAG.getZExtOrTrunc(RHS, DL, MVT::i64);

      Little64CC::CondCode TCC = mapCondCode(CC);
      canonicalizeBranchCondForCmp(TCC, LHS, RHS);
      SDValue Ops[] = {Chain, DestBB, DAG.getConstant(TCC, DL, MVT::i64), LHS,
                       RHS};
      return DAG.getNode(Little64ISD::BRCC, DL, MVT::Other, Ops);
    }
  }

  // BRCOND operates on an i1-like condition, but BRCC expects i64 GPRs.
  SDValue Cond64 = DAG.getZExtOrTrunc(Cond, DL, MVT::i64);
  SDValue Zero = DAG.getRegister(Little64::R0, MVT::i64);
  SDValue Ops[] = {Chain, DestBB, DAG.getConstant(Little64CC::NE, DL, MVT::i64),
                   Cond64, Zero};
  return DAG.getNode(Little64ISD::BRCC, DL, MVT::Other, Ops);
}

SDValue Little64TargetLowering::LowerSETCC(SDValue Op,
                                            SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();

  // Preserve LLVM signed-icmp semantics by lowering to an unsigned compare
  // after sign-bit biasing.
  lowerSignedCompareViaUnsigned(CC, LHS, RHS, DL, DAG);

  // Replace constant zero with the hardware zero register R0 so the pseudo
  // always receives register operands.
  auto materializeIfZero = [&](SDValue V) -> SDValue {
    if (auto *C = dyn_cast<ConstantSDNode>(V))
      if (C->isZero())
        return DAG.getRegister(Little64::R0, MVT::i64);
    return V;
  };
  LHS = materializeIfZero(LHS);
  RHS = materializeIfZero(RHS);

  Little64CC::CondCode TCC = mapCondCode(CC);
  SDValue Ops[] = {DAG.getConstant(TCC, DL, MVT::i64), LHS, RHS};
  return DAG.getNode(Little64ISD::SETCC, DL, MVT::i64, Ops);
}

SDValue Little64TargetLowering::LowerSELECT_CC(SDValue Op,
                                               SelectionDAG &DAG) const {
  SDValue LHS = Op.getOperand(0);      // Compare LHS
  SDValue RHS = Op.getOperand(1);      // Compare RHS
  SDValue TVal = Op.getOperand(2);     // True value
  SDValue FVal = Op.getOperand(3);     // False value
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDLoc DL(Op);

  auto invertLittle64Cond = [](Little64CC::CondCode C) {
    switch (C) {
    case Little64CC::EQ:
      return Little64CC::NE;
    case Little64CC::NE:
      return Little64CC::EQ;
    case Little64CC::LT:
      return Little64CC::GE;
    case Little64CC::GE:
      return Little64CC::LT;
    case Little64CC::GT:
      return Little64CC::LE;
    case Little64CC::LE:
      return Little64CC::GT;
    case Little64CC::ULT:
      return Little64CC::UGE;
    case Little64CC::UGE:
      return Little64CC::ULT;
    case Little64CC::UGT:
      return Little64CC::ULE;
    case Little64CC::ULE:
      return Little64CC::UGT;
    }
    llvm_unreachable("unknown Little64 condition code");
  };

  auto canonicalizeSetccSelect = [&](Little64CC::CondCode &TCC,
                                     SDValue &TrueVal,
                                     SDValue &FalseVal) {
    switch (TCC) {
    case Little64CC::NE:
    case Little64CC::GE:
    case Little64CC::LE:
    case Little64CC::UGE:
    case Little64CC::ULE:
      TCC = invertLittle64Cond(TCC);
      std::swap(TrueVal, FalseVal);
      break;
    default:
      break;
    }
  };

  // Common case: SELECT was expanded from SELECT(SETCC(x, y, cc), t, f).
  // By the time LowerSELECT_CC is called, the condition has been custom-lowered
  // to Little64ISD::SETCC and SELECT expansion created SELECT_CC(..., 0, ..., SETNE).
  // Unwrap to recover the original comparison operands so we can emit a direct
  // TEST + CMOV instead of a SETCC + TEST + CMOV sequence.
  if (LHS.getOpcode() == Little64ISD::SETCC &&
      (CC == ISD::SETNE || CC == ISD::SETEQ)) {
    auto *RHSConst = dyn_cast<ConstantSDNode>(RHS);
    if (RHSConst && RHSConst->isZero()) {
      int64_t InnerCC = cast<ConstantSDNode>(LHS.getOperand(0))->getSExtValue();
      SDValue X = LHS.getOperand(1);
      SDValue Y = LHS.getOperand(2);
      // SETEQ against 0 means "condition was false" — invert by swapping values.
      if (CC == ISD::SETEQ)
        std::swap(TVal, FVal);
      Little64CC::CondCode TCC = static_cast<Little64CC::CondCode>(InnerCC);
      canonicalizeSetccSelect(TCC, TVal, FVal);
      SDValue Ops[] = {DAG.getConstant(static_cast<int64_t>(TCC), DL, MVT::i64),
                       X, Y, TVal, FVal};
      return DAG.getNode(Little64ISD::SETCC_SELECT, DL, MVT::i64, Ops);
    }
  }

  // General case: SELECT(non-SETCC-condition, t, f) or direct SELECT_CC in IR.
  // Replace constant zero operands with R0 so the pseudo always gets GPR inputs.
  lowerSignedCompareViaUnsigned(CC, LHS, RHS, DL, DAG);

  auto materializeIfZero = [&](SDValue V) -> SDValue {
    if (auto *C = dyn_cast<ConstantSDNode>(V))
      if (C->isZero())
        return DAG.getRegister(Little64::R0, MVT::i64);
    return V;
  };
  LHS = materializeIfZero(LHS);
  RHS = materializeIfZero(RHS);

  Little64CC::CondCode TCC = mapCondCode(CC);
  canonicalizeSetccSelect(TCC, TVal, FVal);
  SDValue Ops[] = {DAG.getConstant(TCC, DL, MVT::i64), LHS, RHS, TVal, FVal};
  return DAG.getNode(Little64ISD::SETCC_SELECT, DL, MVT::i64, Ops);
}

//===----------------------------------------------------------------------===//
// EmitInstrWithCustomInserter — expand pseudo-instructions
//===----------------------------------------------------------------------===//

MachineBasicBlock *
Little64TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                     MachineBasicBlock *MBB) const {
  assert(MI.getOpcode() == Little64::SETCC &&
         "EmitInstrWithCustomInserter: unknown pseudo");

  const TargetInstrInfo *TII =
      MBB->getParent()->getSubtarget().getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();

  Register Dst = MI.getOperand(0).getReg();
  int64_t CC = MI.getOperand(1).getImm();
  Register LHS = MI.getOperand(2).getReg();
  Register RHS = MI.getOperand(3).getReg();

  auto invertLittle64Cond = [](Little64CC::CondCode C) {
    switch (C) {
    case Little64CC::EQ:
      return Little64CC::NE;
    case Little64CC::NE:
      return Little64CC::EQ;
    case Little64CC::LT:
      return Little64CC::GE;
    case Little64CC::GE:
      return Little64CC::LT;
    case Little64CC::GT:
      return Little64CC::LE;
    case Little64CC::LE:
      return Little64CC::GT;
    case Little64CC::ULT:
      return Little64CC::UGE;
    case Little64CC::UGE:
      return Little64CC::ULT;
    case Little64CC::UGT:
      return Little64CC::ULE;
    case Little64CC::ULE:
      return Little64CC::UGT;
    }
    llvm_unreachable("unknown Little64 condition code");
  };

  auto canonicalizeSetccSelectRegs = [&](Little64CC::CondCode &TCC,
                                         Register &TrueReg,
                                         Register &FalseReg) {
    switch (TCC) {
    case Little64CC::NE:
    case Little64CC::GE:
    case Little64CC::LE:
    case Little64CC::UGE:
    case Little64CC::ULE:
      TCC = invertLittle64Cond(TCC);
      std::swap(TrueReg, FalseReg);
      break;
    default:
      break;
    }
  };

  MachineBasicBlock::iterator InsertPos = MI.getIterator();
  MachineFunction &MF = *MBB->getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  // Materialize boolean constants before scheduling and keep compare+select
  // itself as a SETCC_SELECT pseudo until late pre-emit expansion.
  Register FalseReg = MRI.createVirtualRegister(&Little64::GPRRegClass);
  Register TrueReg = MRI.createVirtualRegister(&Little64::GPRRegClass);
  BuildMI(*MBB, InsertPos, DL, TII->get(Little64::LDI), FalseReg).addImm(0);
  BuildMI(*MBB, InsertPos, DL, TII->get(Little64::LDI), TrueReg).addImm(1);

    Little64CC::CondCode TCC = static_cast<Little64CC::CondCode>(CC);
    canonicalizeSetccSelectRegs(TCC, TrueReg, FalseReg);

  BuildMI(*MBB, InsertPos, DL, TII->get(Little64::SETCC_SELECT), Dst)
      .addImm(static_cast<int64_t>(TCC))
      .addReg(LHS)
      .addReg(RHS)
      .addReg(TrueReg)
      .addReg(FalseReg);

  MI.eraseFromParent();
  return MBB;
}

Register Little64TargetLowering::getRegisterByName(const char *RegName, LLT VT,
                                                   const MachineFunction &MF) const {
  std::string Name = StringRef(RegName).lower();
  Register Reg = StringSwitch<Register>(Name)
    .Case("r0", Little64::R0)
    .Case("r1", Little64::R1)
    .Case("r2", Little64::R2)
    .Case("r3", Little64::R3)
    .Case("r4", Little64::R4)
    .Case("r5", Little64::R5)
    .Case("r6", Little64::R6)
    .Case("r7", Little64::R7)
    .Case("r8", Little64::R8)
    .Case("r9", Little64::R9)
    .Case("r10", Little64::R10)
    .Case("r11", Little64::R11)
    .Case("r12", Little64::R12)
    .Case("r13", Little64::R13)
    .Case("sp",  Little64::R13)
    .Case("r14", Little64::R14)
    .Case("lr",  Little64::R14)
    .Case("r15", Little64::R15)
    .Case("pc",  Little64::R15)
    .Default(0);

  if (Reg)
    return Reg;

  report_fatal_error("Invalid register name global variable");
}

std::pair<unsigned, const TargetRegisterClass *>
Little64TargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                     StringRef Constraint,
                                                     MVT VT) const {
  if (Constraint.size() == 1) {
    // Standard constraints like 'r'
    switch (Constraint[0]) {
    case 'r':
      return std::make_pair(0U, &Little64::GPRRegClass);
    default:
      break;
    }
  }

  // Handle specific register names (e.g. {r5})
  if (Constraint.starts_with("{") && Constraint.ends_with("}")) {
    StringRef RegName = Constraint.substr(1, Constraint.size() - 2);
    unsigned Reg = StringSwitch<unsigned>(RegName.lower())
                       .Case("r0", Little64::R0)
                       .Case("r1", Little64::R1)
                       .Case("r2", Little64::R2)
                       .Case("r3", Little64::R3)
                       .Case("r4", Little64::R4)
                       .Case("r5", Little64::R5)
                       .Case("r6", Little64::R6)
                       .Case("r7", Little64::R7)
                       .Case("r8", Little64::R8)
                       .Case("r9", Little64::R9)
                       .Case("r10", Little64::R10)
                       .Case("r11", Little64::R11)
                       .Case("r12", Little64::R12)
                       .Case("r13", Little64::R13)
                       .Case("sp", Little64::R13)
                       .Case("r14", Little64::R14)
                       .Case("lr", Little64::R14)
                       .Case("r15", Little64::R15)
                       .Case("pc", Little64::R15)
                       .Default(0);
    if (Reg)
      return std::make_pair(Reg, &Little64::GPRRegClass);
  }

  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}
