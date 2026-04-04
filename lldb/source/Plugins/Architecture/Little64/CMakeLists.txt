add_lldb_library(lldbPluginArchitectureLittle64 PLUGIN
  ArchitectureLittle64.cpp

  LINK_COMPONENTS
    Support
  LINK_LIBS
    lldbCore
    lldbTarget
    lldbUtility
  )
