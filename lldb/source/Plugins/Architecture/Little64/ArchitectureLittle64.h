//===-- ArchitectureLittle64.h ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_ARCHITECTURE_LITTLE64_ARCHITECTURELITTLE64_H
#define LLDB_SOURCE_PLUGINS_ARCHITECTURE_LITTLE64_ARCHITECTURELITTLE64_H

#include "lldb/Core/Architecture.h"

namespace lldb_private {

class ArchitectureLittle64 : public Architecture {
public:
  static llvm::StringRef GetPluginNameStatic() { return "little64"; }
  static void Initialize();
  static void Terminate();

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

  void OverrideStopInfo(Thread &thread) const override {}

private:
  static std::unique_ptr<Architecture> Create(const ArchSpec &arch);
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_ARCHITECTURE_LITTLE64_ARCHITECTURELITTLE64_H
