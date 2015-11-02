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
#define R_SP r[11]
#define R_BA r[12]
#define R_FL r[13]
#define R_C1 r[14]
#define R_C2 r[15]

/*
 * Memory Mapped I/O:
 */
uint32_t mmapIOstart = 0;
uint32_t mmapIOend = 0; //0x9C
struct cpuState {
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
} cpu;

#define NUM_REGISTERS 16
uint32_t r[NUM_REGISTERS], pc;
uint8_t mem[32*1024*1024] = {0};

uint64_t 	ic;
uint32_t 	nextPC;
uint32_t	startingPC;
char		msg[4096];
int			beInteractive = 0;
uint64_t	maxCycles = UINT64_MAX;
char		*romFile = NULL;

struct instruction {
	uint8_t op;
	uint8_t mode;
	uint8_t reg0;
	uint8_t reg1;

	uint32_t raw2;
	uint32_t opr0, opr1, opr2;
};

struct binary {
	char *filePath;
	uint32_t memoryOffset;
	struct binary *next;
};

struct binary *gBinaryList;

#define byteSwap16(x) \
	((((x) & 0xFF) << 8) | \
	 (((x) & 0xFF00) >> 8))

#define byteSwap32(x) \
	((((x) & 0xFF) << 24) | \
	 (((x) & 0xFF00) << 8) | \
	 (((x) & 0xFF0000) >> 8) | \
	 (((x) & 0xFF000000) >> 24))

uint16_t littleToHost16(uint16_t x)
{
	x = byteSwap16(x);
	return ntohs(x);
}

uint16_t hostToLittle16(uint16_t x)
{
	x = htons(x);
	return byteSwap16(x);
}

uint32_t littleToHost32(uint32_t x)
{
	x = byteSwap32(x);
	return ntohl(x);
}

uint32_t hostToLittle32(uint32_t x)
{
	x = htonl(x);
	return byteSwap32(x);
}

void initEnvironment()
{
	memset(mem, 0, sizeof(mem));
	memset(r, 0, sizeof(r));
	pc = 0;
}

int addBinary(char *binaryInfo)
{
	struct binary *newBinary;
	char *separator, *path, *offset;

	/*
	 * Parse path and offset from binaryInfo.
	 */
	if (((separator = rindex(binaryInfo, ':')) == NULL) ||
		(separator == binaryInfo) || (strlen(separator) <= 1)) {
		fprintf(stderr, "Expected binaryInfo as <filePath>:<memoryOffset>\n");
		return(-1);
	}
	separator[0] = '\0';
	path = binaryInfo;
	offset = separator + 1;

	newBinary = malloc(sizeof(*newBinary));
	newBinary->filePath = strdup(path);
	newBinary->memoryOffset = strtoull(offset, NULL, 0);

	newBinary->next = gBinaryList;
	gBinaryList = newBinary;

	return(0);
}

int loadBinary(struct binary *binary)
{
	int		fd;
	struct stat	statBuffer;

	printf("Loading %s at 0x%" PRIX32 "\n", binary->filePath, binary->memoryOffset);

	if ((fd = open(binary->filePath, O_RDONLY)) < 0) {
		fprintf(stderr, "Can't open %s: %s\n", binary->filePath, strerror(errno));
		exit(1);
	}

	if (fstat(fd, &statBuffer) < 0) {
		fprintf(stderr, "Failed stat(%s,): %s\n", binary->filePath, strerror(errno));
		exit(1);
	}

	if ((binary->memoryOffset > sizeof(mem)) ||
		(statBuffer.st_size > sizeof(mem) - binary->memoryOffset)) {
		fprintf(stderr, "Binary does not fit in memory.\n");
		exit(1);
	}

	void *buffer = ((void *)mem) + binary->memoryOffset;
	uint32_t bytesLeft = statBuffer.st_size;

	while (bytesLeft > 0) {
		ssize_t bytesRead;

		if ((bytesRead = read(fd, buffer, bytesLeft)) < 0) {
			fprintf(stderr, "Can't read %s: %s\n", binary->filePath, strerror(errno));
			close(fd);
			return(-1);
		}

		bytesLeft -= (uint32_t)bytesRead;
		buffer += bytesRead;
	}

	return(0);
}

void parse8bit(char *bits, uint8_t *memory)
{
	*(uint8_t *)memory = (uint8_t)strtoul(bits, NULL, 2);
}

void parse16bit(char *bits, uint8_t *memory)
{
	*(uint32_t *)memory = hostToLittle16((uint32_t)strtoul(bits, NULL, 2));
}

