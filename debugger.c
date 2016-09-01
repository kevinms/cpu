#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>

#include "debugger.h"
#include "isa.h"

typedef struct DebugInfo
{
	char *sourceFile;
	char *debugFile;
	char *binaryFile;

	char **text;
	int lineCount;

	int *indexLine;
	int *indexOffset;
	int indexCount;

	uint32_t binarySize;
	uint32_t baseAddr;

	struct DebugInfo *next;
} DebugInfo;

typedef struct Window
{
	WINDOW *wn;
	int w, h;
	int x, y;
	char *title;
} Window;

static Window top;
static Window *code;
static Window *regs;
static Window *mem;
static Window *shell;

static DebugInfo *gInfo;
static int keepGoing;
static int simpleTUI = 0;

#define BP_PC 0
#define BP_LINE 1
#define BP_MEM_RD 2
#define BP_MEM_WR 3
#define BP_MEM_RDWR 4
#define BP_FINISH 5

struct breakpoint {
	int type;
	uint32_t condition; // Does this need to be 64 bit?
	uint64_t maxHits;
	uint64_t id;

	/*
	 * Generic storage for breakpoint type agnostic arguments.
	 */
	char opt1[512];
	char opt2[512];
	uint32_t num1;
	uint32_t num2;

	struct breakpoint *next;
};

struct bptype {
	int type;
	char *name;
};

static struct breakpoint *bp_list;
static uint64_t bp_list_count;
static struct bptype bp_table[] = {
	{BP_PC, "pc"},
	{BP_LINE, "line"},
	{BP_MEM_RD, "rd"},
	{BP_MEM_WR, "wr"},
	{BP_MEM_RDWR, "rdwr"},
	{BP_FINISH, "fin"}
};

static int shellPrint(const char *fmt, ...)
{
	wmove(shell->wn, shell->h - 1, 5);

	va_list argPtr;
	va_start(argPtr, fmt);
	if (simpleTUI != 0) {
		fprintf(stderr, fmt, argPtr);
	} else {
		vw_printw(shell->wn, fmt, argPtr);
		wrefresh(shell->wn);
		refresh();
	}
	va_end(argPtr);

	return 0;
}

DebugInfo *mapLineToOffset(char *file, int lineNum, uint32_t *progOffset)
{
	int i;
	DebugInfo *info;

	/*
	 * Find the source file that matches.
	 */
	for (info = gInfo; info != NULL; info = info->next) {
		if (strcmp(info->sourceFile, file) == 0) {
			/*
			 * Found debug info match.
			 */
			break;
		}
	}
	if (info == NULL) {
		shellPrint("Can't find file %s for line: %d", file, lineNum);
		return NULL;
	}

	/*
	 * Brute force search for program offset based on line number
	 * using the simple array mapping.
	 */
	for (i = 0; i < info->indexCount; i++) {
		if (info->indexLine[i] == lineNum) {
			/*
			 * Found match.
			 */
			*progOffset = info->indexOffset[i] + info->baseAddr;
			break;
		}
	}
	if (i >= info->indexCount) {
		shellPrint("Can't map line (%d) to offset for %s\n", lineNum, file);
		return NULL;
	}

	shellPrint("Mapped line (%d) to offset (0x%X)for %s\n", lineNum, *progOffset, file);

	return info;
}

void freeDebugInfo(DebugInfo *info)
{
	if (info == NULL) {
		return;
	}

	free(info->sourceFile);
	free(info->debugFile);
	free(info->binaryFile);

	int i;
	for (i = 0; i < info->lineCount; i++) {
		free(info->text[i]);
	}
	free(info->text);

	/*
	 * We allocate a single buffer and split it between indexLine
	 * and indexOffset.
	 */
	free(info->indexLine);

	free(info);
}

