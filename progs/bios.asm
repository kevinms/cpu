;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Basic Input/Output System
; 
; Responsible for loading the kernel from the mass storage device.
;

.kernel 0x0
.stdlib 0x3000
.bios	0x4000
.tmp_sd 0x5000

.sd_kernel_data 0x60

.fs_super_size		0x10
.fs_header_size		0x50
.fs_overhead		0x54

; initialize the stack
mov ba .bios
mov sp 0x1000

;
; Initialize the SD card.
;
sub sp sp 4
stw sp ._ra0
jmp .SDinit
._ra0

;
; Read the first file header.
;
sub sp sp .fs_header_size	; make room on the stack for the header
add r0 sp ba				; destination address (main memory)
mov r1 .fs_super_size		; skip past superBlock and read first header (SD card)
mov r2 .fs_header_size		; count (size of header)
sub sp sp 4
stw sp ._ra1
jmp .SDread
._ra1

;
; Get the kernel size (r5) and length (r6) from the header.
;
ldw r6 sp
add sp sp 12
ldw r5 sp
add sp sp .fs_header_size+-12

;
; Copy kernel into main memory.
;
mov r0 .kernel			; destination address (main memory)
mov r1 .sd_kernel_data	; Skip past superBlock and read contents of first filesystem object (SD card)
mov r2 r5				; count (kernel size on stack)
sub sp sp 4
stw sp ._ra2
jmp .SDread
._ra2

;
; Get offset of second file header.
;
add r6 r6 .fs_super_size
add r6 r6 .fs_overhead

;
; Read the second file header.
;
sub sp sp .fs_header_size	; make room on the stack for the header
add r0 sp ba				; destination address (main memory)
mov r1 r6					; skip past superBlock and first file (SD card)
mov r2 .fs_header_size		; count (size of header)
sub sp sp 4
stw sp ._ra3
jmp .SDread
._ra3

;
; Get the stdlib size (r5)
;
add sp sp 12
ldw r5 sp
add sp sp .fs_header_size+-12

;
; Copy standard library into main memory.
;
mov r0 .stdlib				; destination address (main memory)
add r1 r6 .fs_header_size	; read contents of second file (SD card)
mov r2 r5					; count (stdlib size on stack)
sub sp sp 4
stw sp ._ra4
jmp .SDread
._ra4

;
; Transfer control to the kernel.
;
mov ba 0x0
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

	; take source address and add the temporary offset to it
	add r1 r1 .tmp_sd

	.copy
	cmp r2 0
	jz .endcopy
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
