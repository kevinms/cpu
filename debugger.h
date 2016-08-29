#ifndef __DEBUGGER_H
#define __DEBUGGER_H

int initTUI();
void freeTUI();
int updateTUI(int progOffset);
int loadDebugInfo(char *fileName, uint32_t baseAddr);

#endif /* __DEBUGGER_H */
