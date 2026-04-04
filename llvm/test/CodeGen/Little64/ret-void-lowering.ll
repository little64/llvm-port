; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Ensure void returns still lower to LR-based return branch.

; CHECK-LABEL: ret_void:
; CHECK: MOVE    R14, PC

define void @ret_void() {
entry:
  ret void
}
