; RUN: llc -mtriple=little64 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=little64 -O2 -stop-after=machine-scheduler %s -o - | FileCheck %s --check-prefix=MIR
;
; Loop-and-call shape similar to fdt path traversal code: loop gate, helper call,
; branch on helper result, and backedge update.

declare ptr @memchr(ptr, i32, i64)

; CHECK-LABEL: fdt_path_walk_like:
; CHECK: TEST
; CHECK: JUMP.Z
; CHECK: MOVE    R15, R14+2
; CHECK: MOVE    R1, R15
; CHECK: TEST
; CHECK: JUMP.Z
; CHECK: JUMP
; MIR-LABEL: name:            fdt_path_walk_like
; MIR: BRCC %bb.{{[0-9]+}}, 0,
; MIR: CALL
; MIR: BRCC %bb.{{[0-9]+}}, 1,
; MIR: JMP %bb.{{[0-9]+}}

define ptr @fdt_path_walk_like(ptr %p, i64 %len, i8 %ch) {
entry:
  %ch32 = zext i8 %ch to i32
  br label %loop

loop:
  %curp = phi ptr [ %p, %entry ], [ %next, %cont ]
  %curn = phi i64 [ %len, %entry ], [ %nnext, %cont ]
  %empty = icmp eq i64 %curn, 0
  br i1 %empty, label %ret_null, label %scan

scan:
  %m = call ptr @memchr(ptr %curp, i32 %ch32, i64 %curn)
  %found = icmp ne ptr %m, null
  br i1 %found, label %ret_found, label %cont

cont:
  %next = getelementptr i8, ptr %curp, i64 1
  %nnext = add i64 %curn, -1
  br label %loop

ret_found:
  ret ptr %m

ret_null:
  ret ptr null
}
