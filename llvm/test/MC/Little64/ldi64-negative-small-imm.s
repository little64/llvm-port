# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -d --triple=little64 - | FileCheck %s

# Small negative values that fit signed i32 use byte-wise LDI expansion.

LDI64 #-1, R2
LDI64 #-129, R3

# CHECK: LDI     #255, R2
# CHECK-NEXT: LDI.S1  #255, R2
# CHECK-NEXT: LDI.S2  #255, R2
# CHECK-NEXT: LDI.S3  #255, R2
# CHECK-NEXT: LDI     #127, R3
# CHECK-NEXT: LDI.S1  #255, R3
# CHECK-NEXT: LDI.S2  #255, R3
# CHECK-NEXT: LDI.S3  #255, R3