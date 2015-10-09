#!/bin/bash

output=$(basename "$1" .asm)
echo $output

cat $1 > $output.all.asm
shift

for i in "$@"; do
	echo "Add $i"
	cat $i >> $output.all.asm
done

./assembler.py --assemble $output.all.asm > $output.rom
