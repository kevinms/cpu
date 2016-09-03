;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Standard Library
;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Globals
;

;TODO: why is the heapOffset using absolute addressing?
;      could i just modify the base offset so everything
;      is position independant (relative)?

; All code below expects to access the __heapOffsedt using absolute addressing.
; This means the standard "lib" must be assembled with a static memory
; position, i.e. assembler.py --base-address=<offset> -a lib.asm
.__heapOffset
w 0x0 ; Heap Offset

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Constants
;

; Heap structure sizes.
.__heapSuper         12
.__heapHeader        12
.__heapTrailer       4
.__heapHeaderTrailer 16
.__heapSmallest      17 ; Smallest allowed object. (header + 1 + trailer)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Initialize the dynamic memory allocator.
;
; in @r0 desired heap offset
; in  r1 desired heap size
export .init_malloc

	mov r2 .__heapOffset
	stw @r2 r0

	add r3 r0 r1 ; end of heap offset
	add r2 r0 12 ; free object header offset
	sub r3 r3 4  ; free object trailer offset
	sub r4 r1 .__heapSuper
	sub r4 r4 .__heapHeaderTrailer ; free object length

	; init super block
	stw @r0 r1  ; size
	add r0 r0 4
	stw @r0 r2  ; freeOffset
	add r0 r0 4
	stw @r0 0x0 ; usedOffset (no used blocks to start out with)

	; init free object trailer
	stw @r3 r2

	; init free block header
	stw @r2 r4  ; length
	add r2 r2 4
	stw @r2 0x0 ; prevOffset
	add r2 r2 4
	stw @r2 0x0 ; nextOffset

	; return to caller
	ldw r4 sp
	add sp sp 4
	jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Dynamically allocate a region of memory.
;
; in   r0 requested size (bytes)
; out @r3 address of allocation (0 on failure)
export .malloc

	ldw r1 @.__heapOffset ; super block offset

	add r1 r1 4
	ldw r1 @r1  ; super.freeOffset

	;
	; Walk the free list looking for a candidate.
	; r1 - (this) candidate offset
	;
	.__l_malloc_walk
	cmp r1 0
	jz .__l_malloc_return

		ldw r2 @r1 ; this.length
		cmp r2 r0
		jg .__l_malloc_detach

		add r1 r1 8
		ldw r1 @r1 ; this.nextOffset
		jmp .__l_malloc_walk

	;
	; Detach from free list.
	;
	.__l_malloc_detach
	add r2 r1 4
	add r3 r1 8
	ldw r2 @r2  ; this.prevOffset
	ldw r3 @r3  ; this.nextOffset

	cmp r2 0
	jnz .__l_malloc_freehead
		ldw r4 @.__heapOffset
		add r4 r4 4
		stw @r4 r3 ; super.freeOffset = this.nextOffset
	.__l_malloc_freehead

	cmp r2 0
	jz .__l_malloc_noprev
		add r4 r2 8
		stw @r4 r3 ; prev.nextOffset = this.nextOffset
	.__l_malloc_noprev

	cmp r3 0
	jz .__l_malloc_nonext
		add r4 r3 8
		stw @r4 r2 ; next.prevOffset = this.prevOffset
	.__l_malloc_nonext

	;
	; Do we need to split?
	;
	; +--------------------+     +--------+-----------+
	; |      This Obj      |  => |  This  |  New Free |
	; +--------------------+     +--------+-----------+
	; ^        ^           ^     ^        ^           ^
	; '--------'-----------'     '--------'-----------'
	; Requested   Leftover       Requested   Leftover
	;   Size       Space           Size       Space
	;  
	ldw r2 @r1 ; this.length
	add r3 r0 .__heapSmallest
	cmp r2 r3
	jl .__l_malloc_attach

		;
		; Initialize a new trailer for "this".
		;
		add r2 r1 .__heapHeader
		add r2 r2 r0 ; new trailer for "this".
		stw @r2 r1   ; trailer.headerOffset = this

		;
		; Initialize fields of new free object header.
		; r2 - (new) free object offset
		;
	 	add r2 r2 .__heapTrailer ; (new) New free object header.

		ldw r3 @r1
		sub r3 r3 r0                   ; tmp = this.size - requestedSize
		sub r3 r3 .__heapHeaderTrailer ; tmp = tmp - .__heapHeaderTrailer
		stw @r2 r3                     ; new.size = tmp

		add r3 r2 r3
		add r3 r3 .__heapHeader        ; Trailer for new free object.
		stw @r3 r2                     ; trailer.headerOffset = new

		;
		; Attach new free object to free list.
		;
		add r3 r2 4
		stw @r3 0 ; new.prevOffset = 0

		ldw r3 @.__heapOffset
		add r3 r3 4
		ldw r3 @r3 ; super.freeOffset

		add r4 r2 8
		stw @r4 r3 ; new.nextOffset = super.freeOffset

		ldw r3 @.__heapOffset
		add r3 r3 4
		stw @r3 r2 ; super.freeOffset = new

		;
		; Update "this" object's size.
		;
		or r2 r0 0x80000000 ; Set the 32nd bit to indicate it is used.
		stw @r1 r2          ; this.size = requestedSize

	;
	; Attach this object to used list.
	;
	.__l_malloc_attach
	add r2 r1 4
	stw @r2 0   ; this.prevOffset = 0

	ldw r3 @.__heapOffset
	add r3 r3 8
	ldw r4 @r3 ; super.usedOffset
	add r2 r1 8
	stw @r2 r4 ; this.nextOffset = super.usedOffset
	stw @r3 r1 ; super.usedOffset = this

	.__l_malloc_return ; Return to caller.
	add r3 r1 12 ; Return addres of new block's data.
	ldw r4 sp
	add sp sp 4
	jmp r4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Free a region of dynamically allocated memory.
