#
# Function stack.
#

; Memory:
; 0x0    - 0x3FFF  Program
; 0x4000 - 0x7FFF  Stack

; Stack:
; sp  Stack Pointer
; bp  Base Pointer
; r7  Return Value

.globals
nop ; hell
nop ; o wo
nop ; rld\0
stb 0x0 'h'
stb 0x1 'e'
stb 0x2 'l'
stb 0x3 'l'
stb 0x4 'o'
stb 0x5 32
stb 0x6 'w'
stb 0x7 'o'
stb 0x8 'r'
stb 0x9 'l'
stb 0xa 'd'
stb 0xb 0

.setup
mov sp 0x7FFF
mov bp 0x7FFF

.main

mov r0 0x0
mov r1 0x8000
sub sp sp 2
stw sp .raprint
jmp .print
.raprint
add sp sp 2 ; throw away return address

die

; r0 - address of string to print
; r1 - offset to print string
.print
; save base pointer
sub sp sp 2
stw sp bp
mov bp sp

; save registers to be used
sub sp sp 2
stw sp r0
sub sp sp 2
stw sp r1
sub sp sp 2
stw sp r2

.loop

ldw r2 r0
bez .end r2

sub sp sp 2
stw sp .raputc
jmp .putc
.raputc
add sp sp 2 ; throw out the return address

add r0 r0 1
add r1 r1 1

.end

; restore used registers
ldw r2 sp
add sp sp 2
ldw r1 sp
add sp sp 2
ldw r0 sp
add sp sp 2

; restore base pointer
ldw bp sp
add sp sp 2

; return to caller
ldw r6 sp
jmp r6

; r0 - character to print
; r1 - offset to print character
.putc
; save base register
sub sp sp 2
stw sp bp
mov bp sp

; save registers to be used
sub sp sp 2
stw sp r2

ldw r2 r0 ; load char into r2
stw r1 r2 ; store char at offset r1

; restore registers
ldw r2 sp
add sp sp 2

; restore base register
ldw bp sp
add sp sp 2

; return to caller
ldw r6 sp
jmp r6
