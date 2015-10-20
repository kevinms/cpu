;
; Gameboy Memory Management Unit
;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Read 8 bits from memory at address.
; in  r0 address (16b)
; out r3 data    (8b)
.gb_read8

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Read 16 bits from memory at address.
; in  r0 address (16b)
; out r3 length  (16b)
.gb_read16

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Write 8 bits to memory at address.
; in  r0 address (16b)
; in  r1 data    (8b)
.gb_write8

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Write 16 bits to memory at address.
; in  r0 address (16b)
; in  r1 data    (16b)
.gb_write16

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

