#!/bin/bash

width=4

while [ "$#" -gt 0 ]; do
	case "$1" in
		-w)
			width="$2"
			shift 2;;
		-h)
			echo "Usage: $0 [-h] [-w <width>] <binary_file>"
			exit 1;;
		*)
			break;;
	esac
done

echo "memory_initialization_radix=2;"
echo "memory_initialization_vector="

lastLine=$(xxd -c$width -b progs/boot.bin | wc -l)

for ((i=0; i<$width; i++)); do
	if ((i > 0)); then
		output=$output'" "'
	fi
	output=$output"$"$((i+2))
done

xxd -c$width -b $1 | awk "{ printf $output} {if (FNR != "$lastLine') print ","; else print ";"}' | tr -d ' '
