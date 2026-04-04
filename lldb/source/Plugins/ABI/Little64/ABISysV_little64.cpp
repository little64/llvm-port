//===-- ABISysV_little64.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABISysV_little64.h"

#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Value.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/RegisterValue.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "llvm/TargetParser/Triple.h"

using namespace lldb;
using namespace lldb_private;

static constexpr size_t g_little64_num_arg_registers = 5;
static constexpr size_t g_little64_stack_arg_slot_size = 8;

LLDB_PLUGIN_DEFINE_ADV(ABISysV_little64, ABILittle64)

namespace dwarf {
enum regnums {
  r0 = 0,
  r1 = 1,
  r2 = 2,
  r3 = 3,
  r4 = 4,
  r5 = 5,
  r6 = 6,
  r7 = 7,
  r8 = 8,
  r9 = 9,
  r10 = 10,
  r11 = 11,
  r12 = 12,
  sp = 13,
  lr = 14,
  pc = 15,
  flags = 16,
  sr0 = 17,
  sr1 = 18,
  sr2 = 19,
  sr3 = 20,
  sr4 = 21,
  sr5 = 22,
  sr6 = 23,
  sr7 = 24,
  sr8 = 25,
  sr9 = 26,
  sr10 = 27,
  sr11 = 28,
  sr12 = 29,
  sr13 = 30,
  sr14 = 31,
  sr15 = 32,
  sr16 = 33,
};
}

