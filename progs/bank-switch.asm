.main
mov sp 0x8000

; do some stuff
mov r0 1
mov r1 1
add r0 r0 r1

; call a function from one switchable bank to another
;   - store current bn and ra on the stack
;   - set a next-bank register
;   - call sepcial farjmp instruction that uses next-bank register
;   - when the callee returns restore the bn and ra from the stack
;   - call farjmp to return to caller

; call a function in a different bank
;   - store bank number on stack
;   - store return address on stack
;   - set the bank number
; <if the caller is in a switchable bank, this won't work>
;   - jmp into switchable bank area
;   - callee begins executing
;       - callee 

.farjmp


.bank0 0x8000
.bank1 0x10000
.bank2 0x18000
.bank3 0x20000
.bank4 0x28000
