; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Ensure LowerVASTART points va_list at the first variadic stack slot.
; For Little64 vararg functions, fixed args are also stack-assigned, so the
; start pointer advances by 8 bytes per fixed i64 argument.

declare void @llvm.va_start(ptr)

; CHECK-LABEL: vastart0:
; CHECK: LDI     #8, R12
; CHECK: ADD     R13, R12
; CHECK: MOVE    R12, R1
; CHECK: STORE   [R13], R1
define ptr @vastart0(...) {
entry:
  %ap = alloca ptr, align 8
  call void @llvm.va_start(ptr %ap)
  %p = load ptr, ptr %ap, align 8
  ret ptr %p
}

; CHECK-LABEL: vastart1:
; CHECK: LDI     #16, R12
; CHECK: ADD     R13, R12
; CHECK: MOVE    R12, R1
; CHECK: STORE   [R13], R1
define ptr @vastart1(i64 %a, ...) {
entry:
  %ap = alloca ptr, align 8
  call void @llvm.va_start(ptr %ap)
  %p = load ptr, ptr %ap, align 8
  ret ptr %p
}

; CHECK-LABEL: vastart2:
; CHECK: LDI     #24, R12
; CHECK: ADD     R13, R12
; CHECK: MOVE    R12, R1
; CHECK: STORE   [R13], R1
define ptr @vastart2(i64 %a, i64 %b, ...) {
entry:
  %ap = alloca ptr, align 8
  call void @llvm.va_start(ptr %ap)
  %p = load ptr, ptr %ap, align 8
  ret ptr %p
}

; CHECK-LABEL: vastart5:
; CHECK: LDI     #48, R12
; CHECK: ADD     R13, R12
; CHECK: MOVE    R12, R1
; CHECK: STORE   [R13], R1
define ptr @vastart5(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, ...) {
entry:
  %ap = alloca ptr, align 8
  call void @llvm.va_start(ptr %ap)
  %p = load ptr, ptr %ap, align 8
  ret ptr %p
}