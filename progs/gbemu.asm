#
# Game Boy Emulator
#

; Memory:
; 0x0    - 0x3FFF  Program
; 0x4000 - 0x7FFF  Stack
; 0x8000 - 0xFFFF  Game Boy ROM

; Stack:
; sp  Stack Pointer
; r6  Return Address
; r7  Return Value

jmp .setup

.globals
; GB registers
nop ; AF BC DE HL
nop ; SP PC

.setup
mov sp 0x8000

.gbinit
stw 0x8 0x01B0 ; AF
stw 0xA 0x0013 ; BC
stw 0xC 0x00D8 ; DE
stw 0xE 0x014D ; HL
stw 0x10 0xFFFE ; SP
stw 0x12 0x100  ; PC

.main

.icycle
; load
ldb r0 0x10 ; PC
add r0 r0 0x8000 ; cart offset

; decode and execute
bne .ldSPd16 r0 0x31
add r0 r0 1
ldw r1 r0
add r0 r0 2

.ldSPd16

beq .end r0 0     ; NOP
beq .halt r0 0x76 ; HALT

die

.end

; increment GB's PC
sub r0 r0 0x8000
stb 0x10 r0

jmp .icycle

.halt
die
