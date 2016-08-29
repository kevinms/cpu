#ifndef __DEBUGGER_H
#define __DEBUGGER_H

#include "cpu.h"

int initTUI();
void freeTUI();
int updateTUI(struct cpuState *cpu);
int loadDebugInfo(char *fileName, uint32_t baseAddr);

#endif /* __DEBUGGER_H */
