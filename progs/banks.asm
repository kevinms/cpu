
; PC 32bit register
; BN 16bit register used for loads/stores/jumps/branches

.setup
mov sp 0x8000

.main

mov r0 .data
sub sp sp 2
stw sp ._ra0
jmp .strlen
._ra0

mov r2 r1
add r2 r2 1
mov r0 0x4000
mov r1 .data
sub sp sp 2
stw sp ._ra1
jmp .memcpy
._ra1


mov r0 .data
mov r1 0
sub sp sp 2
stw sp ._ra2
jmp .memset
._ra2

die

.data
b 'h'
b 'e'
b 'l'
b 'l'
b 'o'
b 32
b 'w'
b 'o'
b 'r'
b 'l'
b 'd'
b 0