int loadDebugInfo(char *fileName, uint32_t baseAddr)
{
	DebugInfo *info;
	FILE *f;
	char line[4096];

	if ((info = malloc(sizeof(*info))) == NULL) {
		fprintf(stderr, "Can't allocate info: %s\n", strerror(errno));
		goto ERROR;
	}
	memset(info, 0, sizeof(*info));

	char *marker = strrchr(fileName, '.');
	if (marker == NULL) {
		fprintf(stderr, "Can't find file extension.\n");
		goto ERROR;
	}
	int len = (uintptr_t)marker - (uintptr_t)fileName;
	asprintf(&info->sourceFile, "%.*s.asm", len, fileName);
	asprintf(&info->debugFile, "%.*s.debug", len, fileName);
	asprintf(&info->binaryFile, "%.*s.bin", len, fileName);
	fprintf(stderr, "source: %s\n", info->sourceFile);
	fprintf(stderr, "debug: %s\n", info->debugFile);
	fprintf(stderr, "binary: %s\n", info->binaryFile);

	/*
	 * Load all source file lines of text.
	 */
	if ((f = fopen(info->sourceFile, "r")) == NULL) {
		fprintf(stderr, "Can't open '%s': %s\n", info->sourceFile, strerror(errno));
		goto ERROR;
	}

	while (fgets(line, sizeof(line), f) != NULL) {
		info->lineCount++;
	}
	fseek(f, 0, SEEK_SET);
	if ((info->text = malloc(info->lineCount * sizeof(*info->text))) == NULL) {
		fprintf(stderr, "Can't allocate text buffer: %s\n", strerror(errno));
		goto ERROR;
	}
	memset(info->text, 0, info->lineCount * sizeof(*info->text));

	int i = 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		info->text[i] = strdup(line);
		i++;
	}

#if 0
	fprintf(stderr, "Source file: %s:%d\n", info->sourceFile, info->lineCount);
	for (i = 0; i < info->lineCount; i++) {
		fprintf(stderr, "%d: %s", i+1, info->text[i]);
	}
#endif

	fclose(f);

	/*
	 * Index each lineNum to progOffset mapping.
	 */
	if ((f = fopen(info->debugFile, "r")) == NULL) {
		fprintf(stderr, "Can't open '%s': %s\n", info->debugFile, strerror(errno));
		goto ERROR;
	}

	int lineNum, progOffset;
	i = 0;
	while ((fscanf(f, "%d %X", &lineNum, &progOffset) == 2) && !feof(f)) {
		//fprintf(stderr, "%d 0x%X\n", lineNum, progOffset);
		i++;
	}
	fseek(f, 0, SEEK_SET);
	if ((info->indexLine = malloc(sizeof(*info->indexLine) * i * 2)) == NULL) {
		fprintf(stderr, "Can't allocate line and offset arrays: %s\n", strerror(errno));
		goto ERROR;
	}
	info->indexOffset = info->indexLine + i;
	info->indexCount = i;

	i = 0;
	while ((fscanf(f, "%d %X", &lineNum, &progOffset) == 2) && !feof(f)) {
		info->indexLine[i] = lineNum;
		info->indexOffset[i] = progOffset;
		i++;
	}

	fclose(f);

	struct stat statBuffer;
	if (stat(info->binaryFile, &statBuffer) < 0) {
		fprintf(stderr, "Can't stat '%s': %s\n", info->binaryFile, strerror(errno));
		goto ERROR;
	}
	info->binarySize = statBuffer.st_size;

	info->baseAddr = baseAddr;
	info->next = gInfo;
	gInfo = info;

	return 0;

ERROR:

	if (f != NULL) {
		fclose(f);
	}
	if (info != NULL) {
		free(info->sourceFile);
		free(info->debugFile);
		free(info->binaryFile);
		free(info->indexLine);
		free(info->indexOffset);
		if (info->text) {
			free(info->text);
		}
	}
	free(info);

	return -1;
}

