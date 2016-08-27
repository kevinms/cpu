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
;
; Common bitwise operators:
;   &  and
;   |  or
;   ^  xor
;   ~  not
;  <<  shift left
;  >>  shift right


; less than:
; r0 < r1
mov r0 0
mov r1 1
blt .lt r0 r1
mov r2 42
.lt

; greater than:
; r0 > r1
mov r0 1
mov r1 0
bgt .gt r0 r1
mov r2 42
.gt

; less than or equal to
; r0 <= r1
mov r0 0
mov r1 1
bgt .lte r1 r0
mov r2 42
.lte

; greater than or equal to
; r0 >= r1
mov r0 1
mov r1 0
blt .gte r1 r0
mov r2 42
.gte

; not equal to
; r0 != r1
mov r0 0
mov r1 1
bne .ne r0 r1
mov r2 42
.ne

; equal to
; r0 == r1
mov r0 0
mov r1 1
beq .eq r0 r1
mov r2 42
.eq


; logical or
; r0 = 1;
; r1 = 0;
; if (r0 < r1 || r1 == 0) {
;     r2 = 42;
; }
;
.lor
	mov r0 1
	mov r1 0
	blt .lor_doit r0 r1
	bez .lor_doit r1
	jmp .lor_done
.lor_doit
	mov r2 42
.lor_done

; logical and
; r0 = 0;
; r1 = 0;
; if ((r0 == r1) && (r0 > 5)) {
;     r2 = 42;
; }
.land
	mov r0 6
	mov r1 6
	bne .land_done r0 r1
	mov r3 5
	ble .land_done r0 r3
	jmp .lor_done
.land_doit
	mov r2 42
.land_done

; same as above but without using ble
.land2
	mov r0 0
	mov r1 0
	bne .land_done r0 r1
	mov r3 5
	bgt .land_doit r0 r3
	jmp .land2_done
.land2_doit
	mov r2 42
.land2_done
