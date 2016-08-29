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

#include "debugger.h"

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

static Window *createWindow(Window *p, int h, int w, int y, int x, char *title)
{
	Window *win;

	if ((win = malloc(sizeof(*win))) == NULL) {
		fprintf(stderr, "Can't allocate window struct.\n");
		abort();
	}

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
	while ((fscanf(f, "%X %X", &lineNum, &progOffset) == 2) && !feof(f)) {
		//fprintf(stderr, "0x%X 0x%X\n", lineNum, progOffset);
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
	while ((fscanf(f, "%X %X", &lineNum, &progOffset) == 2) && !feof(f)) {
		/*
 		 * Store mapping in a simple array, for now.
 		 * We index lines starting at "0" rather than "1" -- adjust.
 		 */
		info->indexLine[i] = lineNum - 1;
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
		mvwprintw(shell->wn, 2, 2, "Can't find source file for progOffset:"
		          " 0x%X", progOffset);
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
		mvwprintw(shell->wn, 2, 2, "Can't find offset (0x%X) -> line"
		          " mapping.\n", progOffset);
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

	//TODO: do we want a box?
#if 0
	wattron(code->wn, COLOR_PAIR(2));
	box(code->wn, 0, 0);
	wattroff(code->wn, COLOR_PAIR(2));
#endif

    wrefresh(code->wn);
	
	return 0;
}

static int updateRegsWindow(Window *regs, struct cpuState *cpu)
{
	/*
	 * Print value of each register.
	 */
	int i;
	int maxRegStr = strlen("r15: -0x0000000000");
	for (i = 0; i < 16; i++) {
		int col = i >= 8 ? 1 : 0;
		char buf[256];
		sprintf(buf, "r%d: 0x%" PRIX32, i, cpu->r[i]);
		sprintf(buf+strlen(buf), "%*s", (int)maxRegStr - (int)strlen(buf), " ");
    	mvwaddstr(regs->wn, 3 + (i % 8), 1 + col * maxRegStr, buf);
	}

	wrefresh(regs->wn);

	return 0;
}

static int updateShellWindow(Window *shell, struct cpuState *cpu)
{
	char buf[256] = {0};

	/*
	 * Capture the next shell command.
	 */
	echo();
	mvwprintw(shell->wn, shell->h - 2, 2, "#> ");
	mvwgetstr(shell->wn, shell->h - 2, 5, buf);
	noecho();
	wscrl(shell->wn, 1);
	wrefresh(shell->wn);

	/*
	 * Execute the command.
	 */
	

	return 0;
}

int initTUI()
{
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
	init_color(COLOR_BLACK, 0, 0, 0);
	init_pair(1, COLOR_YELLOW, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_WHITE);
	init_pair(3, COLOR_BLUE, COLOR_BLACK);

    /*
	 * Switch of echoing and enable keypad (for arrow keys).
	 */
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
    /*  Clean up after ourselves  */
	if (code != NULL) {
    	delwin(code->wn);
	}
	if (mem != NULL) {
    	delwin(mem->wn);
	}
	if (regs != NULL) {
    	delwin(regs->wn);
	}
	if (shell != NULL) {
    	delwin(shell->wn);
	}
    delwin(top.wn);
    endwin();
    refresh();
}

/*
 * TODO: Really... the progOffset is an unsigned 32 bit integer.
 * We happen to know that fits in an "int" on this 64 bit machine.
 * This must change when the code becomes self hosted.
 */
int updateTUI(struct cpuState *cpu)
{
	updateCodeWindow(code, cpu->pc);
	updateRegsWindow(regs, cpu);
	updateShellWindow(shell, cpu);

	return 0;
}
