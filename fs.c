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
	{"image", required_argument, NULL, 'i'},
	{"new", required_argument, NULL, 'n'},
	{"ls", no_argument, NULL, 'l'},
	{"delete", required_argument, NULL, 'd'},
	{"add", required_argument, NULL, 'a'},
	{"read", required_argument, NULL, 'r'},
	{"scan", no_argument, NULL, 's'},
	{"help", no_argument, NULL, 'h'}
};

char *optdesc[] = {
	"The disk image file to use.",
	"Create a new disk image of <imageSize> bytes.",
	"List contents of disk image.",
	"Delete file from disk image.",
	"Add file to disk image.",
	"Read file from disk image and write to STDOUT.",
	"Scan disk image.",
	"This help."
};

char *optstring = NULL;

static char *imageFile;
static uint32_t imageSize;

static char *argFilePath;
static int cmdNew;
static int cmdList;
static int cmdDelete;
static int cmdAdd;
static int cmdRead;
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
			case 'i':
				imageFile = strdup(optarg);
				break;
			case 'n':
				imageSize = strtoull(optarg, NULL, 0);
				cmdNew = 1;
				break;
			case 'l':
				cmdList = 1;
				break;
			case 'd':
				argFilePath = strdup(optarg);
				cmdDelete = 1;
				break;
			case 'a':
				argFilePath = strdup(optarg);
				cmdAdd = 1;
				break;
			case 'r':
				argFilePath = strdup(optarg);
				cmdRead = 1;
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

	if (imageFile == NULL) {
		fprintf(stderr, "Expected a disk image file path.\n");
		exit(1);
	}
}

struct superBlock {
	uint32_t magic;
	uint32_t size;
	uint32_t freeOffset;
	uint32_t usedOffset;
};

struct header {
	uint32_t length;
	uint32_t prevOffset;
	uint32_t nextOffset;
	uint32_t fileSize;
	uint8_t fileName[64];
};

struct trailer {
	uint32_t headerOffset;
};

#define OBJECT_SUPER    ((uint32_t)sizeof(struct superBlock))
#define OBJECT_HEADER   ((uint32_t)sizeof(struct header))
#define OBJECT_TRAILER  ((uint32_t)sizeof(struct trailer))
#define OBJECT_OVERHEAD ((uint32_t)(sizeof(struct header) + sizeof(struct trailer)))

static void *mapping;

void printFileSystemInfo()
{
	fprintf(stderr, "superBlock: 0x%" PRIX32 "\n", OBJECT_SUPER);
	fprintf(stderr, "header:     0x%" PRIX32 "\n", OBJECT_HEADER);
	fprintf(stderr, "trailer:    0x%" PRIX32 "\n", OBJECT_TRAILER);
}

void printSuperBlock(struct superBlock *super)
{
	fprintf(stderr, "Super Block at 0x%" PRIX32 ":\n", (uint32_t)(((void *)super) - mapping));
	fprintf(stderr, "\tmagic:      0x%" PRIX32 "\n", super->magic);
	fprintf(stderr, "\tsize:       0x%" PRIX32 "\n", super->size);
	fprintf(stderr, "\tfreeOffset: 0x%" PRIX32 "\n", super->freeOffset);
	fprintf(stderr, "\tusedOffset: 0x%" PRIX32 "\n", super->usedOffset);
}

void printObject(struct header *header, struct trailer *trailer)
{
	fprintf(stderr, "Header at 0x%" PRIX32 ":\n", (uint32_t)(((void *)header) - mapping));
	fprintf(stderr, "\tlength:      0x%" PRIX32 "\n", header->length);
	fprintf(stderr, "\tprevOffset:  0x%" PRIX32 "\n", header->prevOffset);
	fprintf(stderr, "\tnextOffset:  0x%" PRIX32 "\n", header->nextOffset);
	fprintf(stderr, "\tfileSize:    0x%" PRIX32 "\n", header->fileSize);
	fprintf(stderr, "\tfileName:    %s\n", header->fileName);

	fprintf(stderr, "Trailer at 0x%" PRIX32 "\n", (uint32_t)(((void *)trailer) - mapping));
	fprintf(stderr, "\theaderOffset: 0x%" PRIX32 "\n", trailer->headerOffset);
}