void dumpRegisters(struct cpuState *cpu, char *message, int printHeader)
{
	int i;

	if (simpleTUI == 0) {
		return;
	}

	if (printHeader) {
		printf("%25s    pc   npc flags ", "");
		for (i = 0; i < NUM_REGISTERS; i++)
			if (i < 10) {
				printf("   r%d ", i);
			} else {
				printf("  r%d ", i);
			}
		printf("\n");
	}

	printf("%-25s", message == NULL ? "" : message);

	printf(" %5" PRIX32 " %5" PRIX32 " ", cpu->pc, cpu->nextPC);
	printf("%c%c%c%c  ",
	       R_FL & FL_N ? 'n' : ' ', R_FL & FL_Z ? 'z' : ' ',
	       R_FL & FL_V ? 'v' : ' ', R_FL & FL_C ? 'c' : ' ');
	for(i = 0; i < NUM_REGISTERS; i++)
		printf("%5" PRIX32 " ", cpu->r[i]);
	printf("\n");
}

void dumpMemory(struct cpuState *cpu, int width)
{
	int64_t i;
	uint64_t addr;
	uint8_t *line = NULL;
	int duplicate;

	duplicate = 0;
	for (addr = 0; addr < cpu->memSize; ++addr) {
		if ((addr % width) == 0) {
			if ((line != NULL) &&
				(addr + width < cpu->memSize) &&
				(memcmp(line, cpu->mem+addr, width) == 0)) {
				if (duplicate == 0) {
					shellPrint("\n*");
					duplicate = 1;
				}
				addr += (width-1);
				continue;
			}
			duplicate = 0;
			line = cpu->mem+addr;
			shellPrint("\n");
			shellPrint("0x%.4" PRIX64, addr);
		}
		shellPrint(" %.2x", cpu->mem[addr]);
		if ((((addr+1) % width) == 0) || (addr == cpu->memSize)) {
			shellPrint("  >");
			for (i = width - 1; i >= 0; --i) {
				char c = cpu->mem[addr-i];
				shellPrint("%c", isgraph(c) ? c : '.');
			}
			shellPrint("<");
		}
	}
	shellPrint("\n");
}

void deleteBreakpoint(uint64_t n)
{
	struct breakpoint *bp;
	struct breakpoint *prev;

	for (bp = bp_list, prev = NULL;
		 bp != NULL;
		 prev = bp, bp = bp->next) {
		if (bp->id == n) {
			shellPrint("Deleting breakpoint #%" PRIX64 "\n", bp->id);
			if (prev == NULL) {
				bp_list = bp->next;
			} else {
				prev->next = bp->next;
			}
			free(bp);
			break;
		}
	}
}

void deleteAllBreakpoints()
{
	while (bp_list != NULL) {
		deleteBreakpoint(bp_list->id);
	}
}

uint64_t addBreakpoint(char *args)
{
	struct breakpoint *bp;
	int type = -1;
	int i;
	char name[512];

	/*
	 * What type of breakpoint is  this?
	 */
	sscanf(args, "%s", name);
	for (i = 0; i < sizeof(bp_table) / sizeof(*bp_table); ++i) {
		if (strcmp(bp_table[i].name, name) == 0) {
			type = bp_table[i].type;
			break;
		}
	}
	if (type < 0) {
		shellPrint("Unknown breakpoint type: %d\n", type);
		return UINT64_MAX;
	}
	args += strlen(name);

	bp = malloc(sizeof(*bp));
	memset(bp, 0, sizeof(*bp));
	bp->type = type;
	bp->maxHits = UINT64_MAX;
	bp->condition = 0;

	/*
	 * Parse arguments.
	 */
	switch (type) {
		case BP_LINE:
			if (sscanf(args, "%s %d", bp->opt1, &bp->num1) != 2) {
				shellPrint("Line break format: <file> <line>\n");
				free(bp);
				return UINT64_MAX;
			}
			shellPrint("Setting breakpoint for %s:%d\n", bp->opt1, bp->num1);
			if (mapLineToOffset(bp->opt1, (int)bp->num1, &bp->condition) == NULL) {
				return UINT64_MAX;
			}
			break;
		case BP_PC:
			/*
			 * Use strtoull() so that it can interpret base for us.
			 */
			errno = 0;
			bp->condition = strtoull(args, NULL, 0);
			if (errno != 0) {
				shellPrint("Can't parse PC value: %s\n", strerror(errno));
				shellPrint("PC break format: <PC value>\n");
				free(bp);
				return UINT64_MAX;
			}
			break;
		case BP_MEM_RD:
		case BP_MEM_WR:
		case BP_MEM_RDWR:
		case BP_FINISH:
			break;
		default:
			shellPrint("How on earth did we get here?!\n");
			return UINT64_MAX;
	}

	/*
	 * Add to the global breakpoint list.
	 */
	bp->id = bp_list_count;
	bp_list_count++;
	bp->next = bp_list;
	bp_list = bp;

	shellPrint("Set breakpoint: %s 0x%" PRIX32 "\n", name, bp->condition);

	return(bp->id);
}

