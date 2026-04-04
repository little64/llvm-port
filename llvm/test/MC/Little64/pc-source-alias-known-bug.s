# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -d --triple=little64 - | FileCheck %s

# PC should be accepted as a regular source register alias.

MOVE PC, R3

# CHECK: MOVE    R15, R3
