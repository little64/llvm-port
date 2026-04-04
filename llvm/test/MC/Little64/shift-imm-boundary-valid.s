# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -d --triple=little64 - | FileCheck %s

# Verify boundary shift immediates round-trip for immediate-shift forms.

SLLI #0, R1
SRLI #15, R2
SRAI #15, R3

# CHECK: SLLI    #0, R1
# CHECK-NEXT: SRLI    #15, R2
# CHECK-NEXT: SRAI    #15, R3
