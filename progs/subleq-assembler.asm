
; NL  0  1  2  3  4  5  6  7  8  9  #
; 10 48 49 50 51 52 53 54 55 56 57 35

; Subtract and branch if less than or equal to zero.
; subleq a, b, c   ; Mem[b] = Mem[b] - Mem[a]
                   ; if (Mem[b] ≤ 0) goto c

; ld r0 a
; ld r1 b
; sub r0 r1 r0
; st r1 r0
; blz c r0

; Memory:
; 0x0   - 0x3FF  Assembler
; 0x400 - 0x832  Program Source
; 0x833 - 0xFFFF Output Machine Code

; Registers:
; r3 r7    function ret address
; r4 r5 r6 instruction components

; one instruction word that we can use during execution
nop

.GLOBALS
mov r0 0x3FF      ; location counter

.MAIN ; 45

st 2 0            ; output counter

.parse
mov r3 .dump
jmp .READLINE
.dump

bez .DONE r1

; reuse r0
st 0 r0
ld r0 2

; store everything
st r0 0x0504 ; ld r0 a
add r0 r0 2
st r0 0
add r0 r0 2
st r0 r4
add r0 r0 2
st r0 0
add r0 r0 2

st r0 0x0504 ; ld r1 b
add r0 r0 2
st r0 1
add r0 r0 2
st r0 r5
add r0 r0 2
st r0 0
add r0 r0 2

st r0 0x0207 ; sub r0 r1 r0
add r0 r0 2
st r0 0
add r0 r0 2
st r0 0
add r0 r0 2
st r0 1
add r0 r0 2

st r0 0x0606 ; st r1 r0
add r0 r0 2
st r0 1
add r0 r0 2
st r0 0
add r0 r0 2
st r0 0
add r0 r0 2

st r0 0x0F04 ; ble c r0
add r0 r0 2
st r0 r6
add r0 r0 2
st r0 0
add r0 r0 2
st r0 0

; set r0 back to what it should be
st 2 r0
ld r0 0

jmp .parse

.DONE
die

; uses r4 r5 r6
; return r3
; ouput r1
.READLINE

mov r7 .readone
jmp .READ
.readone
mov r4 r1
add r0 r0 1

mov r7 .readtwo
jmp .READ
.readtwo
mov r5 r1
add r0 r0 1

mov r7 .readthree
jmp .READ
.readthree
mov r6 r1

ld r1 r0    ; read newline or null
add r0 r0 1

jmp r3

;uses r0 r1 r2 r7
;return r7
;output r1
.READ
ld r1 r0        ; load 16bit word
and r1 r1 0x00FF; only keep the second byte
sub r1 r1 48    ; convert ascii to int
mul r1 r1 10000 ; ten-thousands place
add r0 r0 1     ; increment location

ld r2 r0
and r2 r2 0x00FF
sub r2 r2 48
mul r2 r2 1000  ; thousands place
add r1 r1 r2
add r0 r0 1

ld r2 r0
and r2 r2 0x00FF
sub r2 r2 48
mul r2 r2 100   ; hundreds place
add r1 r1 r2
add r0 r0 1

ld r2 r0
and r2 r2 0x00FF
sub r2 r2 48
mul r2 r2 10    ; tens place
add r1 r1 r2
add r0 r0 1

ld r2 r0
and r2 r2 0x00FF
sub r2 r2 48    ; ones place
add r1 r1 r2
add r0 r0 1

jmp r7

.EOF
