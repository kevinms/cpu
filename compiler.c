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
#include "lifo.h"

#define OPERATOR_SYMBOLS \
	"[","]","(",")",".","->", \
	"-","!","~","*","&","@","sizeof", \
	"*","/","%", \
	"+","-", \
	"<<",">>", \
	"<","<=",">",">=", \
	"==","!=", \
	"&", \
	"^", \
	"|", \
	"&&", \
	"||", \
	"=",

char *operatorSymbols[] = {
	OPERATOR_SYMBOLS
};
#define NUM_OPERATOR_SYMBOLS (sizeof(operatorSymbols) / sizeof(*operatorSymbols))

char *symbols[] = {
	OPERATOR_SYMBOLS
	"u8","u32","void",
	"struct",
	",",";","{","}",
	"if","else",
	"while","for","break","continue",
	"return",
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

typedef struct Struct {
	char *name;
	int size;

	// Parameter
	List members;
} Struct;

#define TYPE_U8 0x1
#define TYPE_U32 0x2
#define TYPE_STRUCT 0x4
#define TYPE_PTR 0x8

/*
 * Arrays are just pointers to a region of memory.
 * This type isn't really necessary.
 * We do need someway to keep track of the array size.
 */
#define TYPE_ARRAY 0x10

typedef struct Type {
	Token *token;

	int flags;
	int size;

	int ptrCount;
	int arraySize;
	Struct *structType;
} Type;

typedef struct Parameter {
	Type type;
	char *name;
} Parameter;

typedef struct Variable {
	Type type;
	char *name;
	int address;
	Token *value;
} Variable;

typedef struct Argument {
	Type type;
	char *name;
} Argument;

typedef struct FunctionCall {
	char *name;

	// Expression
	List argExprList;
} FunctionCall;

#define EXPR_OPERATOR 0x1
#define EXPR_LITERAL 0x2
#define EXPR_VAR 0x4
#define EXPR_FUNC 0x8
#define EXPR_OPERAND (EXPR_LITERAL | EXPR_VAR | EXPR_FUNC)

#define ASSOC_LEFT_TO_RIGHT 1
#define ASSOC_RIGHT_TO_LEFT 2

/*
 * <operand> := <variable> | <fn>
 * <operator> := <math symbol>
 */
typedef struct Oper {
	int flags;
	Token *token;
	FunctionCall *call;

	int isBinary;
	int precedence;
	int associativity;
} Oper;

typedef struct Expression {
	// Oper
	List postfix;
} Expression;

#define STMT_IF 0x1
#define STMT_ELIF 0x2
#define STMT_ELSE 0x4
#define STMT_WHILE 0x8
#define STMT_RETURN 0x10
#define STMT_VAR_DECL 0x20
#define STMT_EXPR 0x40

typedef struct Statement {
	// Token
	List tokens;

	int flags;

	Variable *var;
	Expression *expr;
	List statements;
} Statement;

typedef struct Function {
	Type type;
	char *name;

	List variables;

	// Parameter
	List params;

	// Statement
	List statements;
} Function;

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

	FILE *outFP;
} Program;

static char *inFile;
static char *outFile;
static int dumpParseTree;

static struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"parse-tree", no_argument, NULL, 'p'},
	{"output", required_argument, NULL, 'o'},
};

static char *optdesc[] = {
	"This help.",
	"Dump parse tree.",
	"Name of file to write compiled output to.",
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
			case 'o':
				outFile = strdup(optarg);
				break;
			case '?':
				break;
			default:
				exit(1);
		}
	}

	if (optind < argc) {
		inFile = strdup(argv[optind]);
	}

	if (inFile == NULL) {
		fprintf(stderr, "Error: Expected a source file path.\n");
		exit(1);
	}
	if (outFile == NULL) {
		fprintf(stderr, "Error: Expected an output file path.\n");
		exit(1);
	}
}

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

