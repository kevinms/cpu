#!/usr/bin/python

import getopt, sys, re, os
import struct

iset = {}   # instruction set
labels = {} # label table
alias = {'sp':'r11', 'ba':'r12', 'fl':'r13', 'c1':'r14', 'c2':'r15'}

magicNumber = 0xDEADC0DE
baseAddress = stackAddress = stackSize = heapAddress = heapSize = 0x0
header = False

doAssemble = False
binaryFile = asciiFile = symbolFile = debugFile = None

def error(msg):
	print >> sys.stderr, msg
	sys.exit(1)
def debug(msg):
	print >> sys.stderr, msg

def output(msg, trailer = ' '):
	# ASCII Binary STDOUT
	sys.stdout.write(msg + trailer)

	# Binary File
	if binaryFile is not None:
		n = int(msg, 2)
		if len(msg) == 8:
			binaryFile.write( struct.pack('<B', n) )
		if len(msg) == 16:
			binaryFile.write( struct.pack('<H', n) )
		if len(msg) == 32:
			binaryFile.write( struct.pack('<I', n) )

	# ASCII Binary File
	if asciiFile is not None:
		asciiFile.write(msg + trailer)

def exportSymbol(label):
	if symbolFile is not None:
		symbolFile.write(".%s 0x%X\n" % (label, labels[label]))

def exportDebug(lineNum, progOffset):
	if debugFile is not None:
		debugFile.write("0x%X 0x%X\n" % (lineNum, progOffset - baseAddress))

def parseISA():
	parse = 0
	header = open("isa.h", "r")
	for line in header:
		if parse == 0:
			if "Instruction Op Codes" in line:
				parse = 1;
			continue

		if "End Instruction Op Codes" in line:
			parse = 0;
			break;

		code, octothorpe, comment = line.partition('//')
		code = code.strip()
		comment = comment.strip()
		if code == "":
			continue

		i = code.split()
		o = comment.split()
		#debug(i, o)

		op = { 'opcode': "", 'operands': 0, 'var': 0, 'opr0': "", 'opr1': "", 'opr2': "" }
		op['opcode'] = i[2][2:]
		if len(o) > 0:
			op['var'] = int(o[0], 10)

		if len(o) > 1:
			op['operands'] = len(o) - 1
			op['opr0'] = ''.join(re.search('\[(.*?)\]', o[1]).group(1).split(','))
		if len(o) > 2:
			op['opr1'] = ''.join(re.search('\[(.*?)\]', o[2]).group(1).split(','))
		if len(o) > 3:
			op['opr2'] = ''.join(re.search('\[(.*?)\]', o[3]).group(1).split(','))

		#debug(op)

		iset[i[1]] = op

	#for i in iset:
	#	debug(i, iset[i])

def loadLabels(source):
	lineNum = 0
	progOffset = baseAddress
	for line in source:
		export = False
		lineNum += 1
		code, octothorpe, comment = line.partition('#')
		code, semicolon, comment = code.partition(';')
		code = code.strip()

		if code == "":
			continue

		tokens = code.split()
		if tokens[0] == 'export':
			export = True
			tokens = tokens[1:]
		if tokens[0][0] == '.':
			if len(tokens) > 2:
				error('line '+str(line)+': Invalid label syntax, too many tokens.')
			elif len(tokens) > 1:
				labels[tokens[0][1:]] = int(tokens[1], 0)
			else:
				labels[tokens[0][1:]] = progOffset
			if export:
				exportSymbol(tokens[0][1:])
			debug("%d:%s -> %X" % (lineNum, tokens[0][1:], labels[tokens[0][1:]]))
		elif tokens[0] == 'w':
			progOffset += 4
		elif tokens[0] == 'b':
			progOffset += 1
		else:
			exportDebug(lineNum, progOffset);
			progOffset += 8

def parseOperand(opr, modes, line, bits):
	modebit = 0
	absolute = 0
	if opr[0] == '@':
		if '@' not in modes:
			error('line '+str(line)+': Operand does not support absolute addressing '+str(opr))
		absolute = 1
		opr = opr[1:] #strip @ symbol
	if opr in alias:
		opr = alias[opr]
	if opr[0] == '.':
		label, plus, number = opr.partition('+')
		if label[1:] not in labels:
			error('line '+str(line)+': Can\'t find label \''+label+'\'')
		offset = 0
		if plus:
			offset = int(number, 0)
		opr = str(labels[label[1:]] + offset)
	if opr[0] == 'r':
		if 'r' not in modes:
			error('line '+str(line)+': Operand does not support register direct mode.')
		if 'r' in modes and 'c' in modes:
			modebit = 1
		return format(int(opr[1:]), bits), modebit, absolute
	else:
		if 'c' not in modes:
			error('line '+str(line)+': Operand does not support immediate mode '+str(opr))
		if opr[0] == '\'' and opr[2] == '\'':
			opr = str(ord(opr[1]))
		return format(int(opr, 0), bits), modebit, absolute

