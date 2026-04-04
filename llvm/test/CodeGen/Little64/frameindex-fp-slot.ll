; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Verify FP-based local slots do not alias saved FP at [R11].

@G = dso_local global i64 0

declare i64 @mix_debug_value(i64, i64)

define i64 @fp_local_slot_test(i64 %seed) #0 {
entry:
  %tmp = alloca i64, align 8
  %v1 = call i64 @mix_debug_value(i64 %seed, i64 1)
  store i64 %v1, ptr %tmp, align 8
  %v2 = call i64 @mix_debug_value(i64 %v1, i64 2)
  %r  = load i64, ptr %tmp, align 8
  %sum = add i64 %v2, %r
  ret i64 %sum
}

attributes #0 = { "frame-pointer"="all" }

; CHECK-LABEL: fp_local_slot_test:
; CHECK: MOVE    R13, R11
; CHECK: STORE   [R11], R1
; CHECK: LDI     #8, R12
; CHECK: ADD     R11, R12
; CHECK: STORE   [R12], R10
; CHECK: LDI     #8, R12
; CHECK: ADD     R11, R12
; CHECK: LOAD    [R12], R2
