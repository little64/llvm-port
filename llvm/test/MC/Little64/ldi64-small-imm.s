# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -d --triple=little64 - | FileCheck %s

# Small LDI64 immediates should expand through byte-wise LDI sequence.

LDI64 305419896, R1

# CHECK: LDI     #120, R1
# CHECK-NEXT: LDI.S1  #86, R1
# CHECK-NEXT: LDI.S2  #52, R1
# CHECK-NEXT: LDI.S3  #18, R1