int scanImage()
{
	struct superBlock *super = mapping;
	struct header *thisHeader;
	struct trailer *thisTrailer;
	uint32_t thisOffset;

	printSuperBlock(super);

	for (thisOffset = OBJECT_SUPER;
		 thisOffset < imageSize;
		 thisOffset = thisOffset + thisHeader->length + OBJECT_OVERHEAD) {
		thisHeader = mapping + thisOffset;
		thisTrailer = mapping + thisOffset + thisHeader->length + OBJECT_HEADER;

		printObject(thisHeader, thisTrailer);
	}

	return(0);
}

void unlinkObject(uint32_t *headOffset, struct header *this)
{
	struct header *prev;
	struct header *next;
	uint32_t thisOffset = (uintptr_t)this - (uintptr_t)mapping;

	if (this->prevOffset != 0) {
		prev = mapping + this->prevOffset;
		prev->nextOffset = this->nextOffset;
	}
	if (this->nextOffset != 0) {
		next = mapping + this->nextOffset;
		next->prevOffset = this->prevOffset;
	}

	if (*headOffset == thisOffset) {
		*headOffset = this->nextOffset;
	}
}

void linkObject(uint32_t *headOffset, struct header *this)
{
	struct header *next;
	uint32_t thisOffset = (uintptr_t)this - (uintptr_t)mapping;

	if (*headOffset != 0) {
		next = mapping + *headOffset;
		next->prevOffset = thisOffset;
	}

	this->prevOffset = 0;
	this->nextOffset = *headOffset;
	*headOffset = thisOffset;
}

int openDiskImage()
{
	struct stat statBuffer;
	int fd;
	int flags;
	int mode;

	if (imageFile == NULL) {
		fprintf(stderr, "Must provide an image file.\n");
		return(-1);
	}

	if (cmdNew && access(imageFile, F_OK) >= 0) {
		fprintf(stderr, "Image file already exists.\n");
		return(-1);
	}

	flags = O_RDWR;
	mode = 0;
	if (cmdNew) {
		flags |= O_CREAT;
		mode = S_IRWXU | S_IRWXG;
	}

	if ((fd = open(imageFile, flags, mode)) < -1) {
		fprintf(stderr, "Can't open %s: %s\n", imageFile, strerror(errno));
		return(-1);
	}

	if (cmdNew) {
		/*
		 * Size the new image.
		 */
		if (ftruncate(fd, 0) < 0) {
			fprintf(stderr, "Can't truncate to zero: %s\n", strerror(errno));
			close(fd);
			return(-1);
		}
		if (ftruncate(fd, imageSize) < 0) {
			fprintf(stderr, "Can't truncate to %" PRIu32 ": %s\n",
					imageSize, strerror(errno));
			close(fd);
			return(-1);
		}
	}

	if (fstat(fd, &statBuffer) < 0) {
		fprintf(stderr, "Can't stat %s: %s\n", imageFile, strerror(errno));
		close(fd);
		return(-1);
	}
	imageSize = statBuffer.st_size;

	if ((mapping = mmap(NULL, imageSize, PROT_READ | PROT_WRITE,
						MAP_SHARED, fd, 0)) == MAP_FAILED) {
		fprintf(stderr, "Can't memory map image file: %s\n",
				strerror(errno));
		close(fd);
		return(-1);
	}
	close(fd);

	if (cmdNew) {
		struct superBlock *super;
		struct header *header;
		struct trailer *trailer;

		/*
		 * Initialize the new image.
		 */
		super = mapping;
		header = mapping + OBJECT_SUPER;
		trailer = mapping + (imageSize - OBJECT_TRAILER);

		super->magic = 0x42;
		super->size = imageSize;
		super->freeOffset = 0;
		super->usedOffset = 0;

		memset(header, 0, OBJECT_HEADER);
		header->length = imageSize - OBJECT_SUPER - OBJECT_OVERHEAD;
		header->fileSize = UINT32_MAX;
		trailer->headerOffset = OBJECT_SUPER;

		linkObject(&super->freeOffset, header);
	}

	return(0);
}

