
all:
	gcc -Wall -g emulator.c -o emulator
	gcc -Wall -g fs.c -o fs

clean:
	rm -f emulator fs
