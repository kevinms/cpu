#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include "list.h"

static char *file;
static int dumpParseTree;

static struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"parse-tree", no_argument, NULL, 'p'},
};

static char *optdesc[] = {
	"This help.",
	"Dump parse tree.",
};

static char *optstring = NULL;

static void
usage(int argc, char **argv)
{
	int i;
	int numOptions;
	int longest;

	printf("%s [OPTION...]\n\n", argv[0]);

	numOptions = sizeof(longopts) / sizeof(*longopts);
	longest = 0;
	for (i = 0; i < numOptions; ++i) {
		if (longest < strlen(longopts[i].name)) {
			longest = strlen(longopts[i].name);
		}
	}
	for (i = 0; i < numOptions; ++i) {
		printf("--%-*s, -%c : %s\n", longest, longopts[i].name, longopts[i].val, optdesc[i]);
	}
	printf("\n");
}

static void
mainArgs(int argc, char **argv)
{
	int c, i;
	int longindex;
	int numOptions;

	numOptions = sizeof(longopts) / sizeof(*longopts);
	optstring = malloc(numOptions * 3);
	memset(optstring, 0, numOptions * 3);

	c = 0;
	for (i = 0; i < numOptions; ++i) {
		c += sprintf(optstring+c, "%c%s", (char)longopts[i].val,
					 longopts[i].has_arg == no_argument ? "" :
					 longopts[i].has_arg == required_argument ? ":" :
					 "::");
	}

	while ((c = getopt_long(argc, argv, optstring, longopts, &longindex)) >= 0) {
		switch (c) {
			case 'h':
				usage(argc, argv);
				break;
			case 'p':
				dumpParseTree = 1;
				break;
			case '?':
				break;
			default:
				exit(1);
		}
	}

	if (optind < argc) {
		file = strdup(argv[optind]);
	}

	if (file == NULL) {
		fprintf(stderr, "Error: Expected a source file path.\n");
		exit(1);
	}
}

/*
 * Delimiters   | ;
 * Whitespace
 * 'string'
 * # comments
 */

/*
char *symMathBinary[] = {
	"|","&","^",
	"||","&&",
	"+","-","*","/","%",
	"==","!=","<",">","<=",">=",
}

char *symMathUnary[] = {
	"~",
	"!",
	"-",
}
*/

char *symbols[] = {
	"|","&","^",
	"||","&&",
	"+","-","*","/","%",
	"==","!=","<",">","<=",">=",
	"~",
	"!",
	"-",
	"=",
	".","->",
	"[","]","(",")",",",
	"u8","u32","void",
	"struct",
	";","{","}",
	"if","else",
	"while","for","break","continue",
	"return"
};
#define NUM_SYMBOLS (sizeof(symbols) / sizeof(*symbols))

#define T_TERMINAL    0x1
#define T_IDENTIFIER  0x2
#define T_LITERAL_HEX 0x4
#define T_LITERAL_DEC 0x8
#define T_LITERAL_STR 0x10
#define T_LITERAL_CHR 0x20
#define T_LITERAL (T_LITERAL_HEX | T_LITERAL_DEC | T_LITERAL_STR | T_LITERAL_CHR)

typedef struct Token {
	char *string;
	int id;
	int type;
	int line;
	void *private;
} Token;

typedef struct Type {
	Token *token;
	int size;
	int ptr;
} Type;

typedef struct Arg {
	Type type;
	char *name;
} Arg;

typedef struct Variable {
	Type type;
	char *name;
	int address;
	Token *initValue;
} Variable;

typedef struct Statement {
	// Token
	List tokens;
} Statement;

typedef struct Function {
	Type type;
	char *name;

	// Arg
	List args;

	// Statement
	List statements;
} Function;

typedef struct Struct {
	char *name;

	// Arg
	List members;
} Struct;

typedef struct Program {
	/*
	 * Raw tokens.
	 */
	Token **tokenArray;
	int tokenCount;
	int i;

	int lineCount;

	// Struct
	List structs;

	// Variable
	List vars;

	// Function
	List functions;
} Program;

void
fatal(const char *fmt, ...)
{
	va_list args;

	fprintf(stderr, "%s(): Error: ", __func__);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	abort();
}

