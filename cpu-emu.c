#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <getopt.h>

#include "isa.h"

#define log(...) \
	do { \
		sprintf(msg, ##__VA_ARGS__); \
	} while(0)

// Registers
// r0  to r7                General Purpose
//        r8  --> sp        Stack Pointer
//        r9  --> bn        Bank Number
//        r10               Unused
// r11 to r12 --> p0 to p1  Port Mapped I/O
// r13 to r16 --> c0 to c4  64bit Hardware Counter

#define FL_CARRY 0x1
uint16_t r[16], pc, fl;
uint8_t mem[65536];

uint64_t 	ic;
uint16_t 	nextPC;
char		msg[4096];
int			beInteractive = 0;
uint64_t	maxCycles = UINT64_MAX;
char		*romFile = NULL;
char		*blobFile = NULL;
uint16_t	blobOffset = 0x400;


struct instruction {
	uint8_t op;
	uint16_t opr0, opr1, opr2;
	uint16_t raw0, raw1, raw2;
	int m0, m1, m2;
};

void initEnvironment()
{
	memset(mem, 0, sizeof(mem));
	memset(r, 0, sizeof(r));
	pc = 0;
	fl = 0;
}

int loadBinaryBlob(char *path, uint16_t offset)
{
	int		fd;
	uint8_t	buf[65536];
	struct stat	statBuffer;

	printf("loading binary blob.\n");

	if ((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "Failed open %s: %s\n", path, strerror(errno));
		exit(1);
	}

	if (fstat(fd, &statBuffer) < 0) {
		fprintf(stderr, "Failed stat(%s,): %s\n", path, strerror(errno));
		exit(1);
	}

	if (statBuffer.st_size > sizeof(buf)) {
		fprintf(stderr, "Blob must be less then or equal to 64 KB.");
		exit(1);
	}

	memset(buf, 0, sizeof(buf));
	pread(fd, buf, statBuffer.st_size, 0);

	memcpy(mem+offset, buf, statBuffer.st_size);

	printf("%c%c%c%c%c\n", mem[0x400], mem[0x401], mem[0x402], mem[0x403], mem[0x404]);

	return(0);
}

void read8bit(char *bits, uint8_t *memory)
{
	*(uint8_t *)memory = (uint8_t)strtoul(bits, NULL, 2);
}

void read16bit(char *bits, uint8_t *memory)
{
	*(uint16_t *)memory = htons((uint16_t)strtoul(bits, NULL, 2));
}

int loadROM(char *path)
{
	FILE	*fd;
	char	buf[512];
	int		returnValue;

	if ((fd = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Failed open %s: %s\n", path, strerror(errno));
		return(-1);
	}

	returnValue = 0;
	while (fscanf(fd, "%s", buf) != EOF) {
		if (buf[0] == '#' || buf[0] == '\n' || buf[0] == ' ' ||
			buf[0] == '\t' || buf[0] == '/' || buf[0] == ';')
			continue;

		int c = strlen(buf);
		if (c == 8) {
			read8bit(buf, mem+pc);
			pc += 1;
		} else if (c == 16) {
			read16bit(buf, mem+pc);
			pc += 2;
		}
	}

	pc = 0;
	fclose(fd);
	return(returnValue);
}

uint16_t
fetchInst(uint16_t lpc, struct instruction *o)
{
	uint8_t *i = mem + lpc;

	o->op = i[0];

	o->raw0 = (i[2] << 8) | i[3]; o->opr0 = o->raw0; o->m0 = 0;
	o->raw1 = (i[4] << 8) | i[5]; o->opr1 = o->raw1; o->m1 = 0;
	o->raw2 = (i[6] << 8) | i[7]; o->opr2 = o->raw2; o->m2 = 0;
	if (i[1] & MODE_BITA) { o->opr0 = r[o->opr0]; o->m0 = 1; }
	if (i[1] & MODE_BITB) { o->opr1 = r[o->opr1]; o->m1 = 1; }
	if (i[1] & MODE_BITC) { o->opr2 = r[o->opr2]; o->m2 = 1; }

	return(lpc + 8);
} 

void dumpMemory(int width)
{
	uint64_t i;
	uint64_t addr;
	uint8_t *line = NULL;
	int duplicate;

	duplicate = 0;
	for (addr = 0; addr < UINT16_MAX; ++addr) {
		if ((addr % width) == 0) {
			if ((line != NULL) &&
				(addr + width < UINT64_MAX) &&
				(memcmp(line, mem+addr, width) == 0)) {
				if (duplicate == 0) {
					printf("\n*");
					duplicate = 1;
				}
				addr += (width-1);
				continue;
			}
			duplicate = 0;
			line = mem+addr;
			printf("\n");
			printf("0x%.4" PRIX64, addr);
		}
		printf(" %.2X", mem[addr]);
		if ((((addr+1) % width) == 0) ||
			(addr == UINT16_MAX)) {
			printf("  >");
			i = width;
			for (; i > 0; --i) {
				char c = mem[addr-i];
				printf("%c", isgraph(c) ? c : '.');
			}
			printf("<");
		}
	}
	printf("\n");
}

void dumpRegisters(int printHeader, uint16_t nextPC, char *message)
{
	int i;

	if (printHeader) {
		printf("%25s    pc   npc flags ", "");
		for (i = 0; i < 8; i++)
			printf("   r%d ", i);
		printf("\n");
	}

	printf("%-25s", message == NULL ? "" : message);

	printf(" %5" PRIu16 " %5" PRIu16 " ", pc, nextPC);
	printf("%c%c%c%c  ",
	       fl & FL_N ? 'n' : ' ', fl & FL_Z ? 'z' : ' ',
	       fl & FL_V ? 'v' : ' ', fl & FL_C ? 'c' : ' ');
	for(i = 0; i < 8; i++)
		printf("%5" PRIu16 " ", r[i]);
	printf("\n");
}

#define BP_PC 0
#define BP_IC 1
#define BP_MEM_RD 2
#define BP_MEM_WR 3
#define BP_MEM_RDWR 4
#define BP_FINISH 5

struct breakpoint {
	int type;
	uint64_t condition;
	uint64_t maxHits;
	uint64_t id;
	struct breakpoint *next;
};

struct bptype {
	int type;
	char *name;
};

struct breakpoint *bp_list;
uint64_t bp_list_count;
struct bptype bp_table[] = {
	{BP_PC, "pc"},
	{BP_IC, "ic"},
	{BP_MEM_RD, "rd"},
	{BP_MEM_WR, "wr"},
	{BP_MEM_RDWR, "rdwr"},
	{BP_FINISH, "fin"}
};

void deleteBreakpoint(uint64_t n)
{
	struct breakpoint *bp;
	struct breakpoint *prev;
	uint64_t i;

	for (bp = bp_list, i = 0, prev = NULL;
		 bp != NULL;
		 prev = bp, bp = bp->next, ++i) {
		if (i == n) {
			printf("Deleting breakpoint #%" PRIu64 "\n", i);
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
	struct breakpoint *bp;
	uint64_t i;

	for (bp = bp_list, i = 0;
		 bp != NULL;
		 bp = bp->next, ++i);

	while (i-- > 0) {
		deleteBreakpoint(i);
	}
}

uint64_t addBreakpoint(char *name, uint64_t condition, uint64_t maxHits)
{
	struct breakpoint *bp;
	int type = -1;
	int i;

	for(i = 0; i < sizeof(bp_table); ++i) {
		if (strcmp(bp_table[i].name, name) == 0) {
			type = bp_table[i].type;
			break;
		}
	}
	if (type < 0) {
		fprintf(stderr, "Unknown breakpoint type: %d\n", type);
		return(UINT64_MAX);
	}

	bp = malloc(sizeof(*bp));

	bp->type = type;
	bp->condition = condition;
	bp->maxHits = maxHits;
	bp->id = bp_list_count;

	bp_list_count++;
	bp->next = bp_list;
	bp_list = bp;

	printf("Set breakpoint: %s %" PRIu64 "\n", name, condition);

	return(bp->id);
}

int hitBreakpoint()
{
	struct breakpoint *hitBP = NULL;
	struct breakpoint *bp;
	uint64_t i;

	for (bp = bp_list, i = 0; bp != NULL; bp = bp->next, ++i) {
		switch (bp->type) {
		case BP_PC:
			if (pc == (uint16_t)bp->condition) {
				printf("Hit breakpoint #%" PRIu64 ": PC == 0x%" PRIX16 "\n", i, pc);
				hitBP = bp;
			}
			break;
		case BP_FINISH: {
			struct instruction o;
			fetchInst(pc, &o);

			if ((o.op == jmp) && (o.m0 != 0) && (o.raw0 == 4)) {
				printf("Hit breakpoint #%" PRIu64 ": jmp r8\n", i);
				hitBP = bp;
			}
			break;
		}
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

	printf("Breakpoints:\n");
	for (bp = bp_list, i = 0; bp != NULL; bp = bp->next, ++i) {
		printf(" #%" PRIu64 " %s == %" PRIu64 "\n", i, bp_table[bp->type].name, bp->condition);
	}
}

void interactive()
{
	char	input[4096];
	char	cmd[4096];
	char	opt1[4096];
	char	opt2[4096];
	char	savedInput[4096];
	uint64_t num;
	static int keepGoing;

	if (beInteractive == 0) {
		return;
	}

	if (hitBreakpoint() != 0) {
		keepGoing = 0;
	}

	if (keepGoing != 0) {
		return;
	}

	strcpy(savedInput, "s");

	printf("#> ");

	while (fgets(input, 4096, stdin) != NULL) {
		opt1[0] = '\0';
		input[strlen(input)-1] = '\0';
		if (input[0] != '\0') {
			strncpy(savedInput, input, 4096);
		}
		if ((input[0] == '\0') &&
			(savedInput[0] != '\0')) {
			strncpy(input, savedInput, 4096);
		}
		if (input[0] == 's') {
			break;
		}
		if (input[0] == 'c') {
			keepGoing = 1;
			break;
		}
		if (input[0] == 'm') {
			sscanf(input, "%s %s", cmd, opt1);
			num = 16;
			if (opt1[0] != '\0') {
				num = strtoull(opt1, NULL, 0);
			}
			dumpMemory((int)num);
		}
		if (input[0] == 'r') {
			dumpRegisters(0, nextPC, msg);
		}
		if (input[0] == 'f') {
			addBreakpoint("fin", 0, 1);
			keepGoing = 1;
			break;
		}
		if (input[0] == 'b') {
			sscanf(input, "%s %s %s", cmd, opt1, opt2);
			if (opt1[0] != '\0') {
				num = strtoull(opt2, NULL, 0);
				addBreakpoint(opt1, num, UINT64_MAX);
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
		if (input[0] == 'h') {
			printf("s - step forward one instruction\n");
			printf("c - continue until breakpoint or end of execution\n");
			printf("m - print memory contents\n");
			printf("r - print register contents\n");
			printf("b - list or add breakpoints\n");
			printf("d - delete breakpoints\n");
			printf("f - continue until return from function\n");
		}

		printf("#> ");
	}
}

static struct option longopts[] = {
	{"rom", required_argument, NULL, 'r'},
	{"blob", required_argument, NULL, 'b'},
	{"blob-offset", required_argument, NULL, 'o'},
	{"max-cycles", required_argument, NULL, 'c'},
	{"interactive", no_argument, NULL, 'i'},
	{"help", no_argument, NULL, 'h'}
};

char *optdesc[] = {
	"The ROM to load into memory.",
	"A blob to place in memory.",
	"Where to place the blob in memory.",
	"Emulator will exit after N cycles.",
	"Interactive debugging mode.",
	"This help."
};

char *optstring = NULL;

void usage(int argc, char **argv)
{
	int i;
	int numOptions;
	int longest;

	printf("%s [OPTION...]\n\n", argv[0]);

	numOptions = sizeof(longopts) / sizeof(*longopts);
	longest = 0;
	for (i = 0; i < numOptions; ++i) {
		if (longest < strlen(longopts[i].name)) {
			longest = strlen(longopts[i].name);
		}
	}
	for (i = 0; i < numOptions; ++i) {
		printf("--%-*s, -%c : %s\n", longest, longopts[i].name, longopts[i].val, optdesc[i]);
	}
	printf("\n");
}

void parseArgs(int argc, char **argv)
{
	int c, i;
	int longindex;
	int numOptions;

	numOptions = sizeof(longopts) / sizeof(*longopts);
	optstring = malloc(numOptions * 3);
	memset(optstring, 0, numOptions * 3);

	c = 0;
	for (i = 0; i < numOptions; ++i) {
		c += sprintf(optstring+c, "%c%s", (char)longopts[i].val,
					 longopts[i].has_arg == no_argument ? "" :
					 longopts[i].has_arg == required_argument ? ":" :
					 "::");
	}

	while ((c = getopt_long(argc, argv, optstring, longopts, &longindex)) >= 0) {
		switch (c) {
			case 'r':
				romFile = optarg;
				break;
			case 'b':
				blobFile = optarg;
				break;
			case 'o':
				blobOffset = strtoull(optarg, NULL, 0);
				break;
			case 'i':
				beInteractive = 1;
				break;
			case 'c':
				maxCycles = strtoull(optarg, NULL, 0);
				break;
			case 'h':
				usage(argc, argv);
				break;
			case '?':
				break;
			default:
				exit(1);
		}
	}

	if (romFile == NULL) {
		fprintf(stderr, "Expected a ROM file path.\n");
		exit(1);
	}
}

int main(int argc, char **argv)
{
	int			stop;
	uint16_t	opr0, opr1, opr2;
	uint8_t		*i;

	parseArgs(argc, argv);

	initEnvironment();
	loadROM(romFile);

	if (blobFile != NULL) {
		loadBinaryBlob(blobFile, blobOffset);
	}

	dumpRegisters(1, 0, "");

	ic = 0;
	pc = 0;
	stop = 0;
	while (!stop && (maxCycles-- > 0)) {

		interactive();

		i = mem + pc;
		nextPC = pc + 8;

		opr0 = (i[2] << 8) | i[3];
		opr1 = (i[4] << 8) | i[5];
		opr2 = (i[6] << 8) | i[7];
		opr0 = i[1] & MODE_BITA ? r[opr0] : opr0;
		opr1 = i[1] & MODE_BITB ? r[opr1] : opr1;
		opr2 = i[1] & MODE_BITC ? r[opr2] : opr2;

		//printf("%" PRIu8 " %" PRIu8 " %" PRIu16 " %" PRIu16 " %" PRIu16 "\n",
		//		i[0], i[1], opr0, opr1, opr2);

		//printf("i: %d\n", i[0] >> 2);

		switch (i[0]) {

		case nop:
			log("nop\n");
			break;
		case add:
			log("add r[%" PRIu16 "] = %" PRIu16 " + %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 + opr2;
			if ((UINT16_MAX - opr1) < opr2) {
				fl |= FL_CARRY;
			}
			break;
		case sub:
			log("sub r[%" PRIu16 "] = %" PRIu16 " - %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 - opr2;
			break;
		case adc:
			log("adc r[%" PRIu16 "] = %" PRIu16 " + %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 + opr2 + (fl & FL_CARRY);
			if ((UINT16_MAX - opr1) < opr2) {
				fl |= FL_CARRY;
			}
			break;
		case sbc:
			log("sbc r[%" PRIu16 "] = %" PRIu16 " - %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 - opr2;
			break;
		case mul:
			log("mul r[%" PRIu16 "] = %" PRIu16 " * %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 * opr2;
			break;
		case div:
			log("div r[%" PRIu16 "] = %" PRIu16 " / %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 / opr2;
			break;
		case ldb:
			log("ldb r[%" PRIu16 "] = mem[%" PRIu16 "]", opr0, opr1);
			r[opr0] = mem[opr1];
			break;
		case stb:
			log("stb mem[%" PRIu16 "] = %" PRIu16, opr0, opr1);
			mem[opr0] = opr1 & 0xFF;
			break;
		case ldw:
			log("ldw r[%" PRIu16 "] = mem[%" PRIu16 "]", opr0, opr1);
			r[opr0] = (mem[opr1] << 8) | mem[opr1+1];
			break;
		case stw:
			log("stw mem[%" PRIu16 "] = %" PRIu16, opr0, opr1);
			mem[opr0] = opr1 >> 8;
			mem[opr0+1] = opr1 & 0xFF;
			break;
		case mov:
			log("mov r[%" PRIu16 "] = %" PRIu16, opr0, opr1);
			r[opr0] = opr1;
			break;
		case and:
			log("and r[%" PRIu16 "] = %" PRIu16 " & %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 & opr2;
			break;
		case or:
			log("or r[%" PRIu16 "] = %" PRIu16 " | %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 | opr2;
			break;
		case xor:
			log("xor r[%" PRIu16 "] = %" PRIu16 " ^ %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 ^ opr2;
			break;
		case nor:
			log("nor r[%" PRIu16 "] = ~%" PRIu16 " & ~%" PRIu16, opr0, opr1, opr2);
			r[opr0] = ~opr1 & ~opr2;
			break;
		case lsl:
			log("lsl r[%" PRIu16 "] = %" PRIu16 " << %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 << opr2;
			break;
		case lsr:
			log("lsr r[%" PRIu16 "] = %" PRIu16 " >> %" PRIu16, opr0, opr1, opr2);
			r[opr0] = opr1 >> opr2;
			break;
		case bez:
			log("bez %" PRIu16 " %" PRIu16 " == 0", opr0, opr1);
			if (opr1 == 0) {
				nextPC = opr0;
				fl |= FL_Z;
			}
			break;
		case bnz:
			log("bnz %" PRIu16 " %" PRIu16 " == 0", opr0, opr1);
			if (opr1 != 0) {
				nextPC = opr0;
				fl &= ~FL_Z;
			}
			break;
		case ble:
			log("ble %" PRIu16 " %" PRIu16 " <= 0", opr0, opr1);
			if (opr1 <= 0) {
				nextPC = opr0;
			}
			break;
		case bge:
			log("bge %" PRIu16 " %" PRIu16 " >= 0", opr0, opr1);
			if (opr1 >= 0) {
				nextPC = opr0;
			}
			break;
		case bne:
			log("bne %" PRIu16 " %" PRIu16 " != %" PRIu16, opr0, opr1, opr2);
			if (opr1 != opr2) {
				nextPC = opr0;
			}
			break;
		case beq: break;
			log("beq %" PRIu16 " %" PRIu16 " == %" PRIu16, opr0, opr1, opr2);
			if (opr1 == opr2) {
				nextPC = opr0;
			}
			break;
		case jmp:
			log("jmp %" PRIu16, opr0);
			nextPC = opr0;
			break;
		case die:
			log("die");
			stop = 1;
			break;
		default:
			fprintf(stderr, "Unknown opcode: %" PRIu16 " %" PRIu16 " %" PRIu16
					" %" PRIu16 "\n", i[0], i[1], i[2], i[3]);
			break;
		}
		dumpRegisters(0, nextPC, msg);

		pc = nextPC;
	}
	interactive();

	return(0);
}