static void
printType(Type *type, int newline)
{
	int i;

	printf("%s", type->token->string);
	if (type->flags & TYPE_PTR) {
		printf(" ");
		for (i = 0; i < type->ptrCount; i++) {
			printf("*");
		}
	}
	if (newline) {
		printf("\n");
	}
}

static void
printParameter(Parameter *param)
{
	printType(&param->type, 0);
	printf(" %s", param->name);
}

static void
printGlobalVariable(Variable *var)
{
	printType(&var->type, 0);
	printf(" %s", var->name);
	if (var->value != NULL) {
		printf(" = %s", var->value->string);
	}
	printf(";\n");
}

static void
printVariable(Variable *var)
{
	printType(&var->type, 0);
	printf(" %s", var->name);
	if (var->value != NULL) {
		printf(" = %s", var->value->string);
	}
	printf(";\n");
}

static void
printStruct(Struct *s)
{
	printf("struct %s {\n", s->name);
	ListEntry *entry = NULL;
	while (listNext(&s->members, &entry) != NULL) {
		Parameter *param = entry->data;
		printParameter(param);

		printf(";\n");
	}
	printf("}\n");
}

static void printExpression(Expression *e);

static void
printFunctionCall(FunctionCall *call)
{
	printf("%s(", call->name);
	ListEntry *entry = NULL;
	while (listNext(&call->argExprList, &entry) != NULL) {
		Expression *expr = entry->data;
		printExpression(expr);
	}
	printf(")");
}

static void
printExpression(Expression *e)
{
	//printf("E: ");
	ListEntry *entry = NULL;
	while (listNext(&e->postfix, &entry) != NULL) {
		Oper *o = entry->data;
		if (o->flags & EXPR_FUNC) {
			printFunctionCall(o->call);
		} else {
			printf("%s ", o->token->string);
		}
	}
}

static void printStatement(Statement *s);

static void
printBlock(Statement *s)
{
	printf(" {\n");
	ListEntry *entry = NULL;
	while (listNext(&s->statements, &entry) != NULL) {
		Statement *s = entry->data;
		printStatement(s);
	}
	printf("}\n");
}

static void
printConditionalBlock(Statement *s)
{
	printf("(");
	printExpression(s->expr);
	printf(")");
	printBlock(s);
}

static void
printStatement(Statement *s)
{
	if (s->flags & STMT_IF) {
		printf("if ");
		printConditionalBlock(s);
	} else if (s->flags & STMT_ELIF) {
		printf("elif ");
		printConditionalBlock(s);
	} else if (s->flags & STMT_ELSE) {
		printf("else ");
		printBlock(s);
	} else if (s->flags & STMT_WHILE) {
		printf("while ");
		printConditionalBlock(s);
	} else if (s->flags & STMT_RETURN) {
		printf("return ");
		printExpression(s->expr);
		printf(";\n");
	} else if (s->flags & STMT_VAR_DECL) {
		printVariable(s->var);
	} else if (s->flags & STMT_EXPR) {
		printExpression(s->expr);
		printf(";\n");
	}
}

static void
printFunction(Function *f)
{
	printf("fn %s(", f->name);

	ListEntry *entry = NULL;
	while (listNext(&f->params, &entry) != NULL) {
		Parameter *param = entry->data;
		printParameter(param);

		if (entry != f->params.tail) {
			printf(", ");
		}
	}
	printf(") ");

	printType(&f->type, 0);

	printf(" {\n");

	entry = NULL;
	while (listNext(&f->statements, &entry) != NULL) {
		Statement *s = entry->data;
		printStatement(s);
	}

	printf("}\n");
}

