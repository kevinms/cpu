#!/bin/bash

output=$(dirname "$1")/$(basename "$1" .asm)

./assembler.py -a $output.asm --binary --debug > $output.rom
./bin2coe.sh -w 1 $output.bin > $output.coe
