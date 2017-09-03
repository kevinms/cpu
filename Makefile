
all:
	gcc -Wall -g emulator.c debugger.c -lncurses -o emulator
	gcc -Wall -g fs.c -o fs
	gcc -Wall -g heap.c -o heap
	gcc -Wall -g compiler.c list.c -o compiler

#
# Instrument for profiling with gprof.
#
debug:
	gcc -Wall -g -pg emulator.c debugger.c -lncurses -o emulator

os: boot lib kernel
	echo "Building full OS stack."
	rm -f sd.img
	./fs -i sd.img -n4096
	./fs -i sd.img -a progs/all.kernel.bin
	./fs -i sd.img -a progs/lib.bin
	./fs -i sd.img -l

boot:
	./assembler.py -a progs/boot.asm --binary --debug > progs/boot.rom
	./bin2coe.sh -w 1 progs/boot.bin > progs/boot.coe

lib:
	./assembler.py -a progs/lib.asm --binary --debug --symbols -s 0x3000 > progs/lib.rom

kernel:
	cat progs/lib.sym progs/kernel.asm > progs/all.kernel.asm
	./assembler.py -a progs/all.kernel.asm --binary --debug > progs/all.kernel.rom

sane:
	stty sane
	reset

clean:
	rm -f emulator fs heap
