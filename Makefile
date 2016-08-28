
all:
	gcc -Wall -g emulator.c -o emulator
	gcc -Wall -g fs.c -o fs
	gcc -Wall -g heap.c -o heap

os: bios lib kernel
	echo "Building full OS stack."
	rm -f sd.img
	./fs -i sd.img -n4096
	./fs -i sd.img -a progs/kernel.bin
	./fs -i sd.img -a progs/lib.bin
	./fs -i sd.img -l

bios:
	./assembler.py -a progs/bios.asm -b progs/bios.bin > progs/bios.rom

lib:
	./assembler.py -a progs/lib.asm -b progs/lib.bin -e progs/lib.sym -s 0x3000 > progs/lib.rom

kernel:
	cat progs/lib.sym progs/kernel.asm > progs/all.kernel.asm
	./assembler.py -a progs/all.kernel.asm -b progs/kernel.bin > progs/kernel.rom

clean:
	rm -f emulator fs heap
