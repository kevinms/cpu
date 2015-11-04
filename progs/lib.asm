;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Standard Library
;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Globals
;
.__heapOffset
w 0x0 ; Heap Offset

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Initialize the dynamic memory allocator.
;
; in  r0 heap offset
; in  r1 heap size
export .init_malloc

mov r2 .__heapOffset
stw @r2 r0

add r2 r0 12 ; header offset
add r3 r0 r1
sub r3 r3 4  ; trailer offset
sub r4 r1 28 ; free space length

; init super block
stw @r0 r1
add r0 r0 4
stw @r0 0x0
add r0 r0 4
stw @r0 r2

; init free block header
stw @r3 r2

; init free block header
stw @r2 r4
add r2 r2 4
stw @r2 0x0
add r2 r2 4
stw @r2 0x0

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Dynamically allocate a region of memory.
;
; in  r0 requested size (bytes)
; out r3 address of allocation
export .malloc

ldw r1 @.__heapOffset ; super block

; load value of freeOffset from super block
add r2 r1 4
ldw r2 @r2

cmp r2 0
jz .__nextfree ; stop walking the free list if we reach 0x0

; load length from first free block
ldw r3 @r2


cmp r0 r3
jlt .__nextfree

;ble .__nextfree r3 r0; if requested size < free block length it fits

	; if length >= requested size + 12 split free block
		; remove from free list
		; create new free entry
		; add to list
	; add block to used list

.__nextfree

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

; globals
export .__heapOffset
w 0x0 ; Heap Offset

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Free a region of dynamically allocated memory.
;
; in r0 address of allocation
export .free

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The length must be a multiple of the word size.
; Source and destination must be in the same bank.
;
; r0 destination address
; r1 source address
; r2 length
export .memcpy

.__copyword

; copy word from source to destination
ldw r3 r1
stw r0 r3

sub r2 r2 2
add r0 r0 2
add r1 r1 2

bnz .__copyword r2

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The length must be a multiple of the word size.
;
; r0 address
; r1 value
; r2 length
export .memset

.__setword

; set word to given value
stw r0 r1

sub r2 r2 2
add r0 r0 2

bnz .__setword r2

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The length must be a multiple of the word size.
;
; in  r0 address
; out r3 length
export .strlen

mov r3 0

.__countchar

ldb r2 r0
add r3 r3 1
add r0 r0 1

bnz .__countchar r2

sub r3 r3 1

; return to caller
ldw r4 sp
add sp sp 2
jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The length must be a multiple of the word size.
;
; r0 destination address
; r1 source address
export .strcpy

.__copychar

ldb r2 r1
stw r0 r2
add r0 r0 1
add r1 r1 1

bnz .__copychar r2

; return to caller
ldw r4 sp
add sp sp 2
jmp r4