static void
print(Program *prog)
{
	ListEntry *entry;

	entry = NULL;
	while (listNext(&prog->vars, &entry) != NULL) {
		Variable *var = entry->data;
		printGlobalVariable(var);
	}

	entry = NULL;
	while (listNext(&prog->structs, &entry) != NULL) {
		Struct *s = entry->data;
		printStruct(s);
	}

	entry = NULL;
	while (listNext(&prog->functions, &entry) != NULL) {
		Function *f = entry->data;
		printFunction(f);
	}
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
matchChars(Token *token, char *string)
{
	char buf[2];
	buf[1] = '\0';

	int i;
	for (i = 0; i < strlen(string); i++) {
		buf[0] = string[i];
		if (strcmp(token->string, buf) == 0) {
			return 1;
		}
	}
	return 0;
}

/*
 * == 0 on mismatch
 * != 0 on match
 */
static int
isType(Program *p, Token *t)
{
	if (match(t, "u8") ||
		match(t, "u32")) {
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

static int
getTypeInfo(Program *p, Type *type)
{
	if (match(type->token, "u8")) {
		type->size = 1;
		type->flags = TYPE_U8;
		return 0;
	}
	if (match(type->token, "u32")) {
		type->size = 4;
		type->flags = TYPE_U32;
		return 0;
	}

	ListEntry *entry = NULL;
	while (listNext(&p->structs, &entry) != NULL) {
		Struct *s = entry->data;
		if (match(type->token, s->name)) {
			type->size = s->size;
			type->flags = TYPE_STRUCT;
			type->structType = s;
			return 0;
		}
	}

	error(type->token, "Type info of something that is not a type?!\n");
	return -1;
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

	getTypeInfo(prog, type);

	token = nextToken(prog, 1);
	if (allowArray && match(token, "[")) {
		/*
		 * Array.
		 */
		token = nextToken(prog, 1);
		if ((token->type & (T_LITERAL_HEX | T_LITERAL_DEC)) == 0) {
			error(token, "Array size must be numerical literal.\n");
		}
		type->arraySize = strtoul(token->string, NULL, 0);
		token = nextToken(prog, 1);
		if (!match(token, "]")) {
			error(token, "Expected closing ]\n");
		}
		type->flags |= TYPE_PTR;
	} else if (match(token, "*")) {
		/*
		 * Pointer.
		 */
		type->ptrCount = 1;
		token = nextToken(prog, 1);
		while (match(token, "*")) {
			token = nextToken(prog, 1);
			type->ptrCount++;
		}
		prog->i--;
		type->flags |= TYPE_PTR;
	} else {
		/*
		 * Oops, guess we didn't need the token.
		 */
		prog->i--;
	}


	return type;
}

/*
typedef struct Parameter {
	Type type;
	char *name;
} Parameter;
*/

/*
 * Parse a single parameter. If 'param' is NULL then a new object will
 * be allocated.
 */
static Parameter *
parseParameter(Program *prog, Parameter *param)
{
	fprintf(stderr, "Parsing parameter.\n");

	if (param == NULL) {
		if ((param = malloc(sizeof(*param))) == NULL) {
			fatal("Can't allocate Parameter object: %s\n", strerror(errno));
		}
	}
	memset(param, 0, sizeof(*param));

	parseType(prog, &param->type, 0);

	Token *token = nextToken(prog, 1);
	if (token->type != T_IDENTIFIER) {
		error(token, "Expected identifier.\n");
	}
	param->name = token->string;

	return param;
}

static Expression *parseExpression(Program *prog, char *sentinel);

/*
 * <ident> ( [ <expression> [ , <expression> ] ] )
 *
 * foo(<expression>, <expression>, <expression>)
 *
 *  < 0  Error
 * == 0  Not a function call def
 *  > 0  Parsed function call def
 */
static FunctionCall *
parseFunctionCall(Program *prog, char *name)
{
	fprintf(stderr, "Parsing function call.\n");

	FunctionCall *call = NULL;
	if ((call = malloc(sizeof(*call))) == NULL) {
		fatal("Can't allocate FunctionCall object: %s\n", strerror(errno));
	}
	memset(call, 0, sizeof(*call));

	call->name = name;

	Token *token = nextToken(prog, 1);
	if (!match(token, "(")) {
		error(token, "Expected (\n");
	}

	token = nextToken(prog, 1);
	while (!match(token, ")")) {
		prog->i--;
		Expression *expr = parseExpression(prog, ",)");
		listAppend(&call->argExprList, expr);

		token = nextToken(prog, 1);
		if (match(token, ",")) {
			/*
			 * There is another function argument.
			 */
			token = nextToken(prog, 1);
		}
	}

	return call;
}

int
isOperator(Token *token)
{
	int i;
	for (i = 0; i < NUM_OPERATOR_SYMBOLS; i++) {
		if (match(token, operatorSymbols[i])) {
			return 1;
		}
	}

	return 0;
}

int
getOperatorInfo(Oper *o)
{
	char *p1[] = {"[","]","(",")",".","->",NULL};
	char *p2[] = {"-","!","~","*","&","@","sizeof",NULL};
	char *p3[] = {"*","/","%",NULL};
	char *p4[] = {"+","-",NULL};
	char *p5[] = {"<<",">>",NULL};
	char *p6[] = {"<","<=",">",">=",NULL};
	char *p7[] = {"==","!=",NULL};
	char *p8[] = {"&",NULL};
	char *p9[] = {"^",NULL};
	char *p10[] = {"|",NULL};
	char *p11[] = {"&&",NULL};
	char *p12[] = {"||",NULL};
	char *p13[] = {"=",NULL};

	char **precInfo[] = {
		p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12,p13
	};

	int assocInfo[] = {
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_RIGHT_TO_LEFT,
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_LEFT_TO_RIGHT,
		ASSOC_RIGHT_TO_LEFT
	};

	int i, j;
	for (i = 0; i < sizeof(precInfo) / sizeof(*precInfo); i++) {
		if (i == 1 && o->isBinary) {
			/*
			 * Skip over all unary operators.
			 */
			continue;
		}
		for (j = 0; precInfo[i][j] != NULL; j++) {
			if (match(o->token, precInfo[i][j])) {
				o->precedence = i;
				o->associativity = assocInfo[i];
				return 0;
			}
		}
	}
	return -1;
}

int
parseOperator(Expression *expr, Oper *o, Lifo *lifo, int *prevOper)
{
	o->flags |= EXPR_OPERATOR;
	o->isBinary = *prevOper & EXPR_OPERATOR ? 0 : 1;
	getOperatorInfo(o);
	*prevOper = EXPR_OPERATOR;

	Oper *other;
	if (match(o->token, ")")) {
		/*
		 * Pop entries from the stack until we find a match parenthesis.
		 */
		while ((other = lifoPop(lifo)) != NULL) {
			if (match(other->token, "(")) {
				free(other);
				break;
			}
			listAppend(&expr->postfix, other);
			printf("Append ): %s\n", other->token->string);
		}
		return 0;
	}
	if (match(o->token, "(")) {
		lifoPush(lifo, o);
		return 0;
	}

CHECK_STACK:
	if (lifo->head == NULL) {
		printf("Head NULL\n");
		lifoPush(lifo, o);
		return 0;
	}
	Oper *top = lifoPeek(lifo);
	if (match(top->token, "(")) {
		lifoPush(lifo, o);
		return 0;
	}
	
	if (o->precedence < top->precedence) {
		lifoPush(lifo, o);
		return 0;
	}
	if (o->precedence == top->precedence) {
		if (o->associativity == ASSOC_LEFT_TO_RIGHT) {
			other = lifoPop(lifo);
			listAppend(&expr->postfix, other);
			printf("Append equal: %s\n", other->token->string);
		}
		lifoPush(lifo, o);
		return 0;
	}
	if (o->precedence > top->precedence) {
		other = lifoPop(lifo);
		listAppend(&expr->postfix, other);
		printf("Append lower: %s\n", other->token->string);
		goto CHECK_STACK;
	}

	abort();
}

/*
 * Function call:  <ident> ( [<param>[,<param>[...]]] );
 * operator
 * operand
 */
static Expression *
parseExpression(Program *prog, char *sentinels)
{
	fprintf(stderr, "Parsing expression.\n");

	Token *token;

	Expression *expr = NULL;
	if ((expr = malloc(sizeof(*expr))) == NULL) {
		fatal("Can't allocate Expression object: %s\n", strerror(errno));
	}
	memset(expr, 0, sizeof(*expr));

	Lifo *lifo = lifoAlloc();
	Oper *o = NULL;

	/*
	 * The end of an expression is one of: ;,)
	 */
	int prevOper = EXPR_OPERATOR;
	int parenStack = 0;
	while ((token = nextToken(prog, 0)) != NULL) {
		if (matchChars(token, sentinels) && parenStack <= 0) {
			prog->i--;
			break;
		}

		/*
		 * New oper object.
		 */
		if ((o = malloc(sizeof(*o))) == NULL) {
			fatal("Can't allocate Oper object: %s\n", strerror(errno));
		}
		memset(o, 0, sizeof(*o));
		o->token = token;

	/*
	 * 1.	Print operands as they arrive.
	 *
	 * 2.	If the stack is empty or contains a left parenthesis on top, push
	 * the incoming operator onto the stack.
	 *
	 * 3.	If the incoming symbol is a left parenthesis, push it on the stack.
	 *
	 * 4.	If the incoming symbol is a right parenthesis, pop the stack and
	 * print the operators until you see a left parenthesis. Discard the pair
	 * of parentheses.
	 *
	 * 5.	If the incoming symbol has higher precedence than the top of the
	 * stack, push it on the stack.
	 *
	 * 6.	If the incoming symbol has equal precedence with the top of the
	 * stack, use association. If the association is left to right, pop and
	 * print the top of the stack and then push the incoming operator. If the
	 * association is right to left, push the incoming operator.
	 *
	 * 7.	If the incoming symbol has lower precedence than the symbol on the
	 * top of the stack, pop the stack and print the top operator. Then test
	 * the incoming operator against the new top of stack.
	 *
	 * 8.	At the end of the expression, pop and print all operators on the
	 * stack. (No parentheses should remain.)
	 */
		fprintf(stderr, "%s\n", token->string);
		if (isOperator(token)) {
			parseOperator(expr, o, lifo, &prevOper);
		} else {
			/*
			 * Append operands to the postfix list as they arrive.
			 */
			listAppend(&expr->postfix, o);
			printf("Append rand: %s\n", o->token->string);

			if (o->token->type & T_LITERAL) {
				o->flags = EXPR_LITERAL;
			} else {
				o->flags = EXPR_VAR;
			}

			/*
			 * If the operand is a function name, finish parsing the function
			 * and it's arguments.
			 */
			Token *token = nextToken(prog, 1);
			prog->i--;
			if (match(token, "(")) {
				o->flags = EXPR_FUNC;
				o->call = parseFunctionCall(prog, o->token->string);
			}

			prevOper = o->flags;
		}

		if (match(token, "(")) {
			parenStack++;
		}
		if (match(token, ")")) {
			parenStack--;
		}
	}

	/*
	 * Pop any remaining operators.
	 */
	while ((o = lifoPop(lifo)) != NULL) {
		listAppend(&expr->postfix, o);
		printf("Append end: %s\n", o->token->string);
	}

	lifoFree(&lifo);

	return expr;
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
		var->value = token;
		token = nextToken(prog, 1);
	}
	
	if (!match(token, ";")) {
		error(token, "Expected ;\n");
	}

	return var;
}

static Statement *parseStatement(Program *prog);

static int
parseBlock(Program *prog, Statement *statement, char *name)
{
	fprintf(stderr, "Parsing block.\n");

	Token *token;

	token = nextToken(prog, 1);
	if (!match(token, "{")) {
		error(token, "After %s, expected {\n", name);
	}

	while ((token = nextToken(prog, 1)) != NULL) {
		if (match(token, "}")) {
			break;
		}
		prog->i--;

		Statement *stmt = parseStatement(prog);
		listAppend(&statement->statements, stmt);
	}

	return 0;
}

/*
 * if ( <expression> ) {
 *   <statement>
 *   ...
 * }
 */
static int
parseConditionalBlock(Program *prog, Statement *statement, char *name)
{
	fprintf(stderr, "Parsing conditional block.\n");

	Token *token;

	token = nextToken(prog, 1);
	if (!match(token, "(")) {
		error(token, "After %s, expected (\n", name);
	}

	statement->expr = parseExpression(prog, ")");

	token = nextToken(prog, 1);
	if (!match(token, ")")) {
		error(token, "After %s, expected )\n", name);
	}

	return parseBlock(prog, statement, name);
}

/*
 * Variable declaration:
 * <type> <ident> [ = <expression> ];
 *
 * Control flow:
 * if ( <expression> ) {
 *   <statement>
 * }
 *
 * Loop:
 * while ( <expression> ) {
 *   <statement>
 * }
 *
 * Return statement:
 * return [<expression>];
 *
 * Generic expression that results in some value:
 * <expression>
 */
static Statement *
parseStatement(Program *prog)
{
	fprintf(stderr, "Parsing statement.\n");

	Token *token;

	Statement *statement = NULL;
	if ((statement = malloc(sizeof(*statement))) == NULL) {
		fatal("Can't allocate Statement object: %s\n", strerror(errno));
	}
	memset(statement, 0, sizeof(*statement));

	token = nextToken(prog, 1);
	if (match(token, "if")) {
		statement->flags |= STMT_IF;
		parseConditionalBlock(prog, statement, "if");
	} else if (match(token, "elif")) {
		statement->flags |= STMT_ELIF;
		parseConditionalBlock(prog, statement, "elif");
	} else if (match(token, "else")) {
		statement->flags |= STMT_ELSE;
		parseBlock(prog, statement, "else");
	} else if (match(token, "while")) {
		statement->flags |= STMT_WHILE;
		parseConditionalBlock(prog, statement, "while");
	} else if (match(token, "return")) {
		statement->flags |= STMT_RETURN;
		statement->expr = parseExpression(prog, ";");
		token = nextToken(prog, 1);
		if (!match(token, ";")) {
			error(token, "After expression, expected ;\n");
		}
	} else if (isType(prog, token)) {
		statement->flags |= STMT_VAR_DECL;
		prog->i--;
		statement->var = parseVariable(prog);
	} else {
		prog->i--;
		statement->flags |= STMT_EXPR;
		statement->expr = parseExpression(prog, ";");

		token = nextToken(prog, 1);
		if (!match(token, ";")) {
			error(token, "After expression, expected ;\n");
		}
	}

	return statement;
}

/*
 * fn <ident> ( [ <type> <ident> [ , <type> <ident> ] ] ) [ <type> ] { }
 *
 * fn foo(u8 a, u32 b, ptr c) void * { }
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

	Function *fn = NULL;
	if ((fn = malloc(sizeof(*fn))) == NULL) {
		fatal("Can't allocate Function object: %s\n", strerror(errno));
	}
	memset(fn, 0, sizeof(*fn));

	token = nextToken(prog, 1);
	if (token->type != T_IDENTIFIER) {
		error(token, "Expected function name.\n");
	}
	fn->name = token->string;

	token = nextToken(prog, 1);
	if (!match(token, "(")) {
		error(token, "Expected (\n");
	}

	token = nextToken(prog, 1);
	while (!match(token, ")")) {
		prog->i--;
		Parameter *param = parseParameter(prog, NULL);
		listAppend(&fn->params, param);

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
		parseType(prog, &fn->type, 0);
	}

	token = nextToken(prog, 1);
	if (!match(token, "{")) {
		error(token, "After function header, expected {\n");
	}

	while ((token = nextToken(prog, 0)) != NULL) {
		if (match(token, "}")) {
			break;
		}
		prog->i--;

		Statement *statement = parseStatement(prog);
		listAppend(&fn->statements, statement);
	}

	return fn;
}

static Variable *
parseGlobalVariable(Program *prog)
{
	fprintf(stderr, "Parsing global variable.\n");

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
		var->value = token;
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
		Parameter *param = parseParameter(prog, NULL);
		listAppend(&s->members, param);

		s->size += param->type.size;

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
		 *    fn <type> <ident> ( [ <type> <name> [ , <type> <name> ] ] ) <type> { }
		 *  Struct definition
		 *    struct <ident> {
		 *      <type> <ident>,
		 *      <type> <ident>
		 *    };
		 *  Variable declaration (initialize to constant)
		 *    <type> <ident> [ = <literal> ];
		 */

		if (match(token, "fn")) {
			Function *fn = parseFunction(prog);
			listAppend(&prog->functions, fn);
		}
		else if (match(token, "struct")) {
			Struct *s = parseStruct(prog);
			listAppend(&prog->structs, s);
		}
		else if (isType(prog, token)) {
			prog->i--;
			Variable *var = parseGlobalVariable(prog);
			listAppend(&prog->vars, var);
		} else {
			error(token, "Only 'fn', 'struct' and variables are allowed in global scope.\n");
		}
	}

	return 0;
}

