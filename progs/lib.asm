;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Standard Library
;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The length must be a multiple of the word size.
; Source and destination must be in the same bank.
;
; r0 destination address
; r1 source address
; r2 length
.memcpy
; save registers to be used
sub sp sp 2
stw sp r0
sub sp sp 2
stw sp r1
sub sp sp 2
stw sp r2
sub sp sp 2
stw sp r3

._copyword

; copy word from source to destination
ldw r3 r1
stw r0 r3

sub r2 r2 2
add r0 r0 2
add r1 r1 2

bnz ._copyword r2

; restore used registers
ldw r3 sp
add sp sp 2
ldw r2 sp
add sp sp 2
ldw r1 sp
add sp sp 2
ldw r0 sp
add sp sp 2

; return to caller
ldw r6 sp
jmp r6

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The length must be a multiple of the word size.
;
; r0 address
; r1 value
; r2 length
.memset
; save registers to be used
sub sp sp 2
stw sp r0
sub sp sp 2
stw sp r1
sub sp sp 2
stw sp r2

._setword

; set word to given value
stw r0 r1

sub r2 r2 2
add r0 r0 2

bnz ._setword r2

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

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The length must be a multiple of the word size.
;
; r0 address
; r1 length (return value)
.strlen
; save registers to be used
sub sp sp 2
stw sp r0
sub sp sp 2
stw sp r2

mov r1 0

._countchar

ldb r2 r0
add r1 r1 1
add r0 r0 1

bnz ._countchar r2

sub r1 r1 1

; restore used registers
ldw r2 sp
add sp sp 2
ldw r0 sp
add sp sp 2

; return to caller
ldw r6 sp
jmp r6

