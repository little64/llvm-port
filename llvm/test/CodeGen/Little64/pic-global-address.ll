; RUN: llc -mtriple=little64 -relocation-model=pic -O2 < %s | FileCheck %s
;
; Verify that PIC global address lowering generates GOT-indirect access
; via a literal-slot sequence ending in a double LOAD.

@gvar = external global i64

; CHECK-LABEL: load_global_pic:
; CHECK:       JUMP    @+4
; CHECK-NEXT:  .quad   gvar@got
; CHECK-NEXT:  MOVE    @-5, R12
; CHECK-NEXT:  LOAD    [R12], R1
; CHECK-NEXT:  ADD     R12, R1
; CHECK-NEXT:  LOAD    [R1], R1
; CHECK-NEXT:  LOAD    [R1], R1

define i64 @load_global_pic() {
entry:
  %v = load i64, ptr @gvar
  ret i64 %v
}

; CHECK-LABEL: store_global_pic:
; CHECK:       JUMP    @+4
; CHECK-NEXT:  .quad   gvar@got
; CHECK-NEXT:  MOVE    @-5, R12

define void @store_global_pic(i64 %val) {
entry:
  store i64 %val, ptr @gvar
  ret void
}
