#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <getopt.h>

#include "isa.h"
#include "cpu.h"
#include "debugger.h"

#define log(...) \
	do { \
		sprintf(cpu.msg, ##__VA_ARGS__); \
	} while(0)

struct binary {
	char *filePath;
	uint32_t memoryOffset;
	int debugOnly;
	struct binary *next;
};

static struct binary *gBinaryList;

static int		beInteractive;
static int		tui;
static char		*romFile;

static struct cpuState cpu;

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

int openMemoryFile()
{
	int fd;
	int openFlags;
	int mode;

	if (cpu.memoryFile == NULL) {
		fprintf(stderr, "Must provide a memory file.\n");
		return(-1);
	}

	openFlags = O_RDWR | O_CREAT;
	mode = S_IRWXU | S_IRWXG;

	if ((fd = open(cpu.memoryFile, openFlags, mode)) < -1) {
		fprintf(stderr, "Can't open %s: %s\n", cpu.memoryFile, strerror(errno));
		return(-1);
	}

	/*
	 * Size the new image.
	 */
	if (ftruncate(fd, 0) < 0) {
		fprintf(stderr, "Can't truncate to zero: %s\n", strerror(errno));
		close(fd);
		return(-1);
	}
	if (ftruncate(fd, cpu.memSize) < 0) {
		fprintf(stderr, "Can't truncate to %" PRIu32 ": %s\n",
				cpu.memSize, strerror(errno));
		close(fd);
		return(-1);
	}

	if ((cpu.mem = mmap(NULL, cpu.memSize, PROT_READ | PROT_WRITE,
						MAP_SHARED, fd, 0)) == MAP_FAILED) {
		fprintf(stderr, "Can't memory map memory file: %s\n",
				strerror(errno));
		close(fd);
		return(-1);
	}
	close(fd);

	return(0);
}

int initEnvironment()
{
	cpu.pc = 0;
	cpu.mmapIOstart = 0;
	cpu.mmapIOend = 0; //0x9C
	cpu.maxCycles = UINT64_MAX;
	cpu.memSize = 32 * 1024 * 1024;
	cpu.memoryFile = strdup("emulator.memory");

	if (openMemoryFile() < 0) {
		return -1;
	}
	memset(cpu.mem, 0, cpu.memSize);

	memset(cpu.r, 0, sizeof(cpu.r));

	return 0;
}

int addBinary(char *binaryInfo, int debugOnly)
{
	struct binary *newBinary;
	char *separator, *path, *offset;

	/*
	 * Parse path and offset from binaryInfo.
	 */
	if (((separator = rindex(binaryInfo, ':')) == NULL) ||
		(separator == binaryInfo) || (strlen(separator) <= 1)) {
		fprintf(stderr, "Expected binaryInfo as <filePath>:<memoryOffset>\n");
		return -1;
	}
	separator[0] = '\0';
	path = binaryInfo;
	offset = separator + 1;

	newBinary = malloc(sizeof(*newBinary));
	newBinary->filePath = strdup(path);
	newBinary->memoryOffset = strtoull(offset, NULL, 0);
	newBinary->debugOnly = debugOnly;

	newBinary->next = gBinaryList;
	gBinaryList = newBinary;

	return 0;
}

int loadBinary(struct binary *binary)
{
	int		fd;
	struct stat	statBuffer;

	if (binary->debugOnly != 0) {
		return 0;
	}
	printf("Loading %s at 0x%" PRIX32 "\n", binary->filePath, binary->memoryOffset);

	if ((fd = open(binary->filePath, O_RDONLY)) < 0) {
		fprintf(stderr, "Can't open %s: %s\n", binary->filePath, strerror(errno));
		exit(1);
	}

	if (fstat(fd, &statBuffer) < 0) {
		fprintf(stderr, "Failed stat(%s,): %s\n", binary->filePath, strerror(errno));
		exit(1);
	}

	if ((binary->memoryOffset > cpu.memSize) ||
		(statBuffer.st_size > cpu.memSize - binary->memoryOffset)) {
		fprintf(stderr, "Binary does not fit in memory.\n");
		exit(1);
	}

	void *buffer = ((void *)cpu.mem) + binary->memoryOffset;
	uint32_t bytesLeft = statBuffer.st_size;

	while (bytesLeft > 0) {
		ssize_t bytesRead;

		if ((bytesRead = read(fd, buffer, bytesLeft)) < 0) {
			fprintf(stderr, "Can't read %s: %s\n", binary->filePath, strerror(errno));
			close(fd);
			return -1;
		}

		bytesLeft -= (uint32_t)bytesRead;
		buffer += bytesRead;
	}

	return 0;
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
			parse8bit(buf, cpu.mem + cpu.pc);
			cpu.pc += 1;
		} else if (c == 16) {
			parse16bit(buf, cpu.mem + cpu.pc);
			cpu.pc += 2;
		} else if (c == 32) {
			parse32bit(buf, cpu.mem + cpu.pc);
			cpu.pc += 4;
		}
	}

	cpu.pc = 0;
	fclose(fd);
	return(returnValue);
}