#define ERROR_CONTEXT 5

void
error(Token *token, const char *fmt, ...)
{
	va_list args;

	/*
	 * Print lines of context.
	 */
	fprintf(stderr, "Source context:\n");

	Program *prog = token->private;
	int first = token->line - (ERROR_CONTEXT / 2);
	if (first < 0) {
		first = 0;
	}
	int last = first + ERROR_CONTEXT;
	int currentLine = first - 1;
	int i;
	for (i = 0; i < prog->tokenCount; i++) {
		Token *thisToken = prog->tokenArray[i];
		if (thisToken->line >= last) {
			break;
		}
		if (thisToken->line >= first) {
			if (thisToken->line > currentLine) {
				while (thisToken->line > currentLine) {
					fprintf(stderr, "\n");
					currentLine++;
					fprintf(stderr, " %4d: ", currentLine);
				}
			}
			fprintf(stderr, "%s ", thisToken->string);
		}
	}
	fprintf(stderr, "\n\n");

	fprintf(stderr, "Line %d: ", token->line);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "  Token: %s\n", token->string);

	abort();
}

static Token *
nextToken(Program *prog, int abortOnNull)
{
	if (prog->i+1 >= prog->tokenCount) {
		if (abortOnNull) {
			abort();
		}
		return NULL;
	}
	prog->i++;
	return prog->tokenArray[prog->i];
}

static int
isAlpha(char c)
{
	if (((c >= 0x41) && (c <= 0x5A)) ||
		((c >= 0x61) && (c <= 0x7A))) {
		return 1;
	}
	return 0;
}

static int
isNum(char c)
{
	if ((c >= 0x30) && (c <= 0x39)) {
		return 1;
	}
	return 0;
}

static int
isIdentifier(char *token)
{
	int i;

	if ((token[0] != '_') && !isAlpha(token[0])) {
		return 0;
	}
	for (i = 1; token[i] != '\0'; i++) {
		if ((token[i] != '_') && !isAlpha(token[i]) && !isNum(token[i])) {
			return 0;
		}
	}
	return 1;
}

static int
isHex(char c)
{
	if ((((c >= 0x41) && (c <= 0x46)) ||
		 ((c >= 0x61) && (c <= 0x66))) ||
		(isNum(c) != 0)) {
		return 1;
	}
	return 0;
}

static int
isLiteralHex(char *token)
{
	int i;

	if (strncmp(token, "0x", 2) != 0) {
		return 0;
	}
	for (i = 2; token[i] != '\0'; i++) {
		if (isHex(token[i]) == 0) {
			return 0;
		}
	}
	if (i > 2) {
		return 1;
	}
	return 2;
}

static int
isLiteralDec(char *token)
{
	int i;

	for (i = 0; token[i] != '\0'; i++) {
		if (isNum(token[i]) == 0) {
			return 0;
		}
	}
	return 1;
}

static int
isLiteralStr(char *token)
{
	int len = strlen(token);

	if (len == 0) {
		return 0;
	}

	if (token[0] == '"') {
		int i, complete = 0;
		for (i = 1; i < len; i++) {
			if (token[i] == '\\') {
				i++;
				continue;
			}
			if (token[i] == '"') {
				i++;
				complete = 1;
				break;
			}
		}
		if (i < len) {
			return 0;
		}
		if (complete) {
			return 1;
		}
		return 2;
	}

	return 0;
}

static int
isLiteralChr(char *token)
{
	int len = strlen(token);

	if (len == 0) {
		return 0;
	}

	if (token[0] == '\'') {
		int i, complete = 0, chars = 1;
		for (i = 1; i < len && chars < 3; i++) {
			chars++;
			if (token[i] == '\\') {
				i++;
				continue;
			}
			if (token[i] == '\'') {
				i++;
				complete = 1;
				break;
			}
		}
		if (i < len) {
			return 0;
		}
		if (complete) {
			return 1;
		}
		return 2;
	}

	return 0;
}

/*
 * 0  Not a literal.
 * 1  A literal.
 * 2  An incomplete literal.
 */
