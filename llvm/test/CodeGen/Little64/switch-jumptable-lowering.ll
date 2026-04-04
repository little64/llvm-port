; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Inspired by jump-table tests in other backends: verify BR_JT lowering path.

; CHECK-LABEL: switch_dense:
; CHECK: TEST
; CHECK: JUMP.C
; CHECK: SLLI    #3, [[IDX:R[0-9]+]]
; CHECK: LOAD    @-5, [[JTBASE:R[0-9]+]]
; CHECK: ADD     [[IDX]], [[JTBASE]]
; CHECK: LOAD    [{{.*}}], [[JTTGT:R[0-9]+]]
; CHECK: MOVE    [[JTTGT]], PC
; CHECK: .LJTI0_0:
; CHECK: .quad .LBB0_2
; CHECK: .quad .LBB0_17

define i64 @switch_dense(i64 %x) {
entry:
  switch i64 %x, label %def [
    i64 0, label %c0
    i64 1, label %c1
    i64 2, label %c2
    i64 3, label %c3
    i64 4, label %c4
    i64 5, label %c5
    i64 6, label %c6
    i64 7, label %c7
    i64 8, label %c8
    i64 9, label %c9
    i64 10, label %c10
    i64 11, label %c11
    i64 12, label %c12
    i64 13, label %c13
    i64 14, label %c14
    i64 15, label %c15
  ]

c0:  ret i64 100
c1:  ret i64 101
c2:  ret i64 102
c3:  ret i64 103
c4:  ret i64 104
c5:  ret i64 105
c6:  ret i64 106
c7:  ret i64 107
c8:  ret i64 108
c9:  ret i64 109
c10: ret i64 110
c11: ret i64 111
c12: ret i64 112
c13: ret i64 113
c14: ret i64 114
c15: ret i64 115

def:
  ret i64 -1
}
