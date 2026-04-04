; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Direct va_arg lowering coverage for Little64.

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)

; CHECK-LABEL: vaarg_direct_i64:
; CHECK: STORE   [R13], R1
; CHECK: LOAD    [R12], R1
define i64 @vaarg_direct_i64(i64 %fixed, ...) {
entry:
  %ap = alloca ptr, align 8
  call void @llvm.va_start(ptr %ap)
  %v = va_arg ptr %ap, i64
  call void @llvm.va_end(ptr %ap)
  ret i64 %v
}

; CHECK-LABEL: vaarg_direct_two_i64:
; CHECK: AND     R2, R1
; CHECK: LDI     #8, R3
; CHECK: ADD     R3, R2
; CHECK: STORE   [R13], R2
; CHECK: LOAD    [R1], R2
; CHECK: LOAD    [R12], R1
; CHECK: ADD     R2, R1
define i64 @vaarg_direct_two_i64(i64 %fixed, ...) {
entry:
  %ap = alloca ptr, align 8
  call void @llvm.va_start(ptr %ap)
  %a = va_arg ptr %ap, i64
  %b = va_arg ptr %ap, i64
  %s = add i64 %a, %b
  call void @llvm.va_end(ptr %ap)
  ret i64 %s
}

; CHECK-LABEL: vaarg_direct_i32_pair:
; CHECK: LDI     #32, [[SH:R[0-9]+]]
; CHECK: SLL     [[SH]], R1
; CHECK: SRA     [[SH]], R1
; CHECK: WORD_LOAD
; CHECK: ADD     R2, R1
define i64 @vaarg_direct_i32_pair(i64 %fixed, ...) {
entry:
  %ap = alloca ptr, align 8
  call void @llvm.va_start(ptr %ap)
  %a = va_arg ptr %ap, i32
  %b = va_arg ptr %ap, i32
  %sa = sext i32 %a to i64
  %zb = zext i32 %b to i64
  %s = add i64 %sa, %zb
  call void @llvm.va_end(ptr %ap)
  ret i64 %s
}