static int
isLiteral(char *token, int *type)
{
	int len = strlen(token);
	int ret = 0;

	if (type == NULL) {
		fatal("Invalid argument.");
	}
	*type = 0;

	if (len == 0) {
		return 0;
	}

	/*
	 * Numeric literal.
	 */
	if ((ret = isLiteralHex(token)) != 0) {
		*type = T_LITERAL_HEX;
		return ret;
	}
	if (isLiteralDec(token) != 0) {
		*type = T_LITERAL_DEC;
		return 1;
	}

	if ((ret = isLiteralStr(token)) != 0) {
		*type = T_LITERAL_STR;
		return ret;
	}
	if ((ret = isLiteralChr(token)) != 0) {
		*type = T_LITERAL_CHR;
		return ret;
	}

	return 0;
}

static int
readUntil(Program *prog, FILE *fp, char *sentinel)
{
	int c, i;
	char token[4096];
	int len = 0;
	int sentinelLen = strlen(sentinel);

	memset(token, 0, sizeof(token));
	while (((c = fgetc(fp)) != EOF) && (ferror(fp) == 0)) {
		if (c == '\n') {
			prog->lineCount++;
		}
		if (len == sentinelLen) {
			/*
			 * Shift characters left one place.
			 */
			for (i = 0; i < sentinelLen; i++) {
				token[i] = token[i + 1];
			}
			len--;
		}
		token[len] = c;
		len++;
		if (strcmp(token, sentinel) == 0) {
			return 0;
		}
	}

	return -1;
}

/*
 * Caller must free the token that is returned.
 */
static Token *
readToken(FILE *fp, Program *prog)
{
	int c, i;
	char buf[4096];
	int type = T_TERMINAL;
	int len = 0;
	int partialMatch = 0;
	int line = 0;

	memset(buf, 0, sizeof(buf));

	while (((c = fgetc(fp)) != EOF) && (ferror(fp) == 0)) {
		if (c == '\n') {
			prog->lineCount++;
		}
		if ((c == ' ') || (c == '\n') || (c == '\t')) {
			/*
			 * White space is always a delimiter.
			 */
			if (partialMatch == 0) {
				if (len == 0) {
					continue;
				}
				break;
			}
		}

		/*
		 * Add the char to the buf before we know if it will match.
		 * If it causes a mismatch, we will remove it.
		 */
		buf[len] = c;
		len++;

		if (len == 1) {
			line = prog->lineCount;
		}

		if (strcmp(buf, "/*") == 0) {
			/*
			 * Skip over multiline comment.
			 */
			if (readUntil(prog, fp, "*/") < 0) {
				fatal("Failed to read until '*/'\n");
			}
			len = 0;
			memset(buf, 0, sizeof(buf));
			continue;
		}
		if (strcmp(buf, "//") == 0) {
			/*
			 * Skip over single line comment.
			 */
			if (readUntil(prog, fp, "\n") < 0) {
				fatal("Failed to read until '\\n'\n");
			}
			len = 0;
			memset(buf, 0, sizeof(buf));
			continue;
		}

		/*
		 * Find longest matching terminal.
		 */
		for (i = 0; i < NUM_SYMBOLS; i++) {
			if (strcmp(symbols[i], buf) == 0) {
				//fprintf(stderr, "Match: (%s) -> %s\n", symbols[i], buf);
				break;
			}
		}
		if (i < NUM_SYMBOLS) {
			type = T_TERMINAL;
			continue;
		}

		if (isIdentifier(buf) != 0) {
			//fprintf(stderr, "Match ident: %s\n", buf);
			type = T_IDENTIFIER;
			continue;
		}

		int literalType;
		i = isLiteral(buf, &literalType);
		if (i != 0) {
			type = literalType;
			partialMatch = (i == 2) ? 1 : 0;
			continue;
		}

		/*
		 * We didn't find a match. Remove the last char.
		 * Seek back and break.
		 */
		fseek(fp, -1, SEEK_CUR);
		len--;
		buf[len] = '\0';
		break;
	}

	if (partialMatch != 0) {
		fatal("Incomplete match: %s\n", buf);
	}
	if (ferror(fp) != 0) {
		fatal("File error.\n");
	}
	if (len == 0) {
		if (feof(fp) != 0) {
			return NULL;
		}
		fatal("Failed to match next token.\n");
	}

	Token *token;

	if ((token = malloc(sizeof(*token))) == NULL) {
		fatal("Can't allocate token: %s\n", strerror(errno));
	}
	memset(token, 0, sizeof(*token));

	token->string = strdup(buf);
	token->type = type;
	token->line = line;
	token->private = prog;

	return token;
}

