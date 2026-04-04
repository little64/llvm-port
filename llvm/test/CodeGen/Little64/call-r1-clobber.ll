; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Ensure repeated direct calls cannot reuse R1 (return register) as a preserved
; call-target register across call boundaries.
;
; CHECK-LABEL: run_debug_fixture_style:
; CHECK: LOAD    @-5, R1
; CHECK: STORE   [R13], R1
; CHECK: MOVE    R1, R15
; CHECK: MOVE    R1, R10
; CHECK: LOAD    [R13], R1
; CHECK: MOVE    R1, R15
; CHECK: MOVE    R1, R10
; CHECK: LOAD    [R13], R1
; CHECK: MOVE    R1, R15

declare i64 @mix_debug_value(i64, i64)

define i64 @run_debug_fixture_style(i64 %seed) {
entry:
  %v1 = call i64 @mix_debug_value(i64 %seed, i64 1)
  %v2 = call i64 @mix_debug_value(i64 %v1, i64 2)
  %v3 = call i64 @mix_debug_value(i64 %v2, i64 3)
  ret i64 %v3
}
