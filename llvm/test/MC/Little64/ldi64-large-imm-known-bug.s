# RUN: llvm-mc -triple=little64 -filetype=obj %s -o - | \
# RUN:   llvm-objdump -s --section=.text - | FileCheck %s --check-prefix=BYTES

# Large LDI64 immediates should use literal-slot materialization and preserve
# all 64 bits in little-endian order.

LDI64 #1234605616436508552, R1

# BYTES: 0000 04e08877 66554433 2211b143