static int
printToken(Token *token, int pretty)
{
	char *type = "";

	if (pretty == 0) {
		fprintf(stderr, "%d ", token->line);
		if (token->type == T_TERMINAL) {
			type = "t";
		}
		else if (token->type & T_LITERAL) {
			type = "l";
		}
		else if (token->type == T_IDENTIFIER) {
			type = "i";
		} else {
			fatal("Token does not have a valid type?! %s\n", token->string);
		}
		fprintf(stderr, "%s: ", type);
	}

	fprintf(stderr, "%s", token->string);

	if (pretty != 0) {
		if (!strcmp(token->string, ";") ||
			!strcmp(token->string, "{") ||
			!strcmp(token->string, "}")) {
			fprintf(stderr, "\n");
		} else {
			fprintf(stderr, " ");
		}
	} else {
		fprintf(stderr, "\n");
	}

	return 0;
}

static int
lexer(char *path, Program *prog)
{
	FILE *fp;
	Token *token;
	Token **tokenArray = NULL;
	int tokenCount = 0;

	if ((fp = fopen(path, "r")) == NULL) {
		fatal("Can't open %s: %s\n", path, strerror(errno));
	}

	prog->lineCount = 1;

	while (((token = readToken(fp, prog)) != NULL) &&
		   (token != (Token *)-1)) {

		//printToken(token, 1);

		/*
		 * Increase the token array size.
		 */
		tokenCount++;
		tokenArray = realloc(tokenArray, sizeof(*tokenArray) * tokenCount);
		if (tokenArray == NULL) {
			fatal("Can't grow tokenArray: %s\n", strerror(errno));
			abort();
		}

		tokenArray[tokenCount-1] = token;
	}

	prog->tokenArray = tokenArray;
	prog->tokenCount = tokenCount;
	prog->i = -1;

	while ((token = nextToken(prog, 0)) != NULL) {
		printToken(token, 1);
	}
	fprintf(stderr, "Read %d tokens on %d lines.\n",
			tokenCount, tokenArray[tokenCount-1]->line);

	prog->i = -1;

	return 0;
}

void
printType(Type *type, int newline)
{
	printf("%s", type->token->string);
	if (type->ptr) {
		printf(" *");
	}
	if (newline) {
		printf("\n");
	}
}

void
printArg(Arg *arg)
{
	printType(&arg->type, 0);
	printf(" %s", arg->name);
}

void
printVariable(Variable *var)
{
	printType(&var->type, 0);
	printf(" %s", var->name);
	if (var->initValue != NULL) {
		printf(" = %s", var->initValue->string);
	}
	printf(";\n");
}

void
printStruct(Struct *s)
{
	printf("struct %s {\n", s->name);
	ListEntry *entry = NULL;
	while (listNext(&s->members, &entry) != NULL) {
		Arg *arg = entry->data;
		printArg(arg);

		printf(";\n");
	}
	printf("}\n");
}

void
printFunction(Function *f)
{
	printf("func %s(", f->name);

	ListEntry *entry = NULL;
	while (listNext(&f->args, &entry) != NULL) {
		Arg *arg = entry->data;
		printArg(arg);

		if (entry != f->args.tail) {
			printf(", ");
		}
	}
	printf(") ");

	printType(&f->type, 0);

	printf(" {\n");

	entry = NULL;
	while (listNext(&f->statements, &entry) != NULL) {
		Token *token = entry->data;
		printToken(token, 1);
	}

	printf("}\n");
}

/*
 * == 0 on mismatch
 * != 0 on match
 */
static int
match(Token *token, char *string)
{
	if (strcmp(token->string, string) != 0) {
		return 0;
	}
	return 1;
}

/*
 * == 0 on mismatch
 * != 0 on match
 */