int addFile(char *filePath)
{
	struct superBlock *super = mapping;
	struct header *thisFree;
	struct header *thisUsed;
	struct stat statBuffer;
	uint32_t thisOffset;
	uint32_t fileSize;
	int foundSpace;
	int fd;

	/*
	 * Open file to be added.
	 */
	if ((fd = open(filePath, O_RDWR)) < -1) {
		fprintf(stderr, "Can't open %s: %s\n", filePath, strerror(errno));
		return(-1);
	}
	if (fstat(fd, &statBuffer) < 0) {
		fprintf(stderr, "Can't stat %s: %s\n", filePath, strerror(errno));
		close(fd);
		return(-1);
	}
	fileSize = statBuffer.st_size;

	/*
	 * Find a free object that can contain the new file.
	 */
	foundSpace = 0;
	for (thisOffset = super->freeOffset;
		 thisOffset != 0;
		 thisOffset = thisFree->nextOffset) {
		thisFree = mapping + thisOffset;

		if (thisFree->length >= fileSize) {
			foundSpace = 1;
			break;
		}
	}

	if (foundSpace == 0) {
		fprintf(stderr, "Can't find space for %s of size %" PRIu32 "\n",
				filePath, fileSize);
		close(fd);
		return(-1);
	}

	/*
	 * Copy file data to new used object.
	 * We do this before modifying the filesystem structures in case
	 * we have a failure.
	 */
	void *buffer = ((void *)thisFree) + OBJECT_HEADER;
	uint32_t bytesLeft = fileSize;

	fprintf(stderr, "Writing file at 0x%" PRIX32"\n", (uint32_t)(buffer - mapping));

	while (bytesLeft > 0) {
		ssize_t bytesRead;

		if ((bytesRead = read(fd, buffer, bytesLeft)) < 0) {
			fprintf(stderr, "Can't read %s: %s\n", filePath, strerror(errno));
			close(fd);
			return(-1);
		}

		bytesLeft -= (uint32_t)bytesRead;
		buffer += bytesRead;
	}

	/*
	 * Remove free object from free list.
	 */
	unlinkObject(&super->freeOffset, thisFree);

	thisUsed = thisFree;
	thisUsed->fileSize = fileSize;
	strncpy((char *)thisUsed->fileName, filePath, sizeof(thisUsed->fileName) - 1);

	if (thisUsed->length >= fileSize + OBJECT_OVERHEAD) {
		uint32_t newFreeOffset;
		struct trailer *freeTrailer;
		struct trailer *usedTrailer;

		freeTrailer = mapping + thisOffset + OBJECT_HEADER + thisFree->length;
		usedTrailer = mapping + thisOffset + OBJECT_HEADER + fileSize;

		/*
		 * Split the free object in two.
		 */
		newFreeOffset = thisOffset + fileSize + OBJECT_OVERHEAD;
		thisFree = mapping + newFreeOffset;
		thisFree->length = thisUsed->length - fileSize - OBJECT_OVERHEAD;
		thisFree->fileSize = UINT32_MAX;
		freeTrailer->headerOffset = newFreeOffset;
		linkObject(&super->freeOffset, thisFree);

		thisUsed->length = fileSize;
		usedTrailer->headerOffset = thisOffset;
	}

	/*
	 * Add used object to used list.
	 */
	linkObject(&super->usedOffset, thisUsed);

	return(0);
}

int coalesceObjects(struct header *left, struct header *right)
{
	struct superBlock *super = mapping;
	uint32_t leftOffset;

	if ((left->fileSize != UINT32_MAX) || (right->fileSize != UINT32_MAX)) {
		/*
		 * Can't coalesce.
		 */
		return(0);
	}

	struct trailer *rightTrailer = (void *)right + right->length + OBJECT_HEADER;

	unlinkObject(&super->freeOffset, left);
	unlinkObject(&super->freeOffset, right);

	left->length += right->length + OBJECT_OVERHEAD;
	leftOffset = (uintptr_t)left - (uintptr_t)mapping;
	rightTrailer->headerOffset = leftOffset;
	memset(mapping + leftOffset + OBJECT_HEADER, 0, left->length);

	linkObject(&super->freeOffset, left);

	return(1);
}