def assemble(source):

	loadLabels(source)
	source.seek(0)

	# executable binary header
	if header is True:
		output(format(magicNumber, '032b'))
		output(format(baseAddress, '032b'), '\n')
		output(format(stackAddress, '032b'))
		output(format(stackSize, '032b'), '\n')
		output(format(heapAddress, '032b'))
		output(format(heapSize, '032b'), '\n')

	lineNum = 0
	for line in source:
		lineNum += 1
		code, octothorpe, comment = line.partition('#')
		code, semicolon, comment = code.partition(';')
		code = code.strip()
		if code == "":
			continue

		tokens = code.split()
		i = tokens[0]
		args = tokens[1:]
		#print i, args

		if i[0] == '.' or i == 'export':
			continue # skip label definitions

		if i == 'w':
			opr0, m0, a0 = parseOperand(args[0], 'c', lineNum, '032b')
			output(opr0, '\n')
			continue;
		if i == 'b':
			opr0, m0, a0 = parseOperand(args[0], 'c', lineNum, '08b')
			output(opr0, '\n')
			continue;

		if i not in iset:
			error('line '+str(lineNum)+': Unrecognized assembly instruction: '+i)
			sys.exit(1)

		op = iset[i]
		opcode = op['opcode']
		opr0 = opr1 = opr2 = None
		m0 = m1 = m2 = 0
		a0 = a1 = a2 = 0

		if int(op['operands']) != len(args):
			error('line '+str(lineNum)+': Expected '+str(op['operands'])+' operands but found '+str(len(args)))
			sys.exit(1)

		#debug(op)
		#debug(args)

		if len(args) > 0:
			opr0, m0, a0 = parseOperand(args[0], op['opr0'], lineNum, '032b' if op['var'] == 0 else '08b')
		if len(args) > 1:
			opr1, m1, a1 = parseOperand(args[1], op['opr1'], lineNum, '032b' if op['var'] == 1 else '08b')
		if len(args) > 2:
			opr2, m2, a2 = parseOperand(args[2], op['opr2'], lineNum, '032b' if op['var'] == 2 else '08b')

		mode = m0+m1+m2
		if mode > 1:
			error('line '+str(lineNum)+': Only one operand can be variable mode:'+str(m0)+str(m1)+str(m2))
			sys.exit(1)

		absolute = a0+a1+a2
		if absolute > 1:
			error('line '+str(lineNum)+': Only one operand can use absolute addressing:'+str(a0)+str(a1)+str(a2))
			sys.exit(1)

		mode = (mode | (absolute << 1))
		mode = format(int(mode), '08b')
		# Only one operand is variable and which one differs based on the opcode.
		# Makes this assembler code look ugly, but lets us have compact binaries.
		if op['var'] == 0:
			packInstruction(opcode, mode, opr1, opr2, opr0)
		if op['var'] == 1:
			packInstruction(opcode, mode, opr0, opr2, opr1)
		if op['var'] == 2:
			packInstruction(opcode, mode, opr0, opr1, opr2)

def packInstruction(opcode, mode, opr0, opr1, opr2):
	if opr0 is None:
		opr0 = format(0, '08b')
	if opr1 is None:
		opr1 = format(0, '08b')
	if opr2 is None:
		opr2 = format(0, '032b')

	output(opcode)
	output(mode)
	output(opr0)
	output(opr1)
	output(opr2, '\n')

options = [
	('h','help','This help.'),
	('a:','assemble=','Translate assembly language into machine code.'),
	('d:','disassemble=','Translate machine code into assembly language.'),
	('b','binary','Output machine code to binary file.'),
	('t','text','Output machine code to ascii binary file.'),
	('e','symbols','Output exported symbols to ABI file.'),
	('g','debug','Output debug info to file.'),
	('s:','base-address','Base address where the binary will start in memory.')]
shortOpt = "".join([opt[0] for opt in options])
longOpt = [opt[1] for opt in options]

def usage():
	pad = str(len(max(longOpt, key=len)))
	fmt = '  -%s, --%-'+pad+'s : %s'
	debug('Usage: '+sys.argv[0]+' [options]\n')
	for opt in options:
		debug(fmt%(opt[0][0], opt[1], opt[2]))
	sys.exit(1)

def main():
	global binaryFile, asciiFile, symbolFile, debugFile
	global baseAddress
	base = None

	try:
		opts, args = getopt.getopt(sys.argv[1:], shortOpt, longOpt)
	except getopt.GetoptError as err:
		debug(str(err))
		usage()

	parseISA()

	for o, a in opts:
		if o in ('-h', '--help'):
			usage()
		elif o in ('-a', '--assemble'):
			base = os.path.splitext(a)[0]
			source = open(a, "r")
			doAssemble = True
		elif o in ('-d', '--disassemble'):
			rom = open(a, "r");

		elif o in ('-b', '--binary'):
			binaryFile = open(base + '.bin', "wb")
		elif o in ('-t', '--text'):
			asciiFile = open(base + '.rom', "w")
		elif o in ('-e', '--symbols'):
			symbolFile = open(base + '.sym', "w")
		elif o in ('-g', '--debug'):
			debugFile = open(base + '.debug', "w")

		elif o in ('-s', '--base-address'):
			baseAddress = int(a, 0)

		#TODO: finish implementing header info
		elif o in ('-s', '--stack-address'):
			stackAddress = int(a, 0)
		elif o in ('-z', '--stack-size'):
			stackSize = int(a, 0)
		elif o in ('-p', '--heap-address'):
			heapAddress = int(a, 0)
		elif o in ('-z', '--heap-size'):
			heapSize = int(a, 0)

		else:
			assert False, 'Unhandled option: ' + str(o)

	if doAssemble:
		assemble(source)

if __name__ == "__main__":
	main()

