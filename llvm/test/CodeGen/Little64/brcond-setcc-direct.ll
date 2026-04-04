; RUN: llc -mtriple=little64 -O2 < %s | FileCheck %s
;
; Repro for signed i32 branch conditions feeding BRCOND. These should lower
; directly to BRCC (TEST + JUMP.*) without materializing SETCC via CMOV.

; CHECK-LABEL: br_sge_i32_zero:
; CHECK: TEST
; CHECK-NOT: CMOV
; CHECK: JUMP

define i64 @br_sge_i32_zero(i64 %x) {
entry:
  %a = trunc i64 %x to i32
  %cmp = icmp sge i32 %a, 0
  br i1 %cmp, label %t, label %f

t:
  ret i64 1

f:
  ret i64 0
}

; CHECK-LABEL: br_sge_i32_masked:
; CHECK: TEST
; CHECK-NOT: CMOV
; CHECK: JUMP

define i64 @br_sge_i32_masked(i64 %x) {
entry:
  %masked = and i64 %x, 4294967295
  %a = trunc i64 %masked to i32
  %cmp = icmp sge i32 %a, 0
  br i1 %cmp, label %t, label %f

t:
  ret i64 1

f:
  ret i64 0
}
