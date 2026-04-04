; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Regression coverage for ISD::BlockAddress custom lowering via
; Little64ISD::Wrapper → LDI64 → literal-slot expansion.  Without a
; setOperationAction + LowerBlockAddress wrapping the node, the default Legal
; action reaches the selector with no matching pattern and crashes.
;
; Block addresses are always relocatable symbols, so LDI64 always takes the
; literal-slot path: JUMP @+4 / .quad <sym> / LOAD @-5.

; CHECK-LABEL: test_blockaddress:
; CHECK: JUMP    @+4
; CHECK: .quad
; CHECK: LOAD    @-5
; CHECK: MOVE R14, PC
define ptr @test_blockaddress() {
entry:
  br label %target
target:
  ret ptr blockaddress(@test_blockaddress, %target)
}

; Block addresses used in an indirect branch must also compile and produce
; the indirect jump through PC (MOVE <reg>, PC / JMPr).
; CHECK-LABEL: test_indirectbr:
; CHECK: JUMP    @+4
; CHECK: .quad
; CHECK: LOAD    @-5
; CHECK: MOVE {{R[0-9]+}}, PC
define i64 @test_indirectbr(i1 %cond) {
entry:
  %addr = select i1 %cond, ptr blockaddress(@test_indirectbr, %bb1),
                            ptr blockaddress(@test_indirectbr, %bb2)
  indirectbr ptr %addr, [label %bb1, label %bb2]
bb1:
  ret i64 1
bb2:
  ret i64 2
}
