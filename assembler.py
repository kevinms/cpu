#!/usr/bin/python

import getopt, sys, re

iset = {}   # instruction set
labels = {} # label table
alias = {'sp':'r11', 'ba':'r12', 'fl':'r13', 'c1':'r14', 'c2':'r15'}

def error(msg):
	print >> sys.stderr, msg
	sys.exit(1)
def debug(msg):
	print >> sys.stderr, msg

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
		#print i, o

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

		#print op

		iset[i[1]] = op

	#for i in iset:
	#	print i, iset[i]

def loadLabels(source):
	lineNum = 0
	progOffset = 0
	for line in source:
		lineNum += 1
		code, octothorpe, comment = line.partition('#')
		code, semicolon, comment = code.partition(';')
		code = code.strip()

		if code == "":
			continue

		tokens = code.split()
		if tokens[0][0] == '.':
			if len(tokens) > 2:
				error('line '+str(line)+': Invalid label syntax, too many tokens.')
			elif len(tokens) > 1:
				labels[tokens[0][1:]] = int(tokens[1], 0)
			else:
				labels[tokens[0][1:]] = progOffset
			debug(str(lineNum)+': '+tokens[0][1:]+' -> '+str(labels[tokens[0][1:]]))
		elif tokens[0] == 'w':
			progOffset += 2
		elif tokens[0] == 'b':
			progOffset += 1
		else:
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
		#return format(labels[opr[1:]], bits), modebit, absolute
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

		if i[0] == '.':
			continue

		if i == 'w':
			opr0, m0, a0 = parseOperand(args[0], 'c', lineNum, '032b')
			print opr0
			continue;
		if i == 'b':
			opr0, m0, a0 = parseOperand(args[0], 'c', lineNum, '08b')
			print opr0
			continue;

		op = iset[i]
		opcode = op['opcode']
		opr0 = opr1 = opr2 = None
		m0 = m1 = m2 = 0
		a0 = a1 = a2 = 0

		if int(op['operands']) != len(args):
			error('line '+str(lineNum)+': Expected '+str(op['operands'])+' operands but found '+str(len(args)))
			sys.exit(1)

		#print op
		#print args

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
		# Makes the assembly look ugly, but lets us be compact.
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
	print opcode, mode, opr0, opr1, opr2

options = [
	('h','help','This help.'),
	('a:','assemble=','Translate assembly language into machine code.'),
	('d:','disassemble=','Translate machine code into assembly language.')]
shortOpt = "".join([opt[0] for opt in options])
longOpt = [opt[1] for opt in options]

def usage():
	pad = str(len(max(longOpt, key=len)))
	fmt = '  -%s, --%-'+pad+'s : %s'
	print 'Usage: '+sys.argv[0]+' [options]\n'
	for opt in options:
		print fmt%(opt[0][0], opt[1], opt[2])
	sys.exit(1)


def main():
	try:
		opts, args = getopt.getopt(sys.argv[1:], shortOpt, longOpt)
	except getopt.GetoptError as err:
		print str(err)
		usage()

	parseISA()

	for o, a in opts:
		if o in ('-h', '--help'):
			usage()
		elif o in ('-a', '--assemble'):
			source = open(a, "r")
			assemble(source)
		elif o in ('-d', '--disassemble'):
			rom = open(a, "r");
			pass
		else:
			assert False, 'Unhandled option: ' + str(o)

if __name__ == "__main__":
	main()