static const RegisterInfo g_register_infos[] = {
    {"r0", nullptr, 8, 0, eEncodingUint, eFormatHex,
     {dwarf::r0, dwarf::r0, LLDB_INVALID_REGNUM, 0, 0},
     nullptr, nullptr, nullptr},
    {"r1", nullptr, 8, 8, eEncodingUint, eFormatHex,
     {dwarf::r1, dwarf::r1, LLDB_INVALID_REGNUM, 1, 1},
     nullptr, nullptr, nullptr},
    {"r2", nullptr, 8, 16, eEncodingUint, eFormatHex,
     {dwarf::r2, dwarf::r2, LLDB_INVALID_REGNUM, 2, 2},
     nullptr, nullptr, nullptr},
    {"r3", nullptr, 8, 24, eEncodingUint, eFormatHex,
     {dwarf::r3, dwarf::r3, LLDB_INVALID_REGNUM, 3, 3},
     nullptr, nullptr, nullptr},
    {"r4", nullptr, 8, 32, eEncodingUint, eFormatHex,
     {dwarf::r4, dwarf::r4, LLDB_INVALID_REGNUM, 4, 4},
     nullptr, nullptr, nullptr},
    {"r5", nullptr, 8, 40, eEncodingUint, eFormatHex,
     {dwarf::r5, dwarf::r5, LLDB_INVALID_REGNUM, 5, 5},
     nullptr, nullptr, nullptr},
    {"r6", nullptr, 8, 48, eEncodingUint, eFormatHex,
     {dwarf::r6, dwarf::r6, LLDB_REGNUM_GENERIC_ARG5, 6, 6},
     nullptr, nullptr, nullptr},
    {"r7", nullptr, 8, 56, eEncodingUint, eFormatHex,
     {dwarf::r7, dwarf::r7, LLDB_REGNUM_GENERIC_ARG4, 7, 7},
     nullptr, nullptr, nullptr},
    {"r8", nullptr, 8, 64, eEncodingUint, eFormatHex,
     {dwarf::r8, dwarf::r8, LLDB_REGNUM_GENERIC_ARG3, 8, 8},
     nullptr, nullptr, nullptr},
    {"r9", nullptr, 8, 72, eEncodingUint, eFormatHex,
     {dwarf::r9, dwarf::r9, LLDB_REGNUM_GENERIC_ARG2, 9, 9},
     nullptr, nullptr, nullptr},
    {"r10", nullptr, 8, 80, eEncodingUint, eFormatHex,
     {dwarf::r10, dwarf::r10, LLDB_REGNUM_GENERIC_ARG1, 10, 10},
     nullptr, nullptr, nullptr},
    {"r11", "fp", 8, 88, eEncodingUint, eFormatHex,
     {dwarf::r11, dwarf::r11, LLDB_REGNUM_GENERIC_FP, 11, 11},
     nullptr, nullptr, nullptr},
    {"r12", nullptr, 8, 96, eEncodingUint, eFormatHex,
     {dwarf::r12, dwarf::r12, LLDB_INVALID_REGNUM, 12, 12},
     nullptr, nullptr, nullptr},
    {"sp", "r13", 8, 104, eEncodingUint, eFormatHex,
     {dwarf::sp, dwarf::sp, LLDB_REGNUM_GENERIC_SP, 13, 13},
     nullptr, nullptr, nullptr},
    {"lr", "r14", 8, 112, eEncodingUint, eFormatHex,
     {dwarf::lr, dwarf::lr, LLDB_REGNUM_GENERIC_RA, 14, 14},
     nullptr, nullptr, nullptr},
    {"pc", "r15", 8, 120, eEncodingUint, eFormatHex,
     {dwarf::pc, dwarf::pc, LLDB_REGNUM_GENERIC_PC, 15, 15},
     nullptr, nullptr, nullptr},
    {"flags", nullptr, 8, 128, eEncodingUint, eFormatHex,
     {dwarf::flags, dwarf::flags, LLDB_REGNUM_GENERIC_FLAGS, 16, 16},
     nullptr, nullptr, nullptr},
    {"cpu_control", "sr0", 8, 136, eEncodingUint, eFormatHex,
     {dwarf::sr0, dwarf::sr0, LLDB_INVALID_REGNUM, 17, 17},
     nullptr, nullptr, nullptr},
    {"interrupt_table_base", "sr1", 8, 144, eEncodingUint, eFormatHex,
     {dwarf::sr1, dwarf::sr1, LLDB_INVALID_REGNUM, 18, 18},
     nullptr, nullptr, nullptr},
    {"interrupt_mask", "sr2", 8, 152, eEncodingUint, eFormatHex,
     {dwarf::sr2, dwarf::sr2, LLDB_INVALID_REGNUM, 19, 19},
     nullptr, nullptr, nullptr},
    {"interrupt_states", "sr3", 8, 160, eEncodingUint, eFormatHex,
     {dwarf::sr3, dwarf::sr3, LLDB_INVALID_REGNUM, 20, 20},
     nullptr, nullptr, nullptr},
    {"interrupt_epc", "sr4", 8, 168, eEncodingUint, eFormatHex,
     {dwarf::sr4, dwarf::sr4, LLDB_INVALID_REGNUM, 21, 21},
     nullptr, nullptr, nullptr},
    {"interrupt_eflags", "sr5", 8, 176, eEncodingUint, eFormatHex,
     {dwarf::sr5, dwarf::sr5, LLDB_INVALID_REGNUM, 22, 22},
     nullptr, nullptr, nullptr},
    {"trap_cause", "sr6", 8, 184, eEncodingUint, eFormatHex,
     {dwarf::sr6, dwarf::sr6, LLDB_INVALID_REGNUM, 23, 23},
     nullptr, nullptr, nullptr},
    {"trap_fault_addr", "sr7", 8, 192, eEncodingUint, eFormatHex,
     {dwarf::sr7, dwarf::sr7, LLDB_INVALID_REGNUM, 24, 24},
     nullptr, nullptr, nullptr},
    {"trap_access", "sr8", 8, 200, eEncodingUint, eFormatHex,
     {dwarf::sr8, dwarf::sr8, LLDB_INVALID_REGNUM, 25, 25},
     nullptr, nullptr, nullptr},
    {"trap_pc", "sr9", 8, 208, eEncodingUint, eFormatHex,
     {dwarf::sr9, dwarf::sr9, LLDB_INVALID_REGNUM, 26, 26},
     nullptr, nullptr, nullptr},
    {"trap_aux", "sr10", 8, 216, eEncodingUint, eFormatHex,
     {dwarf::sr10, dwarf::sr10, LLDB_INVALID_REGNUM, 27, 27},
     nullptr, nullptr, nullptr},
    {"page_table_root_physical", "sr11", 8, 224, eEncodingUint, eFormatHex,
     {dwarf::sr11, dwarf::sr11, LLDB_INVALID_REGNUM, 28, 28},
     nullptr, nullptr, nullptr},
    {"boot_info_frame_physical", "sr12", 8, 232, eEncodingUint, eFormatHex,
     {dwarf::sr12, dwarf::sr12, LLDB_INVALID_REGNUM, 29, 29},
     nullptr, nullptr, nullptr},
    {"sr13", "boot_source_page_size", 8, 240, eEncodingUint, eFormatHex,
     {dwarf::sr13, dwarf::sr13, LLDB_INVALID_REGNUM, 30, 30},
     nullptr, nullptr, nullptr},
    {"sr14", "boot_source_page_count", 8, 248, eEncodingUint, eFormatHex,
     {dwarf::sr14, dwarf::sr14, LLDB_INVALID_REGNUM, 31, 31},
     nullptr, nullptr, nullptr},
    {"sr15", "hypercall_caps", 8, 256, eEncodingUint, eFormatHex,
     {dwarf::sr15, dwarf::sr15, LLDB_INVALID_REGNUM, 32, 32},
     nullptr, nullptr, nullptr},
    {"sr16", "interrupt_cpu_control", 8, 264, eEncodingUint, eFormatHex,
     {dwarf::sr16, dwarf::sr16, LLDB_INVALID_REGNUM, 33, 33},
     nullptr, nullptr, nullptr},
};

