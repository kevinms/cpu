;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Kernel
; 
; Responsible for:
;   + Creating the heap.
;   + Task management and switching.
;

; Notes:
; We assume kernel space always starts at 0x0.

; Memory layout:
; 
; 0                                                                   N
; +--------+----------------+----------+--------------+---------------+
; | Kernel | Kernel's Stack | MMap I/O | Standard Lib |     Heap      |
; +--------+----------------+----------+--------------+---------------+

.kernel 0x0
.kstack	0x1000
.mmapIO 0x2000
.stdlib 0x3000
.heap   0x10000

; I think the kernel could use malloc and free for all space allocation.
; Need space for a new task structure? malloc
; Need space for a binary and it's stack? malloc
; Only the Kernel and Standard Lib would have a static location.

; struct task {
;     uint32_t id;
;     uint32_t progOffset;
;     uint32_t progLength;
; };

mov r0 1
mov r1 1
add r2 r0 r1

;
; Initialize the heap.
;
mov r0 .heap     ; 16 * 4K
mov r1 0x1FF0000 ; 32M - (16 * 4K)
sub sp sp 4
stw sp .ra_init_malloc
jmp .init_malloc
.ra_init_malloc

;
; Allocate the very first task structure!
;
.task_size 12
mov r0 .task_size
sub sp sp 4
stw sp .ra_task
jmp .malloc
.ra_task

;
; Allocate another one!
;
mov r0 .task_size
sub sp sp 4
stw sp .ra_task2
jmp .malloc
.ra_task2

die

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Create a new task structure.
; Allocate memory for the binary and it's stack.
; Load the binary from storage device.
; Set BA (base address).
; Jump to beginning of binary.
; 
; r0 SD card offset of binary
; r1 binary length
.new_task


