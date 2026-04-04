; RUN: llc -mtriple=little64 -O0 < %s | FileCheck %s
;
; Regression coverage for custom lowering of llvm.returnaddress/frameaddress.

; CHECK-LABEL: get_returnaddr0:
; CHECK: MOVE    R14, R1
define i64 @get_returnaddr0() {
entry:
  %ra = call ptr @llvm.returnaddress(i32 0)
  %v = ptrtoint ptr %ra to i64
  ret i64 %v
}

; CHECK-LABEL: get_frameaddr0:
; CHECK: MOVE    R13, R11
; CHECK: MOVE    R11, R1
define i64 @get_frameaddr0() #0 {
entry:
  %slot = alloca i64, align 8
  store i64 0, ptr %slot, align 8
  %fa = call ptr @llvm.frameaddress(i32 0)
  %v = ptrtoint ptr %fa to i64
  ret i64 %v
}

declare ptr @llvm.returnaddress(i32 immarg)
declare ptr @llvm.frameaddress(i32 immarg)

attributes #0 = { "frame-pointer"="all" }
