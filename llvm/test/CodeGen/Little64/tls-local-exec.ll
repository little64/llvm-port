; RUN: llc -mtriple=little64 -O2 < %s | FileCheck %s
;
; Verify that thread-local variables are lowered using TLS Local Exec:
; materialize the TP-relative offset, read the thread pointer via LSR,
; and ADD them to form the TLS variable address.

@tvar = thread_local global i64 0

; CHECK-LABEL: load_tls:
; CHECK:       LDI     #0, R12
; CHECK-NEXT:  LDI.S1  #128, R12
; CHECK-NEXT:  LSR     R12, R1
; CHECK-NEXT:  JUMP    @+4
; CHECK-NEXT:  .quad   tvar@tprel
; CHECK-NEXT:  LOAD    @-5, R2
; CHECK-NEXT:  ADD     R2, R1
; CHECK-NEXT:  LOAD    [R1], R1

define i64 @load_tls() {
entry:
  %v = load i64, ptr @tvar
  ret i64 %v
}

; CHECK-LABEL: store_tls:
; CHECK:       LDI     #0, R12
; CHECK-NEXT:  LDI.S1  #128, R12
; CHECK-NEXT:  LSR     R12, R1
; CHECK-NEXT:  JUMP    @+4
; CHECK-NEXT:  .quad   tvar@tprel
; CHECK-NEXT:  LOAD    @-5, R2
; CHECK-NEXT:  ADD     R2, R1
; CHECK-NEXT:  STORE   [R1], R10

define void @store_tls(i64 %val) {
entry:
  store i64 %val, ptr @tvar
  ret void
}