static int
escapeSequence(char c)
{
	if (c == 'n') {
		return '\n';
	}
	if (c == 't') {
		return '\t';
	}
	if (c == '"') {
		return '"';
	}
	if (c == '\'') {
		return '\'';
	}
	if (c == '\\') {
		return '\\';
	}

	fatal("Invalid escape sequence?!\n");
	return -1;
}

static uint32_t nextStrLabelNum = 0;

static int
genLiteralStr(Program *prog, Token *token)
{
	int i;

	if (token->type != T_LITERAL_STR) {
		return 0;
	}

	token->id = nextStrLabelNum;
	nextStrLabelNum++;

	fprintf(prog->outFP, ".str_%" PRIu32 "\n", token->id);

	for (i = 0; i < strlen(token->string); i++) {
		int c = token->string[i];
		if (c == '"') {
			continue;
		}
		if (c == '\\') {
			/*
			 * Escape sequence.
			 */
			i++;
			c = token->string[i];
			fprintf(prog->outFP, "b %d\n", escapeSequence(c));
		}
		fprintf(prog->outFP, "b %d\n", c);
	}

	return 0;
}

static int
literalCharToNum(Program *prog, char *string)
{
	if (string[1] == '\\') {
		/*
		 * Escape sequence.
		 */
		return escapeSequence(string[2]);
	}
	return string[1];
}

