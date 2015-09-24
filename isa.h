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

#define add  0b00000001 // -mm dst{r}   lhs{r,c}  rhs{r,c}
#define sub  0b00000010 // -mm dst{r}   lhs{r,c}  rhs{r,c}
#define mul  0b00000011 // -mm dst{r}   lhs{r,c}  rhs{r,c}
#define div  0b00000100 // -mm dst{r}   lhs{r,c}  rhs{r,c}

#define ldw  0b00000101 // -m- dst{r}   src{r,c}
#define stw  0b00000110 // mm- dst{r,c} src{r,c}
#define ldb  0b00000111 // -m- dst{r}   src{r,c}
#define stb  0b00001000 // mm- dst{r,c} src{r,c}

#define mov  0b00001001 // -m- dst{r}   src{r,c}

#define and  0b00001010 // -mm dst{r}   lhs{r,c}  rhs{r,c}
#define or   0b00001011 // -mm dst{r}   lhs{r,c}  rhs{r,c}
#define xor  0b00001100 // -mm dst{r}   lhs{r,c}  rhs{r,c}
#define nor  0b00001101 // -mm dst{r}   lhs{r,c}  rhs{r,c}
#define lsl  0b00001110 // -mm dst{r}   lhs{r,c}  rhs{r,c}
#define lsr  0b00001111 // -mm dst{r}   lhs{r,c}  rhs{r,c}

#define bez  0b00010000 // mm- dst{r,c} lhs{r,c}
#define ble  0b00010001 // mm- dst{r,c} lhs{r,c}
#define bge  0b00010010 // mm- dst{r,c} lhs{r,c}
#define bne  0b00010011 // mmm dst{r,c} lhs{r,c}  rhs{r,c}
#define beq  0b00010100 // mmm dst{r,c} lhs{r,c}  rhs{r,c}
#define jmp  0b00010101 // m-- dst{r,c}

#define in   0b00010110 // ---
#define out  0b00010111 // ---

#define die  0b11111111
// End Instruction Op Codes

#endif /* __ISA_H */