;
; in @r0 address of allocation
export .free

	sub r0 r0 12 ; this.header

	;
	; If the 32nd bit is not set, something is very wrong.
	;
	ldw r1 @r0 ; this.header.length
	and r1 r1 0x80000000
	jnz .__l_free_valid
		;
		; Something is very wrong... Possibly, the caller gave us a
		; bogus address.
		;
		jmp .__l_free_return
	.__l_free_valid

	;
	; Detach from used list.
	;
	.__l_free_detach
	add r1 r0 4
	add r2 r0 8
	ldw r1 @r1  ; this.prevOffset
	ldw r2 @r2  ; this.nextOffset

	cmp r1 0
	jnz .__l_free_usedhead
		ldw r3 @.__heapOffset
		add r3 r3 4
		stw @r3 r2 ; super.freeOffset = this.nextOffset
	.__l_free_usedhead

	cmp r1 0
	jz .__l_free_noprev
		add r3 r1 8
		stw @r3 r2 ; prev.nextOffset = this.nextOffset
	.__l_free_noprev

	cmp r2 0
	jz .__l_free_nonext
		add r3 r2 8
		stw @r3 r1 ; next.prevOffset = this.prevOffset
	.__l_free_nonext

	;
	; Do we need to coalesce with left and right neighbors?
	;
	; +---------------------------+
	; | Left N. | This | Right N. |
	; +---------------------------+
	;

	;
	; Check if the left neighbor is free.
	;
	sub r1 r0 4
	ldw r1 @r1  ; left.trailer.headerOffset
	ldw r1 @r1  ; left.header.length

	and r2 r1 0x80000000
	cmp r2 0
	jz .__l_free_leftused
		;TODO: Coalesce
	.__l_free_leftused

	;
	; Check if the right neighbor is free.
	;
	ldw r1 @r0   ; this.length
	and r1 r1 0x7FFFFFFF
	add r1 r1 16 ; right.header
	ldw r2 @r1   ; right.header.length

	and r2 r2 0x80000000
	cmp r2 0
	jz .__l_free_rightused
		;TODO: Coalesce
	.__l_free_rightused

	;
	; Attach this object to free list.
	;
	add r1 r0 4
	stw @r1 0   ; this.prevOffset = 0

	ldw r2 @.__heapOffset
	add r2 r2 4
	ldw r3 @r2 ; super.freeOffset

	add r2 r0 8
	stw @r1 r3 ; this.nextOffset = super.freeOffset
	stw @r2 r0 ; super.freeOffset = this

	;
	; Clear the 32nd bit to indicate it is free.
	;
	ldw r1 @r0
	and r1 r1 0x7FFFFFFF ; ~0x80000000
	stw @r0 r1

	.__l_free_return

	; return to caller
	ldw r4 sp
	add sp sp 4
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

	cmp r2 0
	jnz .__copyword

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

	cmp r2 0
	jnz .__setword

	; return to caller
	ldw r4 sp
	add sp sp 4
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

	cmp r2 0
	jnz .__countchar

	sub r3 r3 1

	; return to caller
	ldw r4 sp
	add sp sp 4
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

	cmp r2 0
	jnz .__copychar

	; return to caller
	ldw r4 sp
	add sp sp 4
	jmp r4
