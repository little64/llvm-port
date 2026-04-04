# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -d --triple=little64 - | FileCheck %s

# Small signed branch offsets should round-trip through disassembly.

JUMP.Z @+1
JUMP @-1

# CHECK: JUMP.Z  @+1
# CHECK-NEXT: JUMP    @-1
