; RUN: llc -mtriple=little64 -O1 < %s | FileCheck %s
; RUN: llc -mtriple=little64 -O2 < %s | FileCheck %s

declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly, ptr noalias readonly, i64, i1 immarg)

define void @copy_devinfo(ptr %dst_base, ptr %dev_info) {
entry:
  %is_null = icmp eq ptr %dev_info, null
  br i1 %is_null, label %exit, label %copy

copy:
  %dst = getelementptr i8, ptr %dst_base, i64 16
  call void @llvm.memcpy.p0.p0.i64(ptr %dst, ptr %dev_info, i64 64, i1 false)
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: copy_devinfo:
; CHECK:       TEST
; CHECK:       JUMP.Z
; CHECK:       LDI     #64, R8
; CHECK:       JUMP    @+4
; CHECK:       .quad memcpy
; CHECK:       LOAD    @-5, R1
; CHECK:       MOVE    R15, R14+2
; CHECK-NEXT:  MOVE    R1, R15
; CHECK-NOT:   MOVE    R1, PC
