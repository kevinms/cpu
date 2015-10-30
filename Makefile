
all:
	gcc -Wall -g cpu-emu.c -o cpu-emu
	gcc -Wall -g fs.c -o fs

clean:
	rm -f cpu-emu fs
