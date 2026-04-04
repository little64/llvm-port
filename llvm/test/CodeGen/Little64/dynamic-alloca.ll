; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Regression coverage for DYNAMIC_STACKALLOC, STACKSAVE, and STACKRESTORE.
; These default to Legal in LLVM's base TargetLowering so must be explicitly
; registered as Expand.  R13 is the Little64 stack pointer.
;
;   DYNAMIC_STACKALLOC → AND (align) + SUB (adjust R13) + copy new R13 out
;   STACKSAVE          → MOVE R13, <Rd>  (copy SP out)
;   STACKRESTORE       → MOVE <Rs>, R13  (copy SP in)

; CHECK-LABEL: test_dynamic_alloca:
; CHECK: AND
; CHECK: SUB
; CHECK: MOVE R14, PC
define ptr @test_dynamic_alloca(i64 %n) {
  %p = alloca i8, i64 %n, align 8
  ret ptr %p
}

; STACKSAVE emits MOVE R13, <reg> and STACKRESTORE emits MOVE <reg>, R13.
; Both must appear for the save/restore round-trip.
; CHECK-LABEL: test_stacksave_restore:
; CHECK: MOVE R13,
; CHECK: MOVE {{R[0-9]+}}, R13
; CHECK: MOVE R14, PC
define void @test_stacksave_restore(i64 %n) {
  %sp = call ptr @llvm.stacksave.p0()
  %p  = alloca i8, i64 %n
  call void @llvm.stackrestore.p0(ptr %sp)
  ret void
}

declare ptr @llvm.stacksave.p0()
declare void @llvm.stackrestore.p0(ptr)