void parse32bit(char *bits, uint8_t *memory)
{
	*(uint32_t *)memory = hostToLittle32((uint32_t)strtoul(bits, NULL, 2));
}

int loadROM(char *path)
{
	FILE	*fd;
	char	buf[512];
	int		returnValue;

	/*
	 * Parse the base address from the path string.
	 */

	if ((fd = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Can't open %s: %s\n", path, strerror(errno));
		return(-1);
	}

	returnValue = 0;
	while (fscanf(fd, "%s", buf) != EOF) {
		if (buf[0] == '#' || buf[0] == '\n' || buf[0] == ' ' ||
			buf[0] == '\t' || buf[0] == '/' || buf[0] == ';')
			continue;

		int c = strlen(buf);
		if (c == 8) {
			parse8bit(buf, mem+pc);
			pc += 1;
		} else if (c == 16) {
			parse16bit(buf, mem+pc);
			pc += 2;
		} else if (c == 32) {
			parse32bit(buf, mem+pc);
			pc += 4;
		}
	}

	pc = 0;
	fclose(fd);
	return(returnValue);
}

uint32_t
fetchInst(uint32_t lpc, struct instruction *o)
{
	uint8_t *i = mem + lpc;

	/*
	 * Decode the instruction into it's logical pieces.
	 */
	o->op = i[0];
	o->mode = i[1];
	o->reg0 = i[2];
	o->reg1 = i[3];
	o->raw2 = littleToHost32(*(uint32_t *)(i + 4));

	/*
	 * Set final operand values based on address mode.
	 * Operand 0 and 1 are always register direct.
	 * Operand 2 is based on the mode bit.
	 */
	o->opr0 = r[o->reg0];
	o->opr1 = r[o->reg1];
	o->opr2 = o->raw2;
	if ((o->mode & MODE_OPERAND) == OPR_REG) {
		o->opr2 = r[o->raw2];
	}

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
		printf(" %.2x", mem[addr]);
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

void dumpRegisters(int printHeader, uint32_t nextPC, char *message)
{
	int i;

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

	printf(" %5" PRIu32 " %5" PRIu32 " ", pc, nextPC);
	printf("%c%c%c%c  ",
	       R_FL & FL_N ? 'n' : ' ', R_FL & FL_Z ? 'z' : ' ',
	       R_FL & FL_V ? 'v' : ' ', R_FL & FL_C ? 'c' : ' ');
	for(i = 0; i < NUM_REGISTERS; i++)
		printf("%5" PRIu32 " ", r[i]);
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

	for (bp = bp_list, prev = NULL;
		 bp != NULL;
		 prev = bp, bp = bp->next) {
		if (bp->id == n) {
			printf("Deleting breakpoint #%" PRIu64 "\n", bp->id);
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
			if (pc == (uint32_t)bp->condition) {
				printf("Hit breakpoint #%" PRIu64 ": PC == 0x%" PRIX32 "\n", i, pc);
				hitBP = bp;
			}
			break;
		case BP_FINISH: {
			struct instruction o;
			fetchInst(pc, &o);

			if ((o.op == jmp) && ((o.mode & MODE_OPERAND) == OPR_REG) && (o.raw2 == 4)) {
				printf("Hit breakpoint #%" PRIu64 ": jmp r4\n", i);
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
	static char	savedInput[4096] = "s";
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
		if (input[0] == 's' || input[0] == 'n') {
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
		if (input[0] == 'q') {
			break;
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
	{"binary", required_argument, NULL, 'b'},
	{"max-cycles", required_argument, NULL, 'c'},
	{"interactive", no_argument, NULL, 'i'},
	{"starting-pc", required_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'}
};

char *optdesc[] = {
	"The ROM to load into memory.",
	"A binary to place in memory as <binaryPath>:<memoryOffset>.",
	"Emulator will exit after N cycles.",
	"Interactive debugging mode.",
	"Starting program counter value as <memoryOffset>.",
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
				addBinary(optarg);
				break;
			case 'i':
				beInteractive = 1;
				break;
			case 'c':
				maxCycles = strtoull(optarg, NULL, 0);
				break;
			case 'p':
				startingPC = strtoull(optarg, NULL, 0);
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

	if (romFile == NULL && gBinaryList == NULL) {
		fprintf(stderr, "Expected a ROM or binary file path.\n");
		exit(1);
	}
}

struct SPI {
	uint8_t tx, rx;
};

int SPIsend(uint8_t data)
{
	return(0);
}

uint8_t SPIrecv()
{
	return(0);
}

uint32_t getAddress(uint8_t mode, uint32_t offset)
{
	if ((mode & MODE_ADDRESS) == ADDR_REL) {
		/*
		 * Relative Address.
		 */
		return(R_BA + offset);
	} else {
		/*
		 * Absolute Address.
		 */
		return(offset);
	}
}

uint32_t *mmapIOregister(uint32_t address)
{
	if (address >= 0x0 && address < 0x4) {
		/*
		 * Global Interrupt Control
		 */
		return &cpu.intGlobalControl;
	}
	if (address >= 0x4 && address < 0x8) {
		/*
		 * Pending Interrupts.
		 */
		return &cpu.intPending;
	}
	if (address >= 0x8 && address < 0xC) {
		/*
		 * Per-Interrupt Control
		 */
		return &cpu.intControl;
	}
	if (address >= 0xC && address < 0x8C) {
		int i = (0xC - address) / 4;
		/*
		 * Interrupt Handler Vector
		 */
		return &cpu.intVector[i];
	}

	if (address >= 0x8C && address < 0x90) {
		/*
		 * Timer 1 Terminal Count
		 */
		return &cpu.timerTerminalCount1;
	}
	if (address >= 0x90 && address < 0x94) {
		/*
		 * Timer 1 Control
		 */
		return &cpu.timerControl1;
	}
	if (address >= 0x94 && address < 0x98) {
		/*
		 * Timer 2 Terminal Count
		 */
		return &cpu.timerTerminalCount2;
	}
	if (address >= 0x98 && address < 0x9C) {
		/*
		 * Timer 2 Control
		 */
		return &cpu.timerControl2;
	}

	return(NULL);
}

uint8_t read8bit(uint32_t address)
{
	return mem[address];
}

uint32_t read32bit(uint32_t address)
{
	if (address < mmapIOstart || address >= mmapIOend) {
		/*
		 * Normal memory access.
		 */
		return *(uint32_t *)(mem + address);
	}

	/*
	 * Anything else must be memory mapped I/O.
	 */
	address -= mmapIOstart;

	uint32_t *reg = mmapIOregister(address);

	if (reg == NULL) {
		fprintf(stderr, "Can't get memory mapped register to read.\n");
		exit(1);
	}

	return(*reg);
}

void write8bit(uint32_t address, uint8_t data)
{
	mem[address] = data;
}

void write32bit(uint32_t address, uint32_t data)
{
	if (address < mmapIOstart || address >= mmapIOend) {
		/*
		 * Normal memory access.
		 */
		*(uint32_t *)(mem + address) = data;

		return;
	}

	/*
	 * Anything else must be memory mapped I/O.
	 */
	address -= mmapIOstart;

	uint32_t *reg = mmapIOregister(address);

	if (reg == NULL) {
		fprintf(stderr, "Can't get memory mapped register to write.\n");
		exit(1);
	}
	*reg = data;
}

int main(int argc, char **argv)
{
	int			stop;
	struct instruction o;
	uint32_t address;

	parseArgs(argc, argv);

	initEnvironment();
	if ((romFile != NULL) &&
		(loadROM(romFile) < 0)) {
		return(1);
	}

	if (gBinaryList != NULL) {
		struct binary *thisBinary;

		for (thisBinary = gBinaryList;
			 thisBinary != NULL;
			 thisBinary = thisBinary->next) {
			if (loadBinary(thisBinary) < 0) {
				return(1);
			}
		}
	}

	ic = 0;
	pc = startingPC;
	stop = 0;

	dumpRegisters(1, 0, "");

	while (!stop && (maxCycles-- > 0)) {

		interactive();
		fetchInst(pc, &o);

		nextPC = pc + 8;

		R_C1++;
		R_C2++;

		switch (o.op) {

		case nop:
			log("nop\n");
			break;

		/*
		 * Arithmetic operations.
		 */
		case add:
			log("add r[%" PRIu32 "] = %" PRIu32 " + %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 + o.opr2;
			if ((UINT32_MAX - o.opr1) < o.opr2) {
				R_FL |= FL_C;
			}
			break;
		case sub:
			log("sub r[%" PRIu32 "] = %" PRIu32 " - %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 - o.opr2;
			break;
		case adc:
			log("adc r[%" PRIu32 "] = %" PRIu32 " + %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 + o.opr2 + (R_FL & FL_C);
			if ((UINT32_MAX - o.opr1) < o.opr2) {
				R_FL |= FL_C;
			}
			break;
		case sbc:
			log("sbc r[%" PRIu32 "] = %" PRIu32 " - %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 - o.opr2;
			break;
		case mul:
			log("mul r[%" PRIu32 "] = %" PRIu32 " * %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 * o.opr2;
			break;
		case div:
			log("div r[%" PRIu32 "] = %" PRIu32 " / %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 / o.opr2;
			break;

		/*
		 * Load and stores.
		 */
		case ldb:
			address = getAddress(o.mode, o.opr2);
			log("ldb r[%" PRIu32 "] = mem[%" PRIX32 "]", o.reg0, address);
			r[o.reg0] = read8bit(address);
			break;
		case ldw:
			address = getAddress(o.mode, o.opr2);
			log("ldw r[%" PRIu32 "] = mem[%" PRIX32 "]", o.reg0, address);
			r[o.reg0] = littleToHost32(read32bit(address));
			break;
		case stb:
			address = getAddress(o.mode, o.opr0);
			log("stb mem[%" PRIX32 "] = %" PRIu32, address, o.opr2);
			write8bit(address, (uint8_t)o.opr2);
			break;
		case stw:
			address = getAddress(o.mode, o.opr0);
			log("stw mem[%" PRIX32 "] = %" PRIu32, address, o.opr2);
			write32bit(address, hostToLittle32(o.opr2));
			break;
		case mov:
			log("mov r[%" PRIu32 "] = %" PRIu32, o.reg0, o.opr2);
			r[o.reg0] = o.opr2;
			break;

		/*
		 * Bitwise operations.
		 */
		case and:
			log("and r[%" PRIu32 "] = %" PRIu32 " & %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 & o.opr2;
			break;
		case or:
			log("or r[%" PRIu32 "] = %" PRIu32 " | %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 | o.opr2;
			break;
		case xor:
			log("xor r[%" PRIu32 "] = %" PRIu32 " ^ %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 ^ o.opr2;
			break;
		case nor:
			log("nor r[%" PRIu32 "] = ~%" PRIu32 " & ~%" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = ~o.opr1 & ~o.opr2;
			break;
		case lsl:
			log("lsl r[%" PRIu32 "] = %" PRIu32 " << %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 << o.opr2;
			break;
		case lsr:
			log("lsr r[%" PRIu32 "] = %" PRIu32 " >> %" PRIu32, o.reg0, o.opr1, o.opr2);
			r[o.reg0] = o.opr1 >> o.opr2;
			break;

		/*
		 * Branches and jumps.
		 */
		case bez:
			address = getAddress(o.mode, o.opr2);
			log("bez %" PRIu32 " %" PRIu32 " == 0", address, o.opr0);
			if (o.opr0 == 0) {
				nextPC = address;
				R_FL |= FL_Z;
			}
			break;
		case bnz:
			address = getAddress(o.mode, o.opr2);
			log("bnz %" PRIu32 " %" PRIu32 " == 0", address, o.opr0);
			if (o.opr0 != 0) {
				nextPC = address;
				R_FL &= ~FL_Z;
			}
			break;
		case ble:
			address = getAddress(o.mode, o.opr2);
			log("ble %" PRIu32 " %" PRIu32 " <= 0", address, o.opr0);
			if (o.opr0 <= 0) {
				nextPC = address;
			}
			break;
		case bge:
			address = getAddress(o.mode, o.opr2);
			log("bge %" PRIu32 " %" PRIu32 " >= 0", address, o.opr0);
			if (o.opr0 >= 0) {
				nextPC = address;
			}
			break;
		case bne:
			address = getAddress(o.mode, o.opr2);
			log("bne %" PRIu32 " %" PRIu32 " != %" PRIu32, address, o.opr0, o.opr1);
			if (o.opr0 != o.opr1) {
				nextPC = address;
			}
			break;
		case beq: break;
			address = getAddress(o.mode, o.opr2);
			log("beq %" PRIu32 " %" PRIu32 " == %" PRIu32, address, o.opr0, o.opr1);
			if (o.opr0 == o.opr1) {
				nextPC = address;
			}
			break;
		case jmp:
			address = getAddress(o.mode, o.opr2);
			log("jmp %" PRIu32, address);
			nextPC = address;
			break;

		/*
		 * Others.
		 */
		case die:
			log("die");
			stop = 1;
			break;
		default:
			fprintf(stderr, "Unknown opcode: %" PRIu32 "\n", o.op);
			stop = 1;
			break;
		}
		dumpRegisters(0, nextPC, msg);

		pc = nextPC;
	}
	interactive();

	return(0);
}