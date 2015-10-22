;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Basic Input/Output System
; 
; Responsible for loading the kernel from the mass storage device.
;

; initialize the stack
mov sp .stack+0xFFFF

;
; Initialize the SD card.
;
sub sp sp 4
stw sp ._ra0
jmp .SDinit
._ra0

;
; Get the kernel size.
;
sub sp sp 4
mov r0 sp  ; destination address (main memory)
mov r1 0x0 ; source address (SD card)
mov r2 0x4 ; count (4 bytes for kernel size)
sub sp sp 4
stw sp ._ra1
jmp .SDread
._ra1


;
; Copy kernel into main memory.
;
mov r0 0x0  ; destination address (main memory)
mov r1 0x0  ; source address (SD card)
add sp sp 4
ldw r2 sp   ; count (kernel size on stack)
sub sp sp 4
stw sp ._ra1
jmp .SDread
._ra1

;
; Transfer control to the kernel.
;
jmp 0x0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Initialize SD card for SPI mode.
;
.SDinit

; return to caller
ldw r4 sp
add sp sp 4
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Read count bytes from the SD card and write them to main memory.
;
; r0 destination address (main memory)
; r1 source address (SD card)
; r2 count (bytes)
.SDread

; CMD17 for READ_SINGLE_BLOCK
; Send command frame.
; Receive command response.
; Receive data packet (data token + data block + crc)

; return to caller
ldw r4 sp
add sp sp 4
jmp r4

; The stack will begin after the code.
.stack
