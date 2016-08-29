#ifndef __DEBUGGER_H
#define __DEBUGGER_H

#include "cpu.h"

int initTUI();
void freeTUI();
int updateTUI(struct cpuState *cpu, struct instruction *o);
int loadDebugInfo(char *fileName, uint32_t baseAddr);

#endif /* __DEBUGGER_H */
