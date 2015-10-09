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

.__copyword

; copy word from source to destination
ldw r3 r1
stw r0 r3

sub r2 r2 2
add r0 r0 2
add r1 r1 2

bnz .__copyword r2

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The length must be a multiple of the word size.
;
; r0 address
; r1 value
; r2 length
.memset

.__setword

; set word to given value
stw r0 r1

sub r2 r2 2
add r0 r0 2

bnz .__setword r2

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The length must be a multiple of the word size.
;
; in  r0 address
; out r3 length
.strlen

mov r3 0

.__countchar

ldb r2 r0
add r3 r3 1
add r0 r0 1

bnz .__countchar r2

sub r3 r3 1

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The length must be a multiple of the word size.
;
; r0 destination address
; r1 source address
.strcpy

.__copychar

ldb r2 r1
stw r0 r2
add r0 r0 1
add r1 r1 1

bnz .__copychar r2

; return to caller
ldw r4 sp
add sp sp 2
jmp r4
