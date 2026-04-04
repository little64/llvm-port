//===-- ArchitectureLittle64.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/Architecture/Little64/ArchitectureLittle64.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Utility/ArchSpec.h"

using namespace lldb_private;

LLDB_PLUGIN_DEFINE(ArchitectureLittle64)

void ArchitectureLittle64::Initialize() {
  PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                "Little64-specific algorithms",
                                &ArchitectureLittle64::Create);
}

void ArchitectureLittle64::Terminate() {
  PluginManager::UnregisterPlugin(&ArchitectureLittle64::Create);
}

std::unique_ptr<Architecture>
ArchitectureLittle64::Create(const ArchSpec &arch) {
  if (arch.GetMachine() != llvm::Triple::little64)
    return nullptr;
  return std::unique_ptr<Architecture>(new ArchitectureLittle64());
}
