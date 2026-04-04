# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -d --triple=little64 - | FileCheck %s

# Ensure parser-level CALL/RET pseudos do not crash and expand correctly.

CALL R1
RET

# CHECK: MOVE    R15, R14+2
# CHECK-NEXT: MOVE    R1, PC
# CHECK-NEXT: MOVE    R14, PC
