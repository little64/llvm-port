; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Inspired by vararg legalization tests in other targets.

; CHECK-LABEL: first_vararg:
; CHECK: ADD
; CHECK: STORE
; CHECK: LOAD

define i64 @first_vararg(i64 %fixed, ...) {
entry:
  %ap = alloca ptr, align 8
  call void @llvm.va_start(ptr %ap)
  %listp = load ptr, ptr %ap, align 8
  %argp = getelementptr i8, ptr %listp, i64 8
  store ptr %argp, ptr %ap, align 8
  %v = load i64, ptr %listp, align 8
  call void @llvm.va_end(ptr %ap)
  ret i64 %v
}

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)