static int
isType(Program *p, Token *t)
{
	if (match(t, "u8") ||
		match(t, "u32") ||
		match(t, "ptr")) {
		return 1;
	}

	ListEntry *entry = NULL;
	while (listNext(&p->structs, &entry) != NULL) {
		Struct *s = entry->data;
		if (match(t, s->name)) {
			return 1;
		}
	}
	return 0;
}

/*
typedef struct Type {
	Token *token;
	int size;
	int ptr;
} Type;
*/

/*
 * u8 u32 : Primitive types.
 *      * : Pointers.
 *     [] : Array.
 *
 * <u8 | u32 | void | <struct>> [*[*]] <ident>;
 */
static Type *
parseType(Program *prog, Type *type, int allowArray)
{
	if (type == NULL) {
		if ((type = malloc(sizeof(*type))) == NULL) {
			fatal("Can't allocate type object: %s\n", strerror(errno));
		}
	}
	memset(type, 0, sizeof(*type));

	Token *token = nextToken(prog, 1);
	if (!isType(prog, token)) {
		error(token, "Expected type name.\n");
	}
	type->token = token;

	token = nextToken(prog, 1);
	if (allowArray && match(token, "[")) {
		/*
		 * Array.
		 */
		token = nextToken(prog, 1);
		if ((token->type & (T_LITERAL_HEX | T_LITERAL_DEC)) == 0) {
			error(token, "Array size must be numerical literal.\n");
		}
		token = nextToken(prog, 1);
		if (!match(token, "]")) {
			error(token, "Expected closing ]\n");
		}
	} else if (match(token, "*")) {
		/*
		 * Pointer.
		 */
		token = nextToken(prog, 1);
		while (match(token, "*")) {
			token = nextToken(prog, 1);
		}
		prog->i--;
	} else {
		/*
		 * Oops, guess we didn't need the token.
		 */
		prog->i--;
	}

	return type;
}

/*
typedef struct Arg {
	Type type;
	char *name;
} Arg;
*/

/*
 * Parse a single argument. If 'arg' is NULL then a new object will
 * be allocated.
 */
static Arg *
parseArg(Program *prog, Arg *arg)
{
	fprintf(stderr, "Parsing argument.\n");

	if (arg == NULL) {
		if ((arg = malloc(sizeof(*arg))) == NULL) {
			fatal("Can't allocate Arg object: %s\n", strerror(errno));
		}
	}
	memset(arg, 0, sizeof(*arg));

	parseType(prog, &arg->type, 0);

	Token *token = nextToken(prog, 1);
	if (token->type != T_IDENTIFIER) {
		error(token, "Expected identifier.\n");
	}
	arg->name = token->string;

	return arg;
}

/*
 * func <ident> ( [ <type> <ident> [ , <type> <ident> ] ] ) [ <type> ] { }
 *
 * func foo(u8 a, u32 b, ptr c) void * { }
 *
 *  < 0  Error
 * == 0  Not a function def
 *  > 0  Parsed function def
 */
static Function *
parseFunction(Program *prog)
{
	fprintf(stderr, "Parsing function.\n");

	Token *token = NULL;
	//int i = prog->i;

	Function *func = NULL;
	if ((func = malloc(sizeof(*func))) == NULL) {
		fatal("Can't allocate func object: %s\n", strerror(errno));
	}
	memset(func, 0, sizeof(*func));

	token = nextToken(prog, 1);
	if (token->type != T_IDENTIFIER) {
		error(token, "Expected function name.\n");
	}
	func->name = token->string;

	token = nextToken(prog, 1);
	if (!match(token, "(")) {
		error(token, "Expected (\n");
	}

	token = nextToken(prog, 1);
	while (!match(token, ")")) {
		prog->i--;
		Arg *arg = parseArg(prog, NULL);
		listAppend(&func->args, arg);

		token = nextToken(prog, 1);
		if (match(token, ",")) {
			if (match(token, ")")) {
				error(token, "In funtion header, expected )\n");
			}
			token = nextToken(prog, 1);
		}
	}

	token = nextToken(prog, 1);
	if (!match(token, "{")) {
		prog->i--;
		parseType(prog, &func->type, 0);
	}

	token = nextToken(prog, 1);
	if (!match(token, "{")) {
		error(token, "After function header, expected {\n");
		abort();
	}

	int braceStack = 0;
	while ((token = nextToken(prog, 0)) != NULL) {
		if (match(token, "{")) {
			braceStack++;
		}
		if (match(token, "}")) {
			if (braceStack == 0) {
				break;
			}
			braceStack--;
		}
		listAppend(&func->statements, token);
	}

	return func;
}

