;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Basic Input/Output System
; 
; Responsible for loading the kernel from the mass storage device.
;

.kernel 0x0
.kstack	0x1000
.mmapIO 0x2000
.stdlib 0x3000
.bios	0x10000

.sd_kernel_size 0x18
.sd_kernel_data 0x5C
.sd_stdlib_size 0x0


; initialize the stack
mov ba 0x4000
mov sp .stack+0x0400

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
add r0 sp ba; destination address (main memory)
mov r1 .sd_kernel_size ; Skip past superBlock and read fileSize from first header (SD card)
mov r2 0x4  ; count (4 bytes for kernel size)
sub sp sp 4
stw sp ._ra1
jmp .SDread
._ra1

;
; Copy kernel into main memory.
;
mov r0 0x0   ; destination address (main memory)
mov r1 .sd_kernel_data  ; Skip past superBlock and read contents of first filesystem object (SD card)
ldw r2 sp    ; count (kernel size on stack)
add sp sp 4
sub sp sp 4
stw sp ._ra2
jmp .SDread
._ra2

;
; Transfer control to the kernel.
;
jmp @0x0

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

; take source address and add the temporary offset to it
add r1 r1 0x5000

.copy
bez .endcopy r2
ldb r3 @r1
stb @r0 r3
add r0 r0 1
add r1 r1 1
sub r2 r2 1
jmp .copy
.endcopy

; return to caller
ldw r4 sp
add sp sp 4
jmp r4

; The stack will begin after the code.
.stack