int deleteFile(char *filePath)
{
	struct superBlock *super = mapping;
	struct header *this;
	uint32_t thisOffset;

	for (thisOffset = super->usedOffset;
		 thisOffset != 0;
		 thisOffset = this->nextOffset) {
		this = mapping + thisOffset;

		if (strncmp((char *)this->fileName, filePath, strlen(filePath)) == 0) {
			struct header *neighbor;
			int coalesced = 0;

			/*
			 * Found the file to delete.
			 */
			unlinkObject(&super->usedOffset, this);
			this->fileSize = UINT32_MAX;
			memset(mapping + thisOffset + OBJECT_HEADER, 0, this->length);

			/*
			 * Try to coalesce with left neighbor.
			 */
			if (thisOffset > OBJECT_SUPER) {
				struct trailer *trailer = (void *)this - OBJECT_TRAILER;
				neighbor = mapping + trailer->headerOffset;
				coalesced += coalesceObjects(neighbor, this);
				fprintf(stderr, "Tried to coalesce left: %d\n", coalesced);
			}
			if (coalesced > 0) {
				this = neighbor;
				thisOffset = (uintptr_t)neighbor - (uintptr_t)mapping;
			}

			/*
			 * Try to coalesce with right neighbor.
			 */
			if (thisOffset + this->length + OBJECT_OVERHEAD < UINT32_MAX) {
				neighbor = (void *)this + this->length + OBJECT_OVERHEAD;
				coalesced += coalesceObjects(this, neighbor);
				fprintf(stderr, "Tried to coalesce right: %d\n", coalesced);
			}

			/*
			 * Insert into the free list, if we couldn't coalesce.
			 */
			if (coalesced == 0) {
				fprintf(stderr, "Couldn't coalesce with neighbors.\n");
				linkObject(&super->freeOffset, this);
			}
			break;
		}
	}
	return(0);
}

int readFile(char *filePath)
{
	struct superBlock *super = mapping;
	struct header *thisUsed;
	uint32_t thisOffset;

	/*
	 * Traverse all used objects.
	 */
	super = mapping;
	for (thisOffset = super->usedOffset;
		 thisOffset != 0;
		 thisOffset = thisUsed->nextOffset) {
		thisUsed = mapping + thisOffset;

		if (strncmp((char *)thisUsed->fileName, filePath, strlen(filePath)) == 0) {
			/*
			 * Found a match -- dump contents to STDOUT.
			 */
			void *buffer = ((void *)thisUsed) + OBJECT_HEADER;
			uint32_t bytesLeft = thisUsed->fileSize;

			while (bytesLeft > 0) {
				ssize_t bytesWritten;

				if ((bytesWritten = write(STDOUT_FILENO, buffer, bytesLeft)) < 0) {
					fprintf(stderr, "Can't read %s: %s\n", filePath, strerror(errno));
					return(-1);
				}

				bytesLeft -= (uint32_t)bytesWritten;
				buffer += bytesWritten;
			}
		}
	}

	return(0);
}

int listFiles()
{
	struct superBlock *super = mapping;
	struct header *thisUsed;
	uint32_t thisOffset;
	uint32_t count;


	/*
	 * Traverse all used objects.
	 */
	super = mapping;
	for (thisOffset = super->usedOffset, count = 0;
		 thisOffset != 0;
		 thisOffset = thisUsed->nextOffset, ++count) {
		thisUsed = mapping + thisOffset;

		if (count == 0) {
			fprintf(stderr, "Bytes     File\n");
		}
		fprintf(stderr, "%-10" PRIu32 " %s\n",
				thisUsed->fileSize, thisUsed->fileName);
	}

	fprintf(stderr, "%" PRIu32 " files\n", count);

	return(0);
}

int main(int argc, char **argv) {
	parseArgs(argc, argv);

	//printFileSystemInfo();
	if (openDiskImage() < 0) {
		fprintf(stderr, "Can't open disk image.\n");
		return(1);
	}

	if (cmdList) {
		listFiles();
	}

	if (cmdDelete) {
		deleteFile(argFilePath);
	}

	if (cmdAdd) {
		addFile(argFilePath);
	}

	if (cmdRead) {
		readFile(argFilePath);
	}

	if (cmdScan) {
		scanImage();
	}

	return(0);
}
