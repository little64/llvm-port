add_llvm_component_library(LLVMLittle64Info
  Little64TargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  Little64
)
