add_llvm_component_library(LLVMLittle64Desc
  Little64AsmBackend.cpp
  Little64ELFObjectWriter.cpp
  Little64InstPrinter.cpp
  Little64MCAsmInfo.cpp
  Little64MCCodeEmitter.cpp
  Little64MCTargetDesc.cpp

  LINK_COMPONENTS
  Little64Info
  MC
  MCDisassembler
  Support
  TargetParser

  ADD_TO_COMPONENT
  Little64
)
