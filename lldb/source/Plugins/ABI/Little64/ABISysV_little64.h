//===-- ABISysV_little64.h -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_ABI_LITTLE64_ABISYSV_LITTLE64_H
#define LLDB_SOURCE_PLUGINS_ABI_LITTLE64_ABISYSV_LITTLE64_H

#include "lldb/Target/ABI.h"
#include "lldb/lldb-private.h"

class ABISysV_little64 : public lldb_private::RegInfoBasedABI {
public:
  ~ABISysV_little64() override = default;

  size_t GetRedZoneSize() const override { return 0; }

  bool PrepareTrivialCall(lldb_private::Thread &thread, lldb::addr_t sp,
                          lldb::addr_t functionAddress,
                          lldb::addr_t returnAddress,
                          llvm::ArrayRef<lldb::addr_t> args) const override;

  bool GetArgumentValues(lldb_private::Thread &thread,
                         lldb_private::ValueList &values) const override;

  lldb_private::Status
  SetReturnValueObject(lldb::StackFrameSP &frame_sp,
                       lldb::ValueObjectSP &new_value) override;

  lldb::ValueObjectSP
  GetReturnValueObjectImpl(lldb_private::Thread &thread,
                           lldb_private::CompilerType &type) const override;

  lldb::UnwindPlanSP CreateFunctionEntryUnwindPlan() override;

  lldb::UnwindPlanSP CreateDefaultUnwindPlan() override;

  bool RegisterIsVolatile(const lldb_private::RegisterInfo *reg_info) override;

  bool CallFrameAddressIsValid(lldb::addr_t cfa) override {
    return cfa != 0 && (cfa & 0x7ull) == 0;
  }

  bool CodeAddressIsValid(lldb::addr_t pc) override {
    return (pc & 0x1ull) == 0;
  }

  const lldb_private::RegisterInfo *
  GetRegisterInfoArray(uint32_t &count) override;

  uint64_t GetStackFrameSize() override { return 1024; }

  static void Initialize();
  static void Terminate();

  static lldb::ABISP CreateInstance(lldb::ProcessSP process_sp,
                                    const lldb_private::ArchSpec &arch);

  static llvm::StringRef GetPluginNameStatic() { return "sysv-little64"; }

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

private:
  using lldb_private::RegInfoBasedABI::RegInfoBasedABI;

  bool RegisterIsCalleeSaved(const lldb_private::RegisterInfo *reg_info) const;
};

#endif // LLDB_SOURCE_PLUGINS_ABI_LITTLE64_ABISYSV_LITTLE64_H
