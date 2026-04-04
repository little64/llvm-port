add_llvm_component_library(LLVMLittle64Disassembler
  Little64Disassembler.cpp

  LINK_COMPONENTS
  MCDisassembler
  Little64Desc
  Little64Info
  Support
  TargetParser

  ADD_TO_COMPONENT
  Little64
)
