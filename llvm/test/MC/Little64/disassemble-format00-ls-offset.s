# RUN: llvm-mc -triple=little64 -disassemble < %s | FileCheck %s

# CHECK: MOVE	R15, R14+2
0xfe 0x11

# CHECK: LOAD	[R4+6], R1
0x41 0x03

# CHECK: STORE	[R8+4], R3
0x83 0x06