int hitBreakpoint(struct cpuState *cpu, struct instruction *o)
{
	struct breakpoint *hitBP = NULL;
	struct breakpoint *bp;
	uint64_t i;

	for (bp = bp_list, i = 0; bp != NULL; bp = bp->next, ++i) {
		switch (bp->type) {
			case BP_PC:
				if (cpu->pc == bp->condition) {
					shellPrint("Hit breakpoint #%" PRIX64 ": PC == 0x%" PRIX32 "\n", i, cpu->pc);
					hitBP = bp;
				}
				break;
			case BP_FINISH:
				if ((o->op == jmp) && ((o->mode & MODE_OPERAND) == OPR_REG) && (o->raw2 == 4)) {
					shellPrint("Hit breakpoint #%" PRIX64 ": jmp r4\n", i);
					hitBP = bp;
				}
				break;
			case BP_LINE:
				if (cpu->pc == bp->condition) {
					shellPrint("Hit breakpoint #%" PRIX64 ": line(%s:%d) or progOffset(0x%" PRIX64 ")\n",
					           i, bp->opt1, bp->num1, bp->condition);
					hitBP = bp;
				}
				break;
		}
	}

	if (hitBP != NULL && hitBP->maxHits != UINT64_MAX) {
		hitBP->maxHits--;
		if (hitBP->maxHits == 0) {
			deleteBreakpoint(hitBP->id);
		}
	}

	return(hitBP != NULL);
}

void listBreakpoints()
{
	struct breakpoint *bp;
	uint64_t i;

	shellPrint("Breakpoints:\n");
	for (bp = bp_list, i = 0; bp != NULL; bp = bp->next, ++i) {
		switch (bp->type) {
			case BP_LINE:
				shellPrint(" #%" PRIX64 " line(%s:%d) or progOffset(0x%" PRIX64 ")\n",
						   i, bp->opt1, bp->num1, bp->condition);
				break;
			default:
				shellPrint(" #%" PRIX64 " %s == %" PRIX64 "\n", i, bp_table[bp->type].name, bp->condition);
				break;
		}
	}
}

void freeWindow(Window *w)
{
	if (w == NULL) {
		return;
	}

	if (w->title != NULL) {
		free(w->title);
	}
	if (w->wn != NULL) {
    	delwin(w->wn);
	}
	free(w);
}

static Window *createWindow(Window *p, int h, int w, int y, int x, char *title)
{
	Window *win;

	if ((win = malloc(sizeof(*win))) == NULL) {
		fprintf(stderr, "Can't allocate window struct.\n");
		abort();
	}
	memset(win, 0, sizeof(*win));

	win->w = w;
	win->h = h;
	win->x = x;
	win->y = y;
	if (title != NULL) {
		win->title = strdup(title);
	}

	if ((win->wn = subwin(p->wn, win->h, win->w, win->y, win->x)) == NULL) {
		fprintf(stderr, "Can't create register window.\n");
		abort();
	}

	if (title != NULL) {
    	mvwaddstr(win->wn, 1, (win->w - strlen(win->title)) / 2, win->title);
	}

	return win;
}

