#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

int main()
{
	int len = 4096;
	int fd = open("test.bin", O_CREAT | O_RDWR, 0644);
	ftruncate(fd, len);
	void *mem = mmap (0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	int i;
	for (i = 0; i < len; i++) {
		*(((uint8_t *)mem) + i) = i+1;
	}

	return 0;
}
