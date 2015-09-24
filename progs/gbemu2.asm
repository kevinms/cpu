#
# Game Boy Emulator
#

; Memory:
.progm 0x0    ; 0x0    - 0x3FFF  Program
.stack 0x4000 ; 0x4000 - 0x7FFF  Stack
.gboff 0x8000 ; 0x8000 - 0xFFFF  Game Boy ROM

; Stack:
; sp  Stack Pointer
; r6  Return Address (transient)
; r7  Return Value   (transient)

jmp .main

.gbregs
; GB registers
.AF
w 0x01B0 ; AF
.BC
w 0x0013 ; BC
.DE
w 0x00D8 ; DE
.HL
w 0x014D ; HL
.SP
w 0xFFFE ; SP
.PC
w 0x100  ; PC
.FL
b 0x0    ; FL

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.main

.cycle

; load opcode
;   gb pc  -> r0
;   opcode -> r1
ldb r0 .PC
add r0 r0 .gboff
ldb r1 r0

; lookup opcode
;   entry   -> r1
;   handler -> r2
;   arg1    -> r3
;   arg2    -> r4
mul r2 4 r1 ; offset into instruction set
add r1 r2 .instructionset
ldw r2 r1
add r1 r1 1
ldb r3 r1
add r1 r1 1
ldb r4 r1

; decode arg1
sub sp sp 2
stw sp .argo
jmp .decode
.argo
add sp sp 2

; decode arg two
sub sp sp 2
stw sp .argt
jmp .decode
.argt
add sp sp 2

#jmp .decode+2 ; test label+offset
die ;TODO: debugging

; jmp to handler
sub sp sp 2
stw sp .outer
jmp r2
.outer
add sp sp 2 ; throw out return address

; increment GB's PC
sub r0 r0 0x8000
stb 0x10 r0

jmp .cycle

die

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; r4 operand type
.decode
; save registers to be used
sub sp sp 2
stw sp r2

bne .typeA r4 0x0

.typeA

; restore registers
ldw r2 sp
add sp sp 2

; return to caller
ldw r6 sp
jmp r6

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Instruction Handlers

.NOP
; return to caller
ldw r6 sp
jmp r6

.LD
; return to caller
ldw r6 sp
jmp r6

.INC
; determine address of data to increment

; increment data

; return to caller
ldw r6 sp
jmp r6

.DEC
; return to caller
ldw r6 sp
jmp r6

.RLCA
; return to caller
ldw r6 sp
jmp r6

.RRCA
; return to caller
ldw r6 sp
jmp r6

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;.instructionset
;  Operand Info Bits:
;    Bits   Meaning
;    7 - 5  type
;    4 - 0  offset
;
;  Type Values:
;    The addressing mode determines where
;    the operand is and how large it is.
;
;   type  addressing mode
;     0   null
;     1    8bit register
;     2   16bit register
;     3    8bit immediate
;     4   16bit immediate
;     5    8bit indirect
;     6   16bit indirect
;     7 <reserved>
;
;  Offset Values:
;    To calculate the address for a regsiter,
;    the offset is added to the .gbregs label.
;
;   offset  reg   label
;     0    AF/A  .AF
;     1       F  .AF+1
;     2    BC/B  .BC
;     3       C  .BC+1
;     4    DE/D  .DE
;     5       E  .DE+1
;     6    HL/H  .HL
;     7       L  .HL+1
;     8    SP    .SP
;     9    -      --
;    10    PC    .PC
;    11    -      --
;
;w opcode handler address
;b operand one type
;b operand two type