static int updateCodeWindow(Window *code, int progOffset)
{
	int i;
	static int lineNum = 0;
	DebugInfo *info;

	/*
	 * Find the source file that matches.
	 */
	for (info = gInfo; info != NULL; info = info->next) {
		if (progOffset >= info->baseAddr && progOffset < info->baseAddr + info->binarySize) {
			break;
		}
	}
	if (info == NULL) {
		shellPrint("Can't find source file for progOffset: 0x%X", progOffset);
		return 0;
	}

	/*
	 * Brute force search for line number based on program offset
	 * using the simple array mapping.
	 */
	for (i = 0; i < info->indexCount; i++) {
		if (info->indexOffset[i] + info->baseAddr == progOffset) {
			/*
			 * Found match.
			 */
			lineNum = info->indexLine[i];
			break;
		}
	}
	if (i >= info->indexCount) {
		shellPrint("Can't map offset (0x%X) to line for %s\n", progOffset, info->sourceFile);
		return 0;
	}

	/*
	 * Print window title.
	 */
	wattron(code->wn, A_BOLD);
	wattron(code->wn, COLOR_PAIR(3));
	mvwprintw(code->wn, 0, 0, "%4s", info->sourceFile);
	wclrtoeol(code->wn);
	wattroff(code->wn, COLOR_PAIR(3));
	wattroff(code->wn, A_BOLD);

	/*
	 * We store each line of assembly text in an array indexed at 0.
	 * That means line #1 is in element 0 of the array. Adjust the
	 * lineNum here to account for this.
	 */
	lineNum--;

	/*
	 * Fill window with source lines centered around lineNum.
	 */
	int maxLines = code->h - 1;
	int nextLine = lineNum - maxLines / 2;
	if (nextLine < 0) {
		nextLine = 0;
	}
	for (i = 0; i < maxLines && nextLine < info->lineCount; i++, nextLine++) {
		wattron(code->wn, COLOR_PAIR(1));
		wattron(code->wn, A_BOLD);
		mvwprintw(code->wn, 1 + i, 2, "%4d", nextLine + 1);
		wattroff(code->wn, COLOR_PAIR(1));
		wattroff(code->wn, A_BOLD);

		if (nextLine == lineNum) {
			/*
			 * Highlight the next line to be executed.
			 */
			wattron(code->wn, COLOR_PAIR(2));
		}

		/*
		 * Count characters to print, expanding tabs.
		 */
		int charCount;
		int expandedCount = 0;
		for (charCount = 0; charCount < strlen(info->text[nextLine]); charCount++) {
			int len = 1;
			if (info->text[nextLine][charCount] == '\t') {
				len = 4;
			}
			if (expandedCount + len >= code->w - 7) {
				break;
			}
			expandedCount += len;
		}
		mvwprintw(code->wn, 1 + i, 7, "%.*s", expandedCount, info->text[nextLine]);

		if (nextLine == lineNum) {
			wattroff(code->wn, COLOR_PAIR(2));
		}
	}

    wrefresh(code->wn);
	
	return 0;
}

static int updateRegsWindow(Window *regs, struct cpuState *cpu)
{
	/*
	 * Print value of each register.
	 */
	int i;
	char buf[256];
	int maxRegStr = strlen("r15: -0x0000000000");

	/*
	 * Explicitly print PC register.
	 */
	sprintf(buf, "PC: 0x%" PRIX32, cpu->pc);
	sprintf(buf+strlen(buf), "%*s", (int)maxRegStr - (int)strlen(buf), " ");
    mvwaddstr(regs->wn, 2, 1, buf);

	/*
	 * Print the rest.
	 */
	for (i = 0; i < 16; i++) {
		int col = i >= 8 ? 1 : 0;
		sprintf(buf, "r%d: 0x%" PRIX32, i, cpu->r[i]);
		sprintf(buf+strlen(buf), "%*s", (int)maxRegStr - (int)strlen(buf), " ");
    	mvwaddstr(regs->wn, 3 + (i % 8), 1 + col * maxRegStr, buf);
	}

	wrefresh(regs->wn);

	return 0;
}

/*
 * -1 An error occurred.
 *  0 Keep accepting user commands.
 *  1 Give control back to the emulator.
 */