static int
genVariable(Program *prog, Variable *var)
{
	int i;

	/*
	 * Throw down a label so others can easily reference the variable.
	 */
	fprintf(prog->outFP, ".v_%s\n", var->name);

	/*
	 * struct	can't be initialized
	 * u8		hex|dec|char
	 * u32		hex|dec|char
	 * ptr		hex|dec|char|string
	 */

	if (var->value) {
		if (var->type.flags & TYPE_STRUCT) {
			error(var->value, "Structs can't be initialized!\n");
		}
		if (var->value->type == T_LITERAL_STR) {
			if ((var->type.flags & TYPE_PTR) == 0) {
				error(var->value, "Only pointers can be assigned to a string literal!\n");
			}
			fprintf(prog->outFP, "w .str_%" PRIu32 "\n", var->value->id);
		}
		if (var->value->type & (T_LITERAL_HEX | T_LITERAL_DEC)) {
			if (var->type.flags & TYPE_U8) {
				fprintf(prog->outFP, "b %s\n", var->value->string);
			} else if (var->type.flags & (TYPE_U32 | TYPE_PTR)) {
				fprintf(prog->outFP, "w %s\n", var->value->string);
			}
		}
		if (var->value->type & T_LITERAL_CHR) {
			if (var->type.flags & TYPE_U8) {
				fprintf(prog->outFP, "b %d\n", literalCharToNum(prog, var->value->string));
			} else if (var->type.flags & (TYPE_U32 | TYPE_PTR)) {
				fprintf(prog->outFP, "w %d\n", literalCharToNum(prog, var->value->string));
			}
		}
	} else {
		/*
		 * By default, global variables are initialized to zero.
		 */
		for (i = 0; i < var->type.size / 4; i++)
			fprintf(prog->outFP, "w 0\n");
		for (i = 0; i < var->type.size % 4; i++)
			fprintf(prog->outFP, "b 0\n");
	}
	
	return 0;
}

