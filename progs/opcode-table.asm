;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Instruction lookup table

.instructionset
w .NOP
b 0x00 ; -
b 0x00 ; -
w .LD
b 0x42 ; BC
w .LD
b 0x42 ; BC
b 0x20 ; A
w .INC
b 0x42 ; BC
b 0x00 ; -
w .INC
b 0x22 ; B
b 0x00 ; -
w .DEC
b 0x22 ; B
b 0x00 ; -
w .LD
b 0x22 ; B
w .RLCA
b 0x00 ; -
b 0x00 ; -
w .LD
b 0x20 ; A
b 0x42 ; BC
w .DEC
b 0x42 ; BC
b 0x00 ; -
w .INC
b 0x23 ; C
b 0x00 ; -
w .DEC
b 0x23 ; C
b 0x00 ; -
w .LD
b 0x23 ; C
w .RRCA
b 0x00 ; -
b 0x00 ; -
