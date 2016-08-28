#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <string.h>

typedef struct Window
{
	WINDOW *wn;
	int w, h;
	int x, y;
	char *title;
} Window;

Window top;

Window *createWindow(Window *p, int h, int w, int y, int x, char *title)
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
	win->title = strdup(title);

	if ((win->wn = subwin(p->wn, win->h, win->w, win->y, win->x)) == NULL) {
		fprintf(stderr, "Can't create register window.\n");
		abort();
	}
	box(win->wn, 0, 0);
    mvwaddstr(win->wn, 1, (win->w - strlen(win->title)) / 2, win->title);

	return win;
}

int main(void)
{
    int ch;

    if ((top.wn = initscr()) == NULL) {
		fprintf(stderr, "Error initialising ncurses.\n");
		return 1;
    }
	getmaxyx(top.wn, top.h, top.w);

    /*
	 * Switch of echoing and enable keypad (for arrow keys).
	 */
	noecho();
	keypad(top.wn, TRUE);

	int w, h;
	int x, y;

	w = top.w;
	h = 2 + top.h / 2;
	Window *code = createWindow(&top, h, w, 0, 0, "Code");

	int maxRegStr = strlen("r15: -0x0000000000");
	w = 2 + 1 + 2 * maxRegStr;
	h = 2 + 2 + 8; // border + title + blank + 8 rows
	Window *regs = createWindow(&top, h, w, code->h, 0, "Registers");

	int i;
	for (i = 0; i < 16; i++) {
		int col = i >= 8 ? 1 : 0;
		char buf[256];
		sprintf(buf, "r%d: -0x0000000000", i);
    	mvwaddstr(regs->wn, 3 + (i % 8), 1 + col * maxRegStr, buf);
	}

	w = top.w - regs->w;
	h = regs->h;
	Window *mem = createWindow(&top, h, w, code->h, regs->w, "Memory");

	w = top.w;
	y = code->h + regs->h;
	h = top.h - y;
	Window *shell = createWindow(&top, h, w, y, 0, "Shell");

    refresh();

    /*  Loop until user hits 'q' to quit  */

    while ( (ch = getch()) != 'q' ) {

		switch ( ch ) {
			case KEY_UP:
			case KEY_DOWN:
			case KEY_LEFT:
			case KEY_RIGHT:
			case KEY_HOME:
			case KEY_END:
				break;
		}

		refresh();
    }


    /*  Clean up after ourselves  */

    delwin(top.wn);
    endwin();
    refresh();

    return EXIT_SUCCESS;
}