static int
genExpression(Expression *expr)
{
	Lifo *lifo = lifoAlloc();

	/*
	 * Loop over the expression in postfix order.
	 */
	ListEntry *entry = NULL;
	while (listNext(&expr->postfix, &entry) != NULL) {
		Oper *o = entry->data;
		if (o->flags & EXPR_OPERAND) {
			/*
			 * We push operands onto the stack until we find an operator.
			 */
			lifoPush(lifo, o);
			continue;
		}

		/*
		 * We have an operator.
		 */
		Oper *operand[2];
		operand[0] = lifoPop(lifo);
		if (o->isBinary) {
			operand[1] = lifoPop(lifo);
		}

		/*
		 * Perform the operation.
		 */
		if (match(o->token, "+")) {
			printf("%s + %s",
					operand[0]->token->string,
					operand[1]->token->string);
		}

		/*
		 * Push result back on the stack.
		 */
	}

	lifoFree(&lifo);

	return 0;
}

static int
genStatement(Program *prog, Function *f, Statement *s)
{
	if (s->flags & STMT_IF) {
	} else if (s->flags & STMT_ELIF) {
	} else if (s->flags & STMT_ELSE) {
	} else if (s->flags & STMT_WHILE) {
	} else if (s->flags & STMT_RETURN) {
	} else if (s->flags & STMT_VAR_DECL) {
	} else if (s->flags & STMT_EXPR) {
		genExpression(s->expr);
	}
	return 0;
}

