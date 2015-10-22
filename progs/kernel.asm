;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Kernel
; 
; Responsible for:
;   + Loading the standard library.
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

; I think the kernel could use malloc and free for all space allocation.
; Need space for a new task structure? malloc
; Need space for a binary and it's stack? malloc
; Only the Kernel and Standard Lib would have a static location.

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