ABISP ABISysV_little64::CreateInstance(ProcessSP process_sp,
                                       const ArchSpec &arch) {
  if (arch.GetTriple().getArch() != llvm::Triple::little64)
    return {};
  return ABISP(new ABISysV_little64(std::move(process_sp),
                                    MakeMCRegisterInfo(arch)));
}

const RegisterInfo *ABISysV_little64::GetRegisterInfoArray(uint32_t &count) {
  count = std::size(g_register_infos);
  return g_register_infos;
}

bool ABISysV_little64::PrepareTrivialCall(Thread &thread, addr_t sp,
                                          addr_t functionAddress,
                                          addr_t returnAddress,
                                          llvm::ArrayRef<addr_t> args) const {
  RegisterContextSP reg_ctx_sp = thread.GetRegisterContext();
  if (!reg_ctx_sp)
    return false;

  ProcessSP process_sp = thread.GetProcess();
  if (!process_sp)
    return false;

  const size_t num_reg_args =
      std::min(args.size(), g_little64_num_arg_registers);
  for (size_t idx = 0; idx < num_reg_args; ++idx) {
    const addr_t arg = args[idx];
    const RegisterInfo *reg_info = reg_ctx_sp->GetRegisterInfo(
        eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1 + idx);
    if (!reg_info || !reg_ctx_sp->WriteRegisterFromUnsigned(reg_info, arg))
      return false;
  }

  addr_t call_sp = sp;
  const size_t num_stack_args = args.size() - num_reg_args;
  if (num_stack_args > 0) {
    call_sp -= num_stack_args * g_little64_stack_arg_slot_size;

    for (size_t stack_idx = 0; stack_idx < num_stack_args; ++stack_idx) {
      const uint64_t raw = args[num_reg_args + stack_idx];
      uint8_t encoded[g_little64_stack_arg_slot_size];
      for (size_t byte_idx = 0; byte_idx < g_little64_stack_arg_slot_size;
           ++byte_idx) {
        encoded[byte_idx] =
            static_cast<uint8_t>((raw >> (byte_idx * 8)) & 0xffu);
      }

      const addr_t slot_addr =
          call_sp + stack_idx * g_little64_stack_arg_slot_size;
      Status error;
      const size_t written = process_sp->WriteMemory(
          slot_addr, encoded, g_little64_stack_arg_slot_size, error);
      if (written != g_little64_stack_arg_slot_size || error.Fail())
        return false;
    }
  }

  const RegisterInfo *pc_reg =
      reg_ctx_sp->GetRegisterInfo(eRegisterKindGeneric, LLDB_REGNUM_GENERIC_PC);
  const RegisterInfo *sp_reg =
      reg_ctx_sp->GetRegisterInfo(eRegisterKindGeneric, LLDB_REGNUM_GENERIC_SP);
  const RegisterInfo *ra_reg =
      reg_ctx_sp->GetRegisterInfo(eRegisterKindGeneric, LLDB_REGNUM_GENERIC_RA);

  if (!pc_reg || !sp_reg || !ra_reg)
    return false;

  return reg_ctx_sp->WriteRegisterFromUnsigned(pc_reg, functionAddress) &&
         reg_ctx_sp->WriteRegisterFromUnsigned(sp_reg, call_sp) &&
         reg_ctx_sp->WriteRegisterFromUnsigned(ra_reg, returnAddress);
}

