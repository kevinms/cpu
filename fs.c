#include <stdio.h>

static struct option longopts[] = {
	{"image", required_argument, NULL, 'i'},
	{"ls", no_argument, NULL, 'l'},
	{"rm", required_argument, NULL, 'r'},
	{"cp", required_argument, NULL, 'c'},
	{"help", no_argument, NULL, 'h'}
};

char *optdesc[] = {
	"The disk image file to use.",
	"List contents of disk image.",
	"Remove file from disk image.",
	"Copy file to disk image.",
	"This help."
};

char *optstring = NULL;

static char *fileToRemote;
static char *fileToCopy;
static int cmdList;
static int cmdRemove;
static int cmdCopy;

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
			case 'i':
				diskImageFile = optarg;
				cmdCreate = 1;
				break;
			case 'l':
				cmdList = 1;
				break;
			case 'r':
				fileToRemove = strcpy(optarg);
				cmdRemove = 1;
				break;
			case 'c':
				fileToCopy = strcpy(optarg);
				cmdCopy = 1;
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

	if (diskImageFile == NULL) {
		fprintf(stderr, "Expected a disk image file path.\n");
		exit(1);
	}
}

struct superBlock {
	uint32_t magic;
	uint32_t version;
	uint32_t freeOffset;
	uint32_t usedOffset;
};

struct header {
	uint32_t length;
	uint32_t nextOffset;
	uint32_t prevOffset;
	uint8_t name[64];
};

struct trailer {
	uint32_t headerOffset;
};

int main()
{
	parseArgs(argc, argv);

	openDiskImage();

	if (cmdList) {
		
	}

	if (cmdRemove) {
		
	}

	if (cmdCopy) {
		
	}

	return(0);
}
