; RUN: llc -mtriple=little64 -O1 < %s | FileCheck %s
;
; Ensure non-leaf functions do not use two independent LR save mechanisms.
; We should see a single LR spill/reload path and no PUSH/POP of R14.
;
; CHECK-LABEL: caller:
; CHECK-NOT: PUSH    R14
; CHECK-COUNT-1: STORE   [R11], R14
; CHECK: MOVE    R15, R14+2
; CHECK: MOVE    R1, R15
; CHECK-COUNT-1: LOAD    [R11], R14
; CHECK-NOT: POP    R14

declare i64 @callee(i64)

define i64 @caller(i64 %x) #0 {
entry:
  %v = call i64 @callee(i64 %x)
  %r = add i64 %v, 1
  ret i64 %r
}

attributes #0 = { "frame-pointer"="all" }
