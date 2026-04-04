# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -d --triple=little64 - | FileCheck %s

# Verify the highest encodable LS offset (+6 bytes) round-trips correctly.

LOAD [R3+6], R1
STORE [R3+6], R1

# CHECK: LOAD    [R3+6], R1
# CHECK-NEXT: STORE   [R3+6], R1