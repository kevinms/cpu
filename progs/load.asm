jmp .code

.data
w 0x01234567

.code
ldw r2 .data
ldb r1 .data

mov r3 .data
stw @r3 0xAABBCCDD
stb @r3 0x0

die
