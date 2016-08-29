#ifndef __CPU_H
#define __CPU_H

#include <inttypes.h>

struct flags {
	uint32_t n : 1, // negative
	         z : 1, // zero
	         o : 1, // overflow
	         c : 1; // carry
};

/*
 * Registers:
 *
 * r0  to r10      General Purpose
 *        r11  sp  Stack Pointer
 *        r12  ba  Base Address
 *        r13  fl  CPU Flags
 *        r14  c1  Timer 1 Counter
 *        r15  c2  Timer 2 Counter
 */
#define R_SP cpu.r[11]
#define R_BA cpu.r[12]
#define R_FL cpu.r[13]
#define R_C1 cpu.r[14]
#define R_C2 cpu.r[15]
#define flags (*((struct flags *)(cpu.r+13)))

#define NUM_REGISTERS 16

struct cpuState {
	/*
	 * Memory Mapped I/O:
	 */
	uint32_t mmapIOstart;
	uint32_t mmapIOend;

	/*
	 * Interrupts.
	 */
	uint32_t intGlobalControl;
	uint32_t intPending;
	uint32_t intControl;
	uint32_t intVector[32];

	/*
	 * Timers (counters).
	 */
	uint32_t timerTerminalCount1;
	uint32_t timerControl1;
	uint32_t timerTerminalCount2;
	uint32_t timerControl2;

	/*
	 * Registers and memory.
	 */
	uint32_t	r[NUM_REGISTERS], pc;
	uint32_t	memSize;
	uint8_t		*mem;
	char		*memoryFile;

	uint64_t 	ic;
	uint32_t 	nextPC;
	uint32_t	startingPC;
	uint64_t	maxCycles;

	char		msg[4096];
};

struct instruction {
	uint8_t op;
	uint8_t mode;
	uint8_t reg0;
	uint8_t reg1;

	uint32_t raw2;
	uint32_t opr0, opr1, opr2;
};

#endif /* __CPU_H */