bool ABISysV_little64::GetArgumentValues(Thread &thread,
                                         ValueList &values) const {
  RegisterContextSP reg_ctx_sp = thread.GetRegisterContext();
  if (!reg_ctx_sp)
    return false;

  ProcessSP process_sp = thread.GetProcess();
  if (!process_sp)
    return false;

  const addr_t sp = reg_ctx_sp->GetSP(0);
  if (!sp)
    return false;

  const auto assign_scalar = [](Scalar &scalar, uint64_t raw, uint64_t bit_size,
                                bool is_signed) {
    if (bit_size <= 8) {
      scalar = is_signed ? static_cast<int8_t>(raw) : static_cast<uint8_t>(raw);
    } else if (bit_size <= 16) {
      scalar =
          is_signed ? static_cast<int16_t>(raw) : static_cast<uint16_t>(raw);
    } else if (bit_size <= 32) {
      scalar =
          is_signed ? static_cast<int32_t>(raw) : static_cast<uint32_t>(raw);
    } else {
      scalar =
          is_signed ? static_cast<int64_t>(raw) : static_cast<uint64_t>(raw);
    }
  };

  const size_t num_values = values.GetSize();
  for (size_t value_index = 0; value_index < num_values; ++value_index) {
    Value *value = values.GetValueAtIndex(value_index);
    if (!value)
      return false;

    CompilerType compiler_type = value->GetCompilerType();
    std::optional<uint64_t> bit_size =
        llvm::expectedToOptional(compiler_type.GetBitSize(&thread));
    if (!bit_size || *bit_size == 0 || *bit_size > 64)
      return false;

    bool is_signed = false;
    const bool is_integer = compiler_type.IsIntegerOrEnumerationType(is_signed);
    const bool is_pointer = compiler_type.IsPointerType();
    if (!is_integer && !is_pointer)
      continue;

    uint64_t raw = 0;
    if (value_index < g_little64_num_arg_registers) {
      const RegisterInfo *reg_info = reg_ctx_sp->GetRegisterInfo(
          eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1 + value_index);
      if (!reg_info)
        return false;
      raw = reg_ctx_sp->ReadRegisterAsUnsigned(reg_info, 0);
    } else {
      const size_t stack_arg_index = value_index - g_little64_num_arg_registers;
      const addr_t arg_addr = sp + stack_arg_index * g_little64_stack_arg_slot_size;

      uint8_t encoded[g_little64_stack_arg_slot_size] = {0};
      Status error;
      const size_t bytes_read = process_sp->ReadMemory(
          arg_addr, encoded, g_little64_stack_arg_slot_size, error);
      if (bytes_read != g_little64_stack_arg_slot_size || error.Fail())
        return false;

      for (size_t byte_idx = 0; byte_idx < g_little64_stack_arg_slot_size;
           ++byte_idx) {
        raw |= static_cast<uint64_t>(encoded[byte_idx]) << (byte_idx * 8);
      }
    }

    assign_scalar(value->GetScalar(), raw, *bit_size, is_integer && is_signed);
  }

  return true;
}

