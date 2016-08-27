;
; This program is for jump testing.
;

; Common relational operators:
;   <   less than
;   >   greater than
;   <=  less than or equal to
;   >=  greater than or equal to
;   !=  not equal to
;   ==  equal to
;
; Common logical operators:
;   &&  and
;   ||  or
;    !  not

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; jnz
;

; true
mov r0 2
cmp r0 0
jnz .notzero_t
mov r0 4
.notzero_t
nop

; false
mov r0 2
cmp r0 2
jnz .notzero_f
mov r0 4
.notzero_f
nop

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; jz
;

; true
mov r0 2
cmp r0 2
jz .zero_t
mov r0 4
.zero_t
nop

; flase
mov r0 2
cmp r0 0
jz .zero_f
mov r0 4
.zero_f
nop

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; jl
;

; true
mov r0 2
mov r1 3
cmp r0 r1
jl .less_t
mov r0 4
.less_t
nop

; false
mov r0 3
mov r1 2
cmp r0 r1
jl .less_f
mov r0 4
.less_f
nop

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; jg
;

; true
mov r0 3
mov r1 2
cmp r0 r1
jg .greater_t
mov r0 4
.greater_t
nop

; false
mov r0 2
mov r1 3
cmp r0 r1
jg .greater_f
mov r0 4
.greater_f
die
