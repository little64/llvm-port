; RUN: llc -mtriple=little64 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=little64 -O2 -stop-after=machine-scheduler %s -o - | FileCheck %s --check-prefix=MIR
;
; Mirror the key unsigned-header predicates used by libfdt fdt_check_header/check_block.
; This gives us a controlled CodeGen target when debugging Linux boot regressions.
;
; Source-shape being modeled:
;   totalsize < hdrsize || totalsize > INT_MAX
;   off >= hdrsize && off <= totalsize
;   check_block(base,size):
;     check_off(base) && (base + size) >= base && check_off(base + size)

; CHECK-LABEL: total_range_bad:
; CHECK: TEST
; CHECK: JUMP.C
; CHECK: TEST
; CHECK: JUMP.C
; CHECK: JUMP
; MIR-LABEL: name:            total_range_bad
; MIR: BRCC %bb.{{[0-9]+}},
; MIR: BRCC %bb.{{[0-9]+}},

define i32 @total_range_bad(i32 %totalsize, i32 %hdrsize) {
entry:
  %too_small = icmp ult i32 %totalsize, %hdrsize
  %too_large = icmp ugt i32 %totalsize, 2147483647
  %bad = or i1 %too_small, %too_large
  br i1 %bad, label %badret, label %okret

badret:
  ret i32 -8

okret:
  ret i32 0
}

; CHECK-LABEL: check_off_like:
; CHECK: TEST
; CHECK: CMOV.C
; CHECK: TEST
; CHECK: CMOV.C
; MIR-LABEL: name:            check_off_like
; MIR: SETCC_SELECT
; MIR: SETCC_SELECT

define i1 @check_off_like(i32 %hdrsize, i32 %totalsize, i32 %off) {
entry:
  %start_ok = icmp uge i32 %off, %hdrsize
  %end_ok = icmp ule i32 %off, %totalsize
  %ok = and i1 %start_ok, %end_ok
  ret i1 %ok
}

; CHECK-LABEL: check_block_like:
; CHECK: ADD
; CHECK: TEST
; CHECK: JUMP.Z
; CHECK: TEST
; CHECK: JUMP.C
; MIR-LABEL: name:            check_block_like
; MIR: BRCC %bb.{{[0-9]+}},
; MIR: BRCC %bb.{{[0-9]+}},
; MIR: BRCC %bb.{{[0-9]+}},

define i1 @check_block_like(i32 %hdrsize, i32 %totalsize, i32 %base, i32 %size) {
entry:
  %start_ok = icmp uge i32 %base, %hdrsize
  br i1 %start_ok, label %start_in, label %ret_false

start_in:
  %sum = add i32 %base, %size
  %overflow = icmp ult i32 %sum, %base
  br i1 %overflow, label %ret_false, label %end_check

end_check:
  %end_ok = icmp ule i32 %sum, %totalsize
  br i1 %end_ok, label %ret_true, label %ret_false

ret_true:
  ret i1 true

ret_false:
  ret i1 false
}

; CHECK-LABEL: fdt_header_check_like:
; CHECK: TEST
; CHECK: JUMP.C
; CHECK: TEST
; CHECK: JUMP.C
; CHECK: ADD
; CHECK: TEST
; CHECK: JUMP.Z
; CHECK: TEST
; CHECK: JUMP.C
; MIR-LABEL: name:            fdt_header_check_like
; MIR: BRCC %bb.{{[0-9]+}},
; MIR: BRCC %bb.{{[0-9]+}},
; MIR: BRCC %bb.{{[0-9]+}},
; MIR: BRCC %bb.{{[0-9]+}},
; MIR: BRCC %bb.{{[0-9]+}},
; MIR: BRCC %bb.{{[0-9]+}},
; CHECK: ADD
; CHECK: TEST
; CHECK: JUMP.Z
; CHECK: TEST
; CHECK: JUMP.C

define i32 @fdt_header_check_like(
    i32 %totalsize,
    i32 %hdrsize,
    i32 %off_mem_rsvmap,
    i32 %off_dt_struct,
    i32 %size_dt_struct,
    i32 %off_dt_strings,
    i32 %size_dt_strings) {
entry:
  %too_small = icmp ult i32 %totalsize, %hdrsize
  %too_large = icmp ugt i32 %totalsize, 2147483647
  %bad_total = or i1 %too_small, %too_large
  br i1 %bad_total, label %ret_bad, label %chk_memrsv

chk_memrsv:
  %mem_start_ok = icmp uge i32 %off_mem_rsvmap, %hdrsize
  %mem_end_ok = icmp ule i32 %off_mem_rsvmap, %totalsize
  %mem_ok = and i1 %mem_start_ok, %mem_end_ok
  br i1 %mem_ok, label %chk_struct_base, label %ret_bad

chk_struct_base:
  %struct_base_ok = icmp uge i32 %off_dt_struct, %hdrsize
  br i1 %struct_base_ok, label %chk_struct_ovf, label %ret_bad

chk_struct_ovf:
  %struct_end = add i32 %off_dt_struct, %size_dt_struct
  %struct_ovf = icmp ult i32 %struct_end, %off_dt_struct
  br i1 %struct_ovf, label %ret_bad, label %chk_struct_end

chk_struct_end:
  %struct_end_ok = icmp ule i32 %struct_end, %totalsize
  br i1 %struct_end_ok, label %chk_str_base, label %ret_bad

chk_str_base:
  %str_base_ok = icmp uge i32 %off_dt_strings, %hdrsize
  br i1 %str_base_ok, label %chk_str_ovf, label %ret_bad

chk_str_ovf:
  %str_end = add i32 %off_dt_strings, %size_dt_strings
  %str_ovf = icmp ult i32 %str_end, %off_dt_strings
  br i1 %str_ovf, label %ret_bad, label %chk_str_end

chk_str_end:
  %str_end_ok = icmp ule i32 %str_end, %totalsize
  br i1 %str_end_ok, label %ret_ok, label %ret_bad

ret_bad:
  ret i32 -8

ret_ok:
  ret i32 0
}