Status ABISysV_little64::SetReturnValueObject(StackFrameSP &frame_sp,
                                              ValueObjectSP &new_value) {
  Status result;
  if (!new_value)
    return Status::FromErrorString("Empty value object for return value.");

  CompilerType compiler_type = new_value->GetCompilerType();
  if (!compiler_type)
    return Status::FromErrorString("Null compiler type for return value.");

  bool is_signed = false;
  if (!compiler_type.IsIntegerOrEnumerationType(is_signed) &&
      !compiler_type.IsPointerType()) {
    return Status::FromErrorString(
        "Only integer/enumeration/pointer return values are currently supported for little64 ABI");
  }

  DataExtractor data;
  const size_t num_bytes = new_value->GetData(data, result);
  if (result.Fail())
    return Status::FromErrorStringWithFormat(
        "Couldn't convert return value to raw data: %s", result.AsCString());

  if (num_bytes == 0 || num_bytes > 16) {
    return Status::FromErrorString(
        "little64 ABI currently supports return values up to 16 bytes");
  }

  auto &reg_ctx = *frame_sp->GetThread()->GetRegisterContext();
  const RegisterInfo *reg_info_r1 =
      reg_ctx.GetRegisterInfo(eRegisterKindDWARF, dwarf::r1);
  const RegisterInfo *reg_info_r2 =
      reg_ctx.GetRegisterInfo(eRegisterKindDWARF, dwarf::r2);
  if (!reg_info_r1 || !reg_info_r2)
    return Status::FromErrorString(
        "Failed to resolve return registers for little64 ABI");

  offset_t offset = 0;
  uint64_t low = data.GetMaxU64(&offset, std::min<size_t>(8, num_bytes));
  if (!reg_ctx.WriteRegisterFromUnsigned(reg_info_r1, low)) {
    return Status::FromErrorStringWithFormat(
          "Couldn't write value to register %s", reg_info_r1->name);
  }

  if (num_bytes > 8) {
    uint64_t high = data.GetMaxU64(&offset, num_bytes - 8);
    if (!reg_ctx.WriteRegisterFromUnsigned(reg_info_r2, high)) {
      return Status::FromErrorStringWithFormat(
          "Couldn't write value to register %s", reg_info_r2->name);
    }
  }

  return result;
}

ValueObjectSP ABISysV_little64::GetReturnValueObjectImpl(
    Thread &thread, CompilerType &type) const {
  if (!type)
    return {};

  RegisterContextSP reg_ctx = thread.GetRegisterContext();
  if (!reg_ctx)
    return {};

  const uint32_t type_flags = type.GetTypeInfo();
  const size_t byte_size =
      llvm::expectedToOptional(type.GetByteSize(&thread)).value_or(0);
  if (byte_size == 0 || byte_size > 16)
    return {};

  const RegisterInfo *reg_info_r1 =
      reg_ctx->GetRegisterInfo(eRegisterKindDWARF, dwarf::r1);
  const RegisterInfo *reg_info_r2 =
      reg_ctx->GetRegisterInfo(eRegisterKindDWARF, dwarf::r2);
  if (!reg_info_r1 || !reg_info_r2)
    return {};

  Value value;
  value.SetCompilerType(type);

  if ((type_flags & eTypeIsInteger) || (type_flags & eTypeIsPointer)) {
    uint64_t raw = reg_ctx->ReadRegisterAsUnsigned(reg_info_r1, 0);

    if (byte_size <= 8) {
      bool is_signed = (type_flags & eTypeIsSigned) != 0;
      switch (byte_size) {
      case 1:
        value.GetScalar() =
            is_signed ? static_cast<int8_t>(raw) : static_cast<uint8_t>(raw);
        break;
      case 2:
        value.GetScalar() = is_signed ? static_cast<int16_t>(raw)
                                      : static_cast<uint16_t>(raw);
        break;
      case 4:
        value.GetScalar() = is_signed ? static_cast<int32_t>(raw)
                                      : static_cast<uint32_t>(raw);
        break;
      case 8:
      default:
        value.GetScalar() = is_signed ? static_cast<int64_t>(raw)
                                      : static_cast<uint64_t>(raw);
        break;
      }
      value.SetValueType(Value::ValueType::Scalar);
      return ValueObjectConstResult::Create(thread.GetStackFrameAtIndex(0).get(),
                                            value, ConstString(""));
    }

    uint64_t high = reg_ctx->ReadRegisterAsUnsigned(reg_info_r2, 0);
    std::unique_ptr<DataBufferHeap> heap_data_up(new DataBufferHeap(byte_size, 0));
    auto *buffer = heap_data_up->GetBytes();
    for (size_t i = 0; i < 8; ++i)
      buffer[i] = static_cast<uint8_t>((raw >> (i * 8)) & 0xffu);
    for (size_t i = 8; i < byte_size; ++i)
      buffer[i] = static_cast<uint8_t>((high >> ((i - 8) * 8)) & 0xffu);

    value.SetBytes(heap_data_up.release(), byte_size);
    return ValueObjectConstResult::Create(thread.GetStackFrameAtIndex(0).get(),
                                          value, ConstString(""));
  }

  return {};
}

