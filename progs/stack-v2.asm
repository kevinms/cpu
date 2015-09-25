#
# Function stack.
#

; Memory:
; 0x0    - 0x3FFF  Program
; 0x4000 - 0x7FFF  Stack

; Stack:
; sp  Stack Pointer
; r7  Return Value

jmp .setup

.globals
w 'h'
w 'e'
w 'l'
w 'l'
w 'o'
w 32
w 'w'
w 'o'
w 'r'
w 'l'
w 'd'
w 0

.setup
mov sp 0x8000

.main

mov r0 .globals
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

jmp .loop

.end

; restore used registers
ldw r2 sp
add sp sp 2
ldw r1 sp
add sp sp 2
ldw r0 sp
add sp sp 2

; return to caller
ldw r6 sp
jmp r6

; r0 - character to print
; r1 - offset to print character
.putc
; save registers to be used
sub sp sp 2
stw sp r2

ldw r2 r0 ; load char into r2
stw r1 r2 ; store char at offset r1

; restore registers
ldw r2 sp
add sp sp 2

; return to caller
ldw r6 sp
jmp r6
