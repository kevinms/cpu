#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <getopt.h>
#include <errno.h>

static struct option longopts[] = {
	{"memory", required_argument, NULL, 'm'},
	{"heap-offset", required_argument, NULL, 'o'},
	{"heap-size", required_argument, NULL, 'z'},
	{"scan", no_argument, NULL, 's'},
	{"help", no_argument, NULL, 'h'}
};

char *optdesc[] = {
	"The memory file to use.",
	"Starting offset of heap within memory file.",
	"Total heap size.",
	"Scan heap contents.",
	"This help."
};

char *optstring = NULL;

static char *memoryFile;
static uint32_t memorySize;

static uint32_t heapOffset;
static uint32_t heapSize;

static int cmdScan;

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
			case 'm':
				memoryFile = strdup(optarg);
				break;
			case 'o':
				heapOffset = strtoull(optarg, NULL, 0);
				break;
			case 'z':
				heapSize = strtoull(optarg, NULL, 0);
				break;
			case 's':
				cmdScan = 1;
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

	if (memoryFile == NULL) {
		fprintf(stderr, "Expected a memory file path.\n");
		exit(1);
	}
}

struct heapSuperBlock {
	uint32_t size;
	uint32_t freeOffset;
	uint32_t usedOffset;
};

struct heapHeader {
	uint32_t length;
	uint32_t prevOffset;
	uint32_t nextOffset;
};

struct heapTrailer {
	uint32_t headerOffset;
};

#define OBJECT_SUPER    ((uint32_t)sizeof(struct heapSuperBlock))
#define OBJECT_HEADER   ((uint32_t)sizeof(struct heapHeader))
#define OBJECT_TRAILER  ((uint32_t)sizeof(struct heapTrailer))
#define OBJECT_OVERHEAD ((uint32_t)(sizeof(struct heapHeader) + sizeof(struct heapTrailer)))
#define OBJECT_USED     ((uint32_t)0x80000000)

static void *mapping;

int openMemoryFile()
{
	struct stat statBuffer;
	int fd;
	int flags;
	int mode;

	if (memoryFile == NULL) {
		fprintf(stderr, "Must provide an image file.\n");
		return(-1);
	}

	flags = O_RDWR;
	mode = 0;

	if ((fd = open(memoryFile, flags, mode)) < -1) {
		fprintf(stderr, "Can't open %s: %s\n", memoryFile, strerror(errno));
		return(-1);
	}

	if (fstat(fd, &statBuffer) < 0) {
		fprintf(stderr, "Can't stat %s: %s\n", memoryFile, strerror(errno));
		close(fd);
		return(-1);
	}
	memorySize = statBuffer.st_size;

	if ((mapping = mmap(NULL, memorySize, PROT_READ | PROT_WRITE,
						MAP_SHARED, fd, 0)) == MAP_FAILED) {
		fprintf(stderr, "Can't memory map image file: %s\n",
				strerror(errno));
		close(fd);
		return(-1);
	}
	close(fd);

	return(0);
}

#define C_RED     "\x1B[1;31m"
#define C_GREEN   "\x1B[1;32m"
#define C_MAGENTA "\x1B[1;35m"
#define C_NO      "\x1B[0m"

void printHeapInfo()
{
	fprintf(stderr, "heapSuperBlock: 0x%" PRIX32 "\n", OBJECT_SUPER);
	fprintf(stderr, "heapHeader:     0x%" PRIX32 "\n", OBJECT_HEADER);
	fprintf(stderr, "heapTrailer:    0x%" PRIX32 "\n", OBJECT_TRAILER);
}

void printHeapSuperBlock(struct heapSuperBlock *super)
{
	fprintf(stderr, "Super Block at 0x%" PRIX32 ":\n", (uint32_t)(((void *)super) - mapping));
	fprintf(stderr, "    size:       0x%" PRIX32 "\n", super->size);
	fprintf(stderr, "    freeOffset: 0x%" PRIX32 "\n", super->freeOffset);
	fprintf(stderr, "    usedOffset: 0x%" PRIX32 "\n", super->usedOffset);
}

void printHeapObject(struct heapHeader *header, struct heapTrailer *trailer)
{
	uint32_t headerOffset = ((void *)header) - mapping;
	fprintf(stderr, "Header at 0x%" PRIX32 " (%s):\n", headerOffset,
			header->length & OBJECT_USED ? C_GREEN "used" C_NO : C_MAGENTA "free" C_NO);
	fprintf(stderr, "    length:      0x%" PRIX32 " -> 0x%" PRIX32 "\n", header->length, header->length & ~OBJECT_USED);
	fprintf(stderr, "    prevOffset:  0x%" PRIX32 "\n", header->prevOffset);
	fprintf(stderr, "    nextOffset:  0x%" PRIX32 "\n", header->nextOffset);

	fprintf(stderr, "  Trailer at 0x%" PRIX32 "\n", (uint32_t)(((void *)trailer) - mapping));
	fprintf(stderr, "    headerOffset: 0x%" PRIX32 "%s\n", trailer->headerOffset,
			headerOffset != trailer->headerOffset ? C_RED " Incorrect!" C_NO : "");
}

int scanHeap()
{
	struct heapSuperBlock *super = mapping + heapOffset;
	struct heapHeader *thisHeader;
	struct heapTrailer *thisTrailer;
	uint32_t thisOffset;

	printHeapSuperBlock(super);

	for (thisOffset = OBJECT_SUPER;
		 thisOffset < heapSize;
		 thisOffset = thisOffset + (thisHeader->length & ~OBJECT_USED) + OBJECT_OVERHEAD) {
		thisHeader = mapping + heapOffset + thisOffset;
		thisTrailer = mapping + heapOffset + thisOffset + (thisHeader->length & ~OBJECT_USED) + OBJECT_HEADER;

		printHeapObject(thisHeader, thisTrailer);
	}

	return(0);
}

int main(int argc, char **argv) {
	parseArgs(argc, argv);

	//printHeapInfo();
	if (openMemoryFile() < 0) {
		fprintf(stderr, "Can't open disk image.\n");
		return(1);
	}

	if (cmdScan) {
		scanHeap();
	}

	return(0);
}