UnwindPlanSP ABISysV_little64::CreateFunctionEntryUnwindPlan() {
  UnwindPlan::Row row;
  row.GetCFAValue().SetIsRegisterPlusOffset(dwarf::sp, 0);
  row.SetRegisterLocationToRegister(dwarf::pc, dwarf::lr, true);
  row.SetRegisterLocationToIsCFAPlusOffset(dwarf::sp, 0, true);

  auto plan_sp = std::make_shared<UnwindPlan>(eRegisterKindDWARF);
  plan_sp->AppendRow(std::move(row));
  plan_sp->SetSourceName("little64 at-func-entry default");
  plan_sp->SetSourcedFromCompiler(eLazyBoolNo);
  plan_sp->SetReturnAddressRegister(dwarf::lr);
  return plan_sp;
}

UnwindPlanSP ABISysV_little64::CreateDefaultUnwindPlan() {
  uint32_t fp_reg_num = LLDB_REGNUM_GENERIC_FP;
  uint32_t pc_reg_num = LLDB_REGNUM_GENERIC_PC;
  uint32_t sp_reg_num = LLDB_REGNUM_GENERIC_SP;
  uint32_t ra_reg_num = LLDB_REGNUM_GENERIC_RA;

  UnwindPlan::Row row;

  // Conservative FP-based fallback:
  //   [fp + 0] stores caller FP
  //   return address is carried in LR
  // Keep PC sourced from LR so we can unwind through frames that did not spill
  // LR at the exact slot layout expected by a strict memory-based plan.
  row.GetCFAValue().SetIsRegisterPlusOffset(fp_reg_num, 16);
  row.SetRegisterLocationToAtCFAPlusOffset(fp_reg_num, -16, true);
  row.SetRegisterLocationToRegister(pc_reg_num, ra_reg_num, true);
  row.SetRegisterLocationToIsCFAPlusOffset(sp_reg_num, 0, true);

  auto plan_sp = std::make_shared<UnwindPlan>(eRegisterKindGeneric);
  plan_sp->AppendRow(std::move(row));
  plan_sp->SetSourceName("little64 default unwind plan");
  plan_sp->SetSourcedFromCompiler(eLazyBoolNo);
  plan_sp->SetUnwindPlanValidAtAllInstructions(eLazyBoolNo);
  plan_sp->SetReturnAddressRegister(ra_reg_num);
  return plan_sp;
}

bool ABISysV_little64::RegisterIsCalleeSaved(const RegisterInfo *reg_info) const {
  if (!reg_info)
    return false;

  const llvm::StringRef name(reg_info->name);
  // Keep this aligned with llvm/lib/Target/Little64/Little64CallingConv.td.
  return name == "r11" || name == "lr";
}

bool ABISysV_little64::RegisterIsVolatile(const RegisterInfo *reg_info) {
  return !RegisterIsCalleeSaved(reg_info);
}

void ABISysV_little64::Initialize() {
  PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                "System V ABI for little64 targets",
                                CreateInstance);
}

void ABISysV_little64::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}
