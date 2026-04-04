; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Ensure indirect calls through function pointers use register-target CALL
; lowering (no literal slot materialization in the call sequence itself).

; CHECK-LABEL: call_ptr:
; CHECK-NOT: .quad
; CHECK: MOVE    R15, R14+2
; CHECK: MOVE    R{{[0-9]+}}, PC

define i64 @call_ptr(ptr %fn, i64 %x) {
entry:
  %r = call i64 %fn(i64 %x)
  ret i64 %r
}
