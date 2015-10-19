#ifndef __ISA_H
#define __ISA_H

//NOTICE:
// This file is parsed by the assembler so be careful.

// Big Endian

// Registers
// r0 to r7  General Purpose
// pc        Proram Counter
// flags     Special Flags
// p0 to p3  Port Mapped I/O
// c0 to c4  64bit Hardware Counter

// flags Register
// nzvc----
#define FL_N 0b10000000 // Negative
#define FL_Z 0b01000000 // Zero
#define FL_V 0b00100000 // Overflow
#define FL_C 0b00010000 // Carry

// Addressing Modes
#define MODE_BITA 0b100
#define MODE_BITB 0b010
#define MODE_BITC 0b001
#define MODE_REG  0b1 // Direct Register
#define MODE_IMM  0b0 // Immediate Constant

// Instruction Op Codes
#define nop  0b00000000

//      inst opcode      mode dest   lhs   rhs
#define add  0b00000001 // 2  d[r]   l[r]  r[r,c]
#define sub  0b00000010 // 2  d[r]   l[r]  r[r,c]
#define adc  0b00000011 // 2  d[r]   l[r]  r[r,c]
#define sbc  0b00000100 // 2  d[r]   l[r]  r[r,c]
#define mul  0b00000101 // 2  d[r]   l[r]  r[r,c]
#define div  0b00000110 // 2  d[r]   l[r]  r[r,c]

#define ldw  0b00000111 // 1  d[r]   src[r,c]
#define ldb  0b00001000 // 1  d[r]   src[r,c]
#define stw  0b00001001 // 0  d[r,c] src[r]
#define stb  0b00001010 // 0  d[r,c] src[r]

#define mov  0b00001011 // 1  d[r]   src[r,c]

#define and  0b00001100 // 2  d[r]   l[r]  r[r,c]
#define or   0b00001101 // 2  d[r]   l[r]  r[r,c]
#define xor  0b00001110 // 2  d[r]   l[r]  r[r,c]
#define nor  0b00001111 // 2  d[r]   l[r]  r[r,c]
#define lsl  0b00010000 // 2  d[r]   l[r]  r[r,c]
#define lsr  0b00010001 // 2  d[r]   l[r]  r[r,c]

#define bez  0b00010010 // 0  d[r,c] l[r]
#define bnz  0b00010011 // 0  d[r,c] l[r]
#define ble  0b00010100 // 0  d[r,c] l[r]  r[r]
#define bge  0b00010101 // 0  d[r,c] l[r]  r[r]
#define bne  0b00010110 // 0  d[r,c] l[r]  r[r]
#define beq  0b00010111 // 0  d[r,c] l[r]  r[r]
#define jmp  0b00011000 // 0  d[r,c]

#define in   0b00011001
#define out  0b00011010

#define die  0b11111111
// End Instruction Op Codes

#endif /* __ISA_H */
