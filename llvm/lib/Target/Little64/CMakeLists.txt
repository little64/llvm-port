add_llvm_component_group(Little64)

set(LLVM_TARGET_DEFINITIONS Little64.td)

tablegen(LLVM Little64GenAsmMatcher.inc    -gen-asm-matcher)
tablegen(LLVM Little64GenAsmWriter.inc     -gen-asm-writer)
tablegen(LLVM Little64GenCallingConv.inc   -gen-callingconv)
tablegen(LLVM Little64GenDAGISel.inc       -gen-dag-isel)
tablegen(LLVM Little64GenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM Little64GenInstrInfo.inc     -gen-instr-info)
tablegen(LLVM Little64GenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM Little64GenRegisterInfo.inc  -gen-register-info)
tablegen(LLVM Little64GenSDNodeInfo.inc    -gen-sd-node-info)
tablegen(LLVM Little64GenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(Little64CommonTableGen)

add_llvm_target(Little64CodeGen
  Little64AsmPrinter.cpp
  Little64CallDebugLocFix.cpp
  Little64ExpandPseudos.cpp
  Little64FrameLowering.cpp
  Little64InstrInfo.cpp
  Little64ISelDAGToDAG.cpp
  Little64ISelLowering.cpp
  Little64MachineFunctionInfo.cpp
  Little64MCInstLower.cpp
  Little64RegisterInfo.cpp
  Little64SelectionDAGInfo.cpp
  Little64Subtarget.cpp
  Little64TargetMachine.cpp
  Little64TargetObjectFile.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  Little64AsmParser
  Little64Desc
  Little64Info
  MC
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  Little64
  )

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
