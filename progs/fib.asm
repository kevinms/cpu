
# fibanacci sequence -- first 24 numbers

mov r7 22
mov r0 1
mov r1 1

.fibanacci
bez .done r7

add r3 r0 r1
mov r0 r1
mov r1 r3

sub r7 r7 1

jmp .fibanacci

.done
die