static int
genFunction(Program *prog, Function *f)
{
	/*
	 * Throw down a label so others can easily call the function.
	 */
	fprintf(prog->outFP, ".fn_%s\n", f->name);

	/*
	 * Push any register this function will use onto the stack.
	 */
	ListEntry *entry = NULL;
	while (listNext(&f->statements, &entry) != NULL) {
		Statement *s = entry->data;
		genStatement(prog, f, s);
	}
	//fprintf(prog->outFP, "");

	return 0;
}

static int
generator(Program *prog)
{
	ListEntry *entry = NULL;

	if ((prog->outFP = fopen(outFile, "w")) == NULL) {
		fatal("Can't open %s: %s\n", outFile, strerror(errno));
	}

	fprintf(prog->outFP, "jmp .__main\n");

	/*
	 * Generate code for all string literals.
	 */
	prog->i = -1;
	Token *token;
	while ((token = nextToken(prog, 0)) != NULL) {
		if (token->type == T_LITERAL_STR) {
			genLiteralStr(prog, token);
		}
	}

	/*
	 * Generate code for all global variables.
	 */
	while (listNext(&prog->vars, &entry) != NULL) {
		Variable *var = entry->data;
		genVariable(prog, var);
	}

	/*
	 * Generate code for all functions.
	 */
	entry = NULL;
	while (listNext(&prog->functions, &entry) != NULL) {
		Function *f = entry->data;
		genFunction(prog, f);
	}
	return 0;
}

int
main(int argc, char **argv)
{
	Program prog;
	memset(&prog, 0, sizeof(prog));

	mainArgs(argc, argv);

	int i;
	printf("Operator Symbols %lu:\n", NUM_OPERATOR_SYMBOLS);
	for (i = 0; i < NUM_OPERATOR_SYMBOLS; i++) {
		printf("\"%s\",", operatorSymbols[i]);
	}
	printf("\n");

	printf("Symbols %lu:\n", NUM_SYMBOLS);
	for (i = 0; i < NUM_SYMBOLS; i++) {
		printf("\"%s\",", symbols[i]);
	}
	printf("\n");

	lexer(inFile, &prog);
	parser(&prog);
	print(&prog);
	generator(&prog);

	return 0;
}