uint32_t
fetchInst(uint32_t lpc, struct instruction *o)
{
	uint8_t *i = cpu.mem + lpc;

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
	o->opr0 = cpu.r[o->reg0];
	o->opr1 = cpu.r[o->reg1];
	o->opr2 = o->raw2;
	if ((o->mode & MODE_OPERAND) == OPR_REG) {
		o->opr2 = cpu.r[o->raw2];
	}

	return(lpc + 8);
} 

void dumpRegisters(int printHeader, uint32_t nextPC, char *message)
{
	int i;

	if (tui != 0) {
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

	printf(" %5" PRIX32 " %5" PRIX32 " ", cpu.pc, cpu.nextPC);
	printf("%c%c%c%c  ",
	       R_FL & FL_N ? 'n' : ' ', R_FL & FL_Z ? 'z' : ' ',
	       R_FL & FL_V ? 'v' : ' ', R_FL & FL_C ? 'c' : ' ');
	for(i = 0; i < NUM_REGISTERS; i++)
		printf("%5" PRIX32 " ", cpu.r[i]);
	printf("\n");
}

void interactive()
{
	struct instruction o;

	if (beInteractive == 0) {
		return;
	}

	fetchInst(cpu.pc, &o);

	if (tui != 0) {
		updateTUI(&cpu, &o);
		return;
	}
}

static struct option longopts[] = {
	{"rom", required_argument, NULL, 'r'},
	{"binary", required_argument, NULL, 'b'},
	{"debug", required_argument, NULL, 'g'},
	{"max-cycles", required_argument, NULL, 'c'},
	{"interactive", no_argument, NULL, 'i'},
	{"tui", no_argument, NULL, 't'},
	{"starting-pc", required_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'}
};

char *optdesc[] = {
	"The ROM to load into memory.",
	"A binary to place in memory as <binaryPath>:<memoryOffset>.",
	"Emulator will exit after N cycles.",
	"Interactive debugging mode.",
	"Text user interface (TUI).",
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
				addBinary(optarg, 0);
				break;
			case 'g':
				addBinary(optarg, 1);
				break;
			case 'i':
				beInteractive = 1;
				break;
			case 't':
				beInteractive = 1;
				tui = 1;
				break;
			case 'c':
				cpu.maxCycles = strtoull(optarg, NULL, 0);
				break;
			case 'p':
				cpu.startingPC = strtoull(optarg, NULL, 0);
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
	return cpu.mem[address];
}

uint32_t read32bit(uint32_t address)
{
	if (address < cpu.mmapIOstart || address >= cpu.mmapIOend) {
		/*
		 * Normal memory access.
		 */
		return *(uint32_t *)(cpu.mem + address);
	}

	/*
	 * Anything else must be memory mapped I/O.
	 */
	address -= cpu.mmapIOstart;

	uint32_t *reg = mmapIOregister(address);

	if (reg == NULL) {
		fprintf(stderr, "Can't get memory mapped register to read.\n");
		exit(1);
	}

	return(*reg);
}

void write8bit(uint32_t address, uint8_t data)
{
	cpu.mem[address] = data;
}

void write32bit(uint32_t address, uint32_t data)
{
	if (address < cpu.mmapIOstart || address >= cpu.mmapIOend) {
		/*
		 * Normal memory access.
		 */
		*(uint32_t *)(cpu.mem + address) = data;

		return;
	}

	/*
	 * Anything else must be memory mapped I/O.
	 */
	address -= cpu.mmapIOstart;

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
			if (tui != 0) {
				loadDebugInfo(thisBinary->filePath, thisBinary->memoryOffset);
			}
		}
	}

	if (tui != 0) {
		initTUI();
	}

	//ic = 0;
	cpu.pc = cpu.startingPC;
	stop = 0;

	dumpRegisters(1, 0, "");

	while (!stop && (cpu.maxCycles-- > 0)) {

		interactive();
		fetchInst(cpu.pc, &o);

		cpu.nextPC = cpu.pc + 8;

		R_C1++;
		R_C2++;

		switch (o.op) {

		case nop:
			log("nop");
			break;

		/*
		 * Arithmetic operations.
		 */
		case add:
			log("add r[%" PRIu32 "] = %" PRIX32 " + %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 + o.opr2;
			if ((UINT32_MAX - o.opr1) < o.opr2) {
				R_FL |= FL_C;
			}
			break;
		case sub:
			log("sub r[%" PRIu32 "] = %" PRIX32 " - %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 - o.opr2;
			break;
		case adc:
			log("adc r[%" PRIu32 "] = %" PRIX32 " + %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 + o.opr2 + (R_FL & FL_C);
			if ((UINT32_MAX - o.opr1) < o.opr2) {
				R_FL |= FL_C;
			}
			break;
		case sbc:
			log("sbc r[%" PRIu32 "] = %" PRIX32 " - %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 - o.opr2;
			break;
		case mul:
			log("mul r[%" PRIu32 "] = %" PRIX32 " * %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 * o.opr2;
			break;
		case div:
			log("div r[%" PRIu32 "] = %" PRIX32 " / %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 / o.opr2;
			break;

		/*
		 * Load and stores.
		 */
		case ldb:
			address = getAddress(o.mode, o.opr2);
			log("ldb r[%" PRIu32 "] = mem[%" PRIX32 "]", o.reg0, address);
			cpu.r[o.reg0] = read8bit(address);
			break;
		case ldw:
			address = getAddress(o.mode, o.opr2);
			log("ldw r[%" PRIu32 "] = mem[%" PRIX32 "]", o.reg0, address);
			cpu.r[o.reg0] = littleToHost32(read32bit(address));
			break;
		case stb:
			address = getAddress(o.mode, o.opr0);
			log("stb mem[%" PRIX32 "] = %" PRIX32, address, o.opr2);
			write8bit(address, (uint8_t)o.opr2);
			break;
		case stw:
			address = getAddress(o.mode, o.opr0);
			log("stw mem[%" PRIX32 "] = %" PRIX32, address, o.opr2);
			write32bit(address, hostToLittle32(o.opr2));
			break;
		case mov:
			log("mov r[%" PRIu32 "] = %" PRIX32, o.reg0, o.opr2);
			cpu.r[o.reg0] = o.opr2;
			break;

		/*
		 * Bitwise operations.
		 */
		case and:
			log("and r[%" PRIu32 "] = %" PRIX32 " & %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 & o.opr2;
			break;
		case or:
			log("or r[%" PRIu32 "] = %" PRIX32 " | %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 | o.opr2;
			break;
		case xor:
			log("xor r[%" PRIu32 "] = %" PRIX32 " ^ %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 ^ o.opr2;
			break;
		case nor:
			log("nor r[%" PRIu32 "] = ~%" PRIX32 " & ~%" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = ~o.opr1 & ~o.opr2;
			break;
		case lsl:
			log("lsl r[%" PRIu32 "] = %" PRIX32 " << %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 << o.opr2;
			break;
		case lsr:
			log("lsr r[%" PRIu32 "] = %" PRIX32 " >> %" PRIX32, o.reg0, o.opr1, o.opr2);
			cpu.r[o.reg0] = o.opr1 >> o.opr2;
			break;

		/*
		 * Branches and jumps.
		 */
		case cmp:
			log("cmp r[%" PRIu32 "] %" PRIX32, o.reg0, o.opr2);
			uint32_t temp = cpu.r[o.reg0] - o.opr2;
			// flags.n = temp & (0x1 << 31); // enable when signed arithmetic is supported
			flags.z = (temp == 0) ? 1 : 0;
			flags.c = cpu.r[o.reg0] < o.opr2; // borrow?
			break;
		case jmp:
			address = getAddress(o.mode, o.opr2);
			log("jmp %" PRIX32, address);
			cpu.nextPC = address;
			break;
		case jz:
			address = getAddress(o.mode, o.opr2);
			log("jz %" PRIX32, address);
			if (flags.z != 0) {
				cpu.nextPC = address;
			}
			break;
		case jnz:
			address = getAddress(o.mode, o.opr2);
			log("jnz %" PRIX32, address);
			if (flags.z == 0) {
				cpu.nextPC = address;
			}
			break;
		case jl:
			address = getAddress(o.mode, o.opr2);
			log("jl %" PRIX32, address);
			if (flags.c != 0) {
				cpu.nextPC = address;
			}
			break;
		case jg:
			address = getAddress(o.mode, o.opr2);
			log("jg %" PRIX32, address);
			if (flags.c == 0) {
				cpu.nextPC = address;
			}
			break;

		/*
		 * Others.
		 */
		case die:
			log("die");
			stop = 1;
			break;
		default:
			fprintf(stderr, "Unknown opcode: %" PRIX32 "\n", o.op);
			stop = 1;
			break;
		}

		dumpRegisters(0, cpu.nextPC, cpu.msg);

		cpu.pc = cpu.nextPC;
		if (cpu.pc > cpu.memSize) {
			fprintf(stderr, "ERROR: Can't access memory at 0x%" PRIX32 "\n", cpu.pc);
			stop = 1;
			break;
		}
	}
	interactive();

	if (tui != 0) {
		freeTUI();
	}

	return(0);
}
