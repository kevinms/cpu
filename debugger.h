#ifndef __DEBUGGER_H
#define __DEBUGGER_H

#include "cpu.h"

/*
 * Initialize and free the Text User Interface (TUI).
 */
int initTUI();
void freeTUI();

/*
 * Parse the *.debug file to map assembly line numbers to program offsets.
 */
int loadDebugInfo(char *fileName, uint32_t baseAddr);

/*
 * Update the TUI and dump CPU state.
 */
int updateTUI(struct cpuState *cpu, struct instruction *o);
void dumpRegisters(struct cpuState *cpu, char *message, int printHeader);
void dumpMemory(struct cpuState *cpu, int width);

#endif /* __DEBUGGER_H */