static int parseCommand(struct cpuState *cpu, char input[4096])
{
	char	cmd[4096];
	char	opt1[4096];
	uint64_t num;
	static char	savedInput[4096] = "s";

	opt1[0] = '\0';
	if (input[strlen(input)-1] == '\n') {
		input[strlen(input)-1] = '\0';
	}
	if (input[0] != '\0') {
		strncpy(savedInput, input, 4096);
	}
	if ((input[0] == '\0') &&
		(savedInput[0] != '\0')) {
		strncpy(input, savedInput, 4096);
	}
	if (input[0] == 's' || input[0] == 'n') {
		return 1;
	}
	if (input[0] == 'c') {
		keepGoing = 1;
		return 1;
	}
	if (input[0] == 'm') {
		sscanf(input, "%s %s", cmd, opt1);
		num = 16;
		if (opt1[0] != '\0') {
			num = strtoull(opt1, NULL, 0);
		}
		if (simpleTUI != 0) {
			//TODO: Support dumping memory to the shell window.
			dumpMemory(cpu, (int)num);
		}
	}
	if (input[0] == 'r') {
		dumpRegisters(cpu, cpu->msg, 0);
	}
	if (input[0] == 'f') {
		addBreakpoint("fin");
		keepGoing = 1;
		return 1;
	}
	if (input[0] == 'b') {
		if (strlen(input) >= 3) {
			addBreakpoint(input+2);
		} else {
			listBreakpoints();
		}
	}
	if (input[0] == 'd') {
		sscanf(input, "%s %s", cmd, opt1);
		if (opt1[0] != '\0') {
			num = strtoull(opt1, NULL, 0);
			deleteBreakpoint(num);
		} else {
			deleteAllBreakpoints();
		}
	}
	if (input[0] == 'q') {
		return 1;
	}
	if (input[0] == 'h') {
		shellPrint("s - step forward one instruction\n");
		shellPrint("c - continue until breakpoint or end of execution\n");
		shellPrint("m - print memory contents\n");
		shellPrint("r - print register contents\n");
		shellPrint("b - list or add breakpoints\n");
		shellPrint("d - delete breakpoints\n");
		shellPrint("f - continue until return from function\n");
	}

	return 0;
}

static sig_atomic_t caughtSignal;
static sigset_t caughtSignalSet;

void signalHandler(int signal)
{
	caughtSignal = 1;

	/*
	 * Add the signal to a set, so we can log it outside of the signal handler.
	 */
	sigaddset(&caughtSignalSet, signal);

	/*
	 *  Disable flushing of the input buffer when an interrupt key
	 *  (kill, suspend or quit) is pressed.
	 */
	noqiflush();
}

int signalsToCatch[] = {SIGHUP, SIGINT, SIGTERM};

static int catchSignals()
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = signalHandler;
	action.sa_flags = SA_RESTART;

	sigemptyset(&action.sa_mask);

	int i;
	for (i = 0; i < sizeof(signalsToCatch) / sizeof(*signalsToCatch); i++) {
		if (sigaction(signalsToCatch[i], &action, NULL) < 0) {
			shellPrint("Can't catch signal %s: %s\n",
					   strsignal(signalsToCatch[i]), strerror(errno));
			return -1;
		}
	}

	return 0;
}

static int uncatchSignals()
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = SIG_DFL;

	int i;
	for (i = 0; i < sizeof(signalsToCatch) / sizeof(*signalsToCatch); i++) {
		if (sigaction(signalsToCatch[i], &action, NULL) < 0) {
			shellPrint("Can't uncatch signal %s: %s\n",
					   strsignal(signalsToCatch[i]), strerror(errno));
			return -1;
		}
	}

	sigemptyset(&caughtSignalSet);
	caughtSignal = 0;

	return 0;
}

static inline int checkForSignals()
{
	if (caughtSignal > 0) {
		int i;
		for (i = 0; i < _NSIG; i++) {
			if (sigismember(&caughtSignalSet, i) > 0) {
				shellPrint("Halting execution -- caught signal: %s\n",
						   strsignal(i));
			}
		}

		return 1;
	}

	return 0;
}