static Variable *
parseVariable(Program *prog)
{
	fprintf(stderr, "Parsing variable.\n");

	Token *token = NULL;

	Variable *var = NULL;
	if ((var = malloc(sizeof(*var))) == NULL) {
		fatal("Can't allocate var object: %s\n", strerror(errno));
	}
	memset(var, 0, sizeof(*var));

	parseType(prog, &var->type, 1);

	token = nextToken(prog, 1);
	if (token->type != T_IDENTIFIER) {
		error(token, "Expected variable name.\n");
	}
	var->name = token->string;

	token = nextToken(prog, 1);
	if (match(token, "=")) {
		/*
		 * Initialize variable to literal.
		 */
		token = nextToken(prog, 1);
		printf("0x%x\n", token->type);
		if ((token->type & T_LITERAL) == 0) {
			error(token, "Variables can only be initialized to literals.\n");
		}
		var->initValue = token;
		token = nextToken(prog, 1);
	}
	
	if (!match(token, ";")) {
		error(token, "Expected ;\n");
	}

	return var;
}

static Struct *
parseStruct(Program *prog)
{
	fprintf(stderr, "Parsing struct.\n");

	Token *token = NULL;

	Struct *s = NULL;
	if ((s = malloc(sizeof(*s))) == NULL) {
		fatal("Can't allocate var object: %s\n", strerror(errno));
	}
	memset(s, 0, sizeof(*s));

	token = nextToken(prog, 1);
	if (token->type != T_IDENTIFIER) {
		error(token, "Expected struct name.\n");
	}
	s->name = token->string;

	token = nextToken(prog, 1);
	if (!match(token, "{")) {
		error(token, "Expected {\n");
	}

	token = nextToken(prog, 1);
	while (!match(token, "}")) {
		prog->i--;
		Arg *arg = parseArg(prog, NULL);
		listAppend(&s->members, arg);

		token = nextToken(prog, 1);
		if (!match(token, ";")) {
			error(token, "Expected ;\n");
		}
		token = nextToken(prog, 1);
	}

	return s;
}

static int
parser(Program *prog)
{
	Token *token;
	while ((token = nextToken(prog, 0)) != NULL) {
		/*
		 * At global scope:
		 * 
		 *  Function definition
		 *    func <type> <ident> ( [ <type> <name> [ , <type> <name> ] ] ) <type> { }
		 *  Struct definition
		 *    struct <ident> {
		 *      <type> <ident>,
		 *      <type> <ident>
		 *    };
		 *  Variable declaration (initialize to constant)
		 *    <type> <ident> [ = <literal> ];
		 */

		if (match(token, "func")) {
			Function *func = parseFunction(prog);
			listAppend(&prog->functions, func);
		}
		else if (match(token, "struct")) {
			Struct *s = parseStruct(prog);
			listAppend(&prog->structs, s);
		}
		else if (isType(prog, token)) {
			prog->i--;
			Variable *var = parseVariable(prog);
			listAppend(&prog->vars, var);
		} else {
			error(token, "Only 'func', 'struct' and variables are allowed in global scope.\n");
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	Program prog;
	memset(&prog, 0, sizeof(prog));

	mainArgs(argc, argv);

	lexer(file, &prog);
	parser(&prog);

	ListEntry *entry;

	entry = NULL;
	while (listNext(&prog.vars, &entry) != NULL) {
		Variable *var = entry->data;
		printVariable(var);
	}

	entry = NULL;
	while (listNext(&prog.structs, &entry) != NULL) {
		Struct *s = entry->data;
		printStruct(s);
	}

	entry = NULL;
	while (listNext(&prog.functions, &entry) != NULL) {
		Function *f = entry->data;
		printFunction(f);
	}

	return 0;
}
