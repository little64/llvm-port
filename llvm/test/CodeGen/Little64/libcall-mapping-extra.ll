; RUN: llc -mtriple=little64 -O2 < %s | FileCheck %s

define i64 @use_ctlz(i64 %x) {
entry:
  %v = call i64 @llvm.ctlz.i64(i64 %x, i1 false)
  ret i64 %v
}

define i64 @use_ctpop(i64 %x) {
entry:
  %v = call i64 @llvm.ctpop.i64(i64 %x)
  ret i64 %v
}

define double @use_add_f64(double %a, double %b) {
entry:
  %v = fadd double %a, %b
  ret double %v
}

define float @use_mul_f32(float %a, float %b) {
entry:
  %v = fmul float %a, %b
  ret float %v
}

define i64 @use_fptosi_f64_i64(double %a) {
entry:
  %v = fptosi double %a to i64
  ret i64 %v
}

define i64 @use_fptoui_f32_i64(float %a) {
entry:
  %v = fptoui float %a to i64
  ret i64 %v
}

define double @use_sitofp_i64_f64(i64 %a) {
entry:
  %v = sitofp i64 %a to double
  ret double %v
}

define float @use_uitofp_i64_f32(i64 %a) {
entry:
  %v = uitofp i64 %a to float
  ret float %v
}

declare i64 @llvm.ctlz.i64(i64, i1 immarg)
declare i64 @llvm.ctpop.i64(i64)

; CHECK-LABEL: use_add_f64:
; CHECK:       .quad __adddf3

; CHECK-LABEL: use_mul_f32:
; CHECK:       .quad __mulsf3

; CHECK-LABEL: use_fptosi_f64_i64:
; CHECK:       .quad __fixdfdi

; CHECK-LABEL: use_fptoui_f32_i64:
; CHECK:       .quad __fixunssfdi

; CHECK-LABEL: use_sitofp_i64_f64:
; CHECK:       .quad __floatdidf

; CHECK-LABEL: use_uitofp_i64_f32:
; CHECK:       .quad __floatundisf