static int updateShellWindow(Window *shell, struct cpuState *cpu, struct instruction *o)
{
	char	input[4096];

	if (checkForSignals() > 0) {
		keepGoing = 0;
	}

	if (hitBreakpoint(cpu, o) != 0) {
		keepGoing = 0;
	}

	if (keepGoing != 0) {
		return 0;
	}

	/*
	 * Remove signal handlers while waiting for shell input.
	 */
	uncatchSignals();

	while (1) {
		/*
		 * Capture the next shell command.
		 */
		echo();
		mvwprintw(shell->wn, shell->h - 1, 1, "#> ");
		mvwgetstr(shell->wn, shell->h - 1, 4, input);
		noecho();
		wrefresh(shell->wn);

		/*
		 * Execute the command.
		 */
		if (parseCommand(cpu, input) != 0) {
			break;
		}
	}

	/*
	 * Set signal handlers before handing control back to the emulator.
	 */
	catchSignals();

	return 0;
}

int updateSimple(struct cpuState *cpu, struct instruction *o)
{
	char	input[4096];

	if (hitBreakpoint(cpu, o) != 0) {
		keepGoing = 0;
	}

	if (keepGoing != 0) {
		return 0;
	}

	printf("#> ");

	while (fgets(input, 4096, stdin) != NULL) {
		if (parseCommand(cpu, input) != 0) {
			break;
		}
		printf("#> ");
	}

	return 0;
}

int initTUI()
{
	/*
	 * Must initialize a few signal handling variables.
	 */
	sigemptyset(&caughtSignalSet);

	if (simpleTUI != 0) {
		return 0;
	}

    if ((top.wn = initscr()) == NULL) {
		fprintf(stderr, "Error initialising ncurses.\n");
		exit(1);
    }
	getmaxyx(top.wn, top.h, top.w);

	if (has_colors() == FALSE) {
		endwin();
		fprintf(stderr, "Your terminal does not support color\n");
		exit(1);
	}
	if (can_change_color() == FALSE) {
		fprintf(stderr, "Your terminal does not support changing color\n");
	}
	start_color();
	init_pair(1, COLOR_YELLOW, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_WHITE);
	init_pair(3, COLOR_BLUE, COLOR_BLACK);

    /*
	 * Switch of echoing and enable keypad (for arrow keys).
	 */
	cbreak();
	noecho();
	keypad(top.wn, TRUE);
	set_tabsize(4);

	int i;
	int w, h;
	int y;

	w = top.w;
	h = 2 + top.h / 2;
	code = createWindow(&top, h, w, 0, 0, "Code");

	int maxRegStr = strlen("r15: -0x0000000000");
	w = 2 + 1 + 2 * maxRegStr;
	h = 2 + 2 + 8; // border + title + blank + 8 rows
	regs = createWindow(&top, h, w, code->h, 0, "Registers");
	box(regs->wn, 0, 0);

	for (i = 0; i < 16; i++) {
		int col = i >= 8 ? 1 : 0;
		char buf[256];
		sprintf(buf, "r%d: -0x0000000000", i);
    	mvwaddstr(regs->wn, 3 + (i % 8), 1 + col * maxRegStr, buf);
	}

	w = top.w - regs->w;
	h = regs->h;
	mem = createWindow(&top, h, w, code->h, regs->w, "Memory");
	box(mem->wn, 0, 0);

	w = top.w;
	y = code->h + regs->h;
	h = top.h - y;
	shell = createWindow(&top, h, w, y, 0, NULL);
	scrollok(shell->wn, TRUE);

	updateCodeWindow(code, 0);

    refresh();

	return 0;
}

void freeTUI()
{
	if (simpleTUI != 0) {
		return;
	}

	freeWindow(code);
	freeWindow(mem);
	freeWindow(regs);
	freeWindow(shell);

	deleteAllBreakpoints();

	while (gInfo != NULL) {
		DebugInfo *info = gInfo;
		gInfo = info->next;
		freeDebugInfo(info);
	}

    delwin(top.wn);
    endwin();
    refresh();
}

int updateTUI(struct cpuState *cpu, struct instruction *o)
{
	if (simpleTUI != 0) {
		updateSimple(cpu, o);
	} else {
		updateCodeWindow(code, cpu->pc);
		updateRegsWindow(regs, cpu);
		updateShellWindow(shell, cpu, o);
	}

	return 0;
}
