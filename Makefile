
all:
	gcc -Wall -g emulator.c -o emulator
	gcc -Wall -g fs.c -o fs

kernel:
	./assembler.py -a progs/bios.asm -b progs/bios.bin > progs/bios.rom
	./assembler.py -a progs/lib.asm -b progs/lib.bin -e progs/lib.sym -s 0x3000 > progs/lib.rom
	cat progs/lib.sym progs/kernel.asm > progs/all.kernel.asm
	./assembler.py -a progs/all.kernel.asm -b progs/kernel.bin > progs/kernel.rom
	rm -f sd.img
	./fs -i sd.img -n4096
	./fs -i sd.img -a progs/kernel.bin
	./fs -i sd.img -a progs/lib.bin
	./fs -i sd.img -l

clean:
	rm -f emulator fs
