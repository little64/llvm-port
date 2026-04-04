add_lldb_library(lldbPluginABILittle64 PLUGIN
  ABISysV_little64.cpp

  LINK_COMPONENTS
    Support
    TargetParser
  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
    lldbValueObject
  )
