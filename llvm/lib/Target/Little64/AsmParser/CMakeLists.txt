add_llvm_component_library(LLVMLittle64AsmParser
  Little64AsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  Little64Desc
  Little64Info
  Support
  TargetParser

  ADD_TO_COMPONENT
  Little64
)
