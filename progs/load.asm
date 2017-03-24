jmp .code

.data
w 0x01234567

.code
ldw r2 .data
ldb r1 .data
