#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#define STATE_NAME         0
#define STATE_NAME_DELIM   1
#define STATE_SYMBOL       2
#define STATE_SYMBOL_DELIM 3
#define STATE_DONE         4

static char *file;
static char *grammarFile = "grammar";
static int dumpParseTree;

static struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"parse-tree", no_argument, NULL, 'p'},
	{"grammar-file", required_argument, NULL, 'g'}
};

static char *optdesc[] = {
	"This help.",
	"Dump parse tree.",
	"Grammar file to use (Default: 'grammar')"
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
parseArgs(int argc, char **argv)
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
			case 'g':
				grammarFile = strdup(optarg);
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
		fprintf(stderr, "Expected a source file path.\n");
		exit(1);
	}
}

#if 0
struct Array {
	void *elements;
	int numElements;
};

static int
addElement(struct Array *array, void *element, void *elementSize)
{
	array->numElements++;
	element = realloc(array->elements,
					   sizeof(*array->elements) * array->numElements);
	if (element == NULL) {
		fprintf(stderr, "Can't allocate a new element: %s\n", strerror(errno));
		return -1;
	}
	array->elements = element;
	element = &array->elements[array->numElements - 1];
	memset(element, 0, sizeof(*element));
}

static int
getElement(struct Array *array, int element)
{

}
#endif

#define SYMBOL_TERMINAL   0
#define SYMBOL_REFERENCE  1
#define SYMBOL_LITERAL    2
#define SYMBOL_IDENTIFIER 3

struct Symbol {
	/*
	 * Types:
	 *   0. char *        (Terminal Symbol)
	 *   1. struct Rule * (Non-terminal Symbol)
	 */
	char *token;
	void *value;
	int type;
};

struct SymbolString {
	struct Symbol *symbols;
	int numSymbols;
};

struct Rule {
	char *name;

	/*
	 * Array of symbol strings.
	 */
	struct SymbolString *strings;
	int numStrings;
};

struct Grammar {
	/*
	 * For now, we will just use an array of rules.
	 */
	struct Rule *rules;
	int numRules;

	/*
	 * Array of all terminal symbols for tokenizing.
	 */
	char **terminals;
	int numTerminals;
};

static void
dumpRule(struct Rule *rule)
{
	int i, j;

	if ((rule == NULL) || (rule == (void *)-1)) {
		return;
	}

	fprintf(stderr, "%s : ", rule->name);
	for (i = 0; i < rule->numStrings; i++) {
		struct SymbolString *string = &rule->strings[i];

		if (i > 0) {
			fprintf(stderr, " | ");
		}
		for (j = 0; j < string->numSymbols; j++) {
			struct Symbol *symbol = &string->symbols[j];

			if (j > 0) {
				fprintf(stderr, " ");
			}
			if (symbol->type == SYMBOL_TERMINAL) {
				fprintf(stderr, "'%s'", symbol->token);
			} else {
				fprintf(stderr, "%s", symbol->token);
			}
		}
	}
	fprintf(stderr, ";\n");
}

struct Stack {
	char **data;
	int count;
};

static struct Stack *
stackInit()
{
	struct Stack *stack;

	if ((stack = malloc(sizeof(*stack))) == NULL) {
		fprintf(stderr, "Failed to allocate stack: %s\n", strerror(errno));
	}
	memset(stack, 0, sizeof(*stack));

	return stack;
}

/*
 * -1 An error occurred.
 *  0 Successful.
 */
static int
stackPush(struct Stack *stack, char *token)
{
	if (stack == NULL) {
		fprintf(stderr, "Invalid parameter.\n");
		return -1;
	}

	stack->count++;
	stack->data = realloc(stack->data, sizeof(*stack->data) * stack->count);
	if (stack->data == NULL) {
		fprintf(stderr, "Can't grow stack: %s\n", strerror(errno));
		return -1;
	}

	stack->data[stack->count-1] = strdup(token);

	return 0;
}

/*
 * Caller must free the token that is returned.
 * NULL An error occurred.
 */
static char *
stackPop(struct Stack *stack)
{
	char *token;

	if (stack == NULL) {
		fprintf(stderr, "Invalid parameter.\n");
		return NULL;
	}

	token = stack->data[stack->count-1];

	stack->count--;
	if (stack->count > 0) {
		stack->data = realloc(stack->data, sizeof(*stack->data) * stack->count);
		if (stack->data == NULL) {
			fprintf(stderr, "Can't shrink stack: %s\n", strerror(errno));
			return NULL;
		}
	} else {
		free(stack->data);
		stack->data = NULL;
	}

	return token;
}

/*
 *  0 Found in stack.
 *  1 Not found in the stack.
 */
static int
findInStack(struct Stack *stack, char *token)
{
	int i;

	for (i = 0; i < stack->count; i++) {
		if (strcmp(stack->data[i], token) == 0) {
			/*
			 * Found a match.
			 */
			return 0;
		}
	}
	return 1;
}

/*
 * Delimiters   | ;
 * Whitespace
 * 'string'
 * # comments
 */

static int
readGrammarToken(FILE *fp, char *token, int maxTokenLen)
{
	int c;
	int len = 0;
	int readingString = 0;

	memset(token, 0, maxTokenLen);

	while (((c = fgetc(fp)) != EOF) && (ferror(fp) == 0)) {
		if (readingString == 0) {
			if (c == '\'') {
				/*
				 * Found a single quote, so this is the beginning of a string.
				 */
				readingString = 1;
			}
			if ((c == ':') || (c == ';')) {
				if (len <= 0) {
					/*
					 * The token is currently empty, so this char is the token.
					 */
					token[len] = c;
					len++;
					break;
				} else {
					/*
					 * Found a delimiter, but it's a special one.
					 * Seek back and break.
					 */
					fseek(fp, -1, SEEK_CUR);
					break;
				}
			}
			if ((c == ' ') || (c == '\n') || (c == '\t')) {
				if (len <= 0) {
					/*
					 * We only break on whitespace once we have a token.
					 */
					continue;
				} else {
					/*
					 * We have a token, so break on whitespace.
					 */
					break;
				}
			}
			if (c == '#') {
				while ((c = fgetc(fp)) != EOF) {
					/*
					 * Skip over the comment.
					 */
					if (c == '\n') {
						break;
					}
				}
				if (ferror(fp) != 0) {
					break;
				}
				if (len <= 0) {
					continue;
				}
				break;
			}
		} else {
			if (c == '\'') {
				/*
				 * Found the end of a string.
				 */
				readingString = 0;
				token[len] = c;
				len++;
				break;
			}
		}

		token[len] = c;
		len++;
	}

	if (ferror(fp) != 0) {
		fprintf(stderr, "Error: Failed to read character?!\n");
		return -1;
	}

	return len;
}

static void
freeRule(struct Rule *rule)
{
	return;
}

static int
indexTerminalSymbol(struct Grammar *grammar, struct Symbol *symbol)
{
	char **terminal;
	int i;

	for (i = 0; i < grammar->numTerminals; i++) {
		if (strcmp(grammar->terminals[i], symbol->token) == 0) {
			/*
			 * This terminal symbol has already been indexed.
			 */
			return 0;
		}
	}

	/*
	 * Increase the terminal array size.
	 */
	grammar->numTerminals++;
	terminal = realloc(grammar->terminals,
					   sizeof(*grammar->terminals) * grammar->numTerminals);
	if (terminal == NULL) {
		fprintf(stderr, "Can't allocate a new terminal: %s\n", strerror(errno));
		return -1;
	}
	grammar->terminals = terminal;
	terminal = &grammar->terminals[grammar->numTerminals - 1];
	memset(terminal, 0, sizeof(*terminal));

	*terminal = symbol->token;

	fprintf(stderr, "Indexed: %s\n", *terminal);

	return 0;
}

static int
addSymbol(struct Grammar *grammar, struct SymbolString *string, char *token)
{
	struct Symbol *symbol;

	/*
	 * Increase the symbol array size.
	 */
	string->numSymbols++;
	symbol = realloc(string->symbols,
					 sizeof(*string->symbols) * string->numSymbols);
	if (symbol == NULL) {
		fprintf(stderr, "Can't allocate a new symbol: %s\n", strerror(errno));
		return -1;
	}
	string->symbols = symbol;
	symbol = &string->symbols[string->numSymbols - 1];
	memset(symbol, 0, sizeof(*symbol));

	symbol->token = strdup(token);

	if ((token[0] == '\'') &&
		(token[strlen(token) - 1] == '\'')) {
		/*
		 * This is a terminal symbol.
		 */
		symbol->type = SYMBOL_TERMINAL;

		/*
		 * Remove quotes.
		 */
		int i;
		token[strlen(token) - 1] = '\0';
		for (i = 0; token[i] != '\0'; i++) {
			token[i] = token[i + 1];
		}
		free(symbol->token);
		symbol->token = strdup(token);

		if (indexTerminalSymbol(grammar, symbol) < 0) {
			fprintf(stderr, "Error: Can't index terminal symbol: %s\n", symbol->token);
			free(symbol);
			return -1;
		}
	} else if (strcmp(token, "IDENTIFIER") == 0) {
		symbol->type = SYMBOL_IDENTIFIER;
	} else if (strcmp(token, "LITERAL") == 0) {
		symbol->type = SYMBOL_LITERAL;
	} else {
		/*
		 * This is a non-terminal symbol.
		 */
		symbol->type = SYMBOL_REFERENCE;
	}

	return 0;
}

static struct Rule *
readRule(FILE *fp, struct Grammar *grammar, struct Rule *rule)
{
	char token[4096];
	int len;
	struct SymbolString *string = NULL;
	int state;

	if (rule == NULL) {
		fprintf(stderr, "Error: Rule struct is NULL.\n");
		return (void *)-1;
	}
	memset(rule, 0, sizeof(*rule));

	/*
	 * Simple state machine to parse rules.
	 */
	state = STATE_NAME;
	while ((len = readGrammarToken(fp, token, sizeof(token))) > 0) {
		//fprintf(stderr, "Token: %s\n", token);
		if (state == STATE_NAME) {
			if ((strcmp(token, "|") == 0) ||
				(strcmp(token, ";") == 0)) {
				fprintf(stderr, "Expected rule name but read '%s'\n", token);
				break;
			}
			rule->name = strdup(token);
			state = STATE_NAME_DELIM;
			continue;
		}
		if (state == STATE_NAME_DELIM) {
			if (strcmp(token, ":") != 0) {
				fprintf(stderr, "Expected rule delimiter but read '%s'\n", token);
				break;
			}
			state = STATE_SYMBOL;
			continue;
		}
		if (state == STATE_SYMBOL) {
			if (strcmp(token, ";") == 0) {
				if (string == NULL) {
					fprintf(stderr, "Expected symbol string before ';'\n");
					break;
				}
				/*
				 * Found end of rule.
				 */
				state = STATE_NAME;
				break;
			}

			if (strcmp(token, "|") == 0) {
				/*
				 * Delimiter separating two strings of symbols.
				 * Start a new SymbolString.
				 */
				if (string != NULL) {
					string = NULL;
				}
				continue;
			}

			if (string == NULL) {
				/*
				 * Allocate a new SymbolString.
				 */
				rule->numStrings++;
				string = realloc(rule->strings,
								 sizeof(*rule->strings) * rule->numStrings);
				if (string == NULL) {
					fprintf(stderr, "Can't allocate a new SymbolString: %s\n", strerror(errno));
					break;
				}
				rule->strings = string;
				string = &rule->strings[rule->numStrings - 1];
				memset(string, 0, sizeof(*string));
			}

			if (addSymbol(grammar, string, token) < 0) {
				fprintf(stderr, "Can't add symbol '%s' to symbol string.\n", token);
				break;
			}
			continue;
		}

		fprintf(stderr, "Unknown state?!\n");
	}

	if (state != STATE_NAME) {
		fprintf(stderr, "Failed to read rule.\n");
		freeRule(rule);
		return (void *)-1;
	}

	if (len <= 0) {
		return NULL;
	}

	return rule;
}

static struct Rule *
findRuleByName(struct Grammar *grammar, char *name)
{
	struct Rule *rule;
	int i;

	for (i = 0; i < grammar->numRules; i++) {
		rule = &grammar->rules[i];
		if (strcmp(rule->name, name) == 0) {
			/*
			 * Found a match.
			 */
			return rule;
		}
	}

	return NULL;
}

static struct Grammar *
loadGrammar(char *path)
{
	FILE	*fp;
	struct Grammar *grammar;
	struct Rule *rule;

	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Can't open %s: %s\n", path, strerror(errno));
		return NULL;
	}

	if ((grammar = malloc(sizeof(*grammar))) == NULL) {
		fprintf(stderr, "Can't allocate new grammar: %s\n", strerror(errno));
		fclose(fp);
		return NULL;
	}
	memset(grammar, 0, sizeof(*grammar));

	while (1) {
		/*
		 * Increase the rule array size.
		 */
		grammar->numRules++;
		rule = realloc(grammar->rules,
					   sizeof(*grammar->rules) * grammar->numRules);
		if (rule == NULL) {
			fprintf(stderr, "Can't allocate a new rule: %s\n", strerror(errno));
			return NULL;
		}
		grammar->rules = rule;
		rule = &grammar->rules[grammar->numRules - 1];
		memset(rule, 0, sizeof(*rule));

		rule = readRule(fp, grammar, rule);
		if ((rule == NULL) || (rule == (void *)-1)) {
			grammar->numRules--;
			break;
		}

		//dumpRule(rule);
	}

	/*
	 * Link all rule references.
	 */
	int i, j, k;
	for (i = 0; i < grammar->numRules; i++) {
		rule = &grammar->rules[i];
		dumpRule(rule);
		for (j = 0; j < rule->numStrings; j++) {
			struct SymbolString *string = &rule->strings[j];
			for (k = 0; k < string->numSymbols; k++) {
				struct Symbol *symbol = &string->symbols[k];
				if (symbol->type == SYMBOL_REFERENCE) {
					struct Rule *ref = findRuleByName(grammar, symbol->token);
					if (ref == NULL) {
						fprintf(stderr, "Error: Reference to undefined rule %s\n", symbol->token);
						return NULL;
					}
					symbol->value = ref;
				}
			}
		}
	}

	fclose(fp);
	return grammar;
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

/*
 * 0  Not a literal.
 * 1  A literal.
 * 2  An incomplete literal.
 */
static int
isLiteral(char *token)
{
	int len = strlen(token);
	int ret = 0;

	if (len == 0) {
		return 0;
	}

	/*
	 * Numeric literal.
	 */
	if ((ret = isLiteralHex(token)) != 0) {
		return ret;
	}
	if (isLiteralDec(token) != 0) {
		return 1;
	}

#if 0
	/*
	 * String literal.
	 */
	if (token[0] == '"') {
		for (i = 0; token[i] != '\0'; i++) {
			if (token[i] == '"') {
				count++;
			}
		}
		if (count == 1) {
			return 2;
		}
		if (count == 2) {
			return 1;
		}
		return 0;
	}

	/*
	 * Character literal.
	 */
	if (token[0] == '\'') {
		for (i = 0; token[i] != '\0'; i++) {
			
		}
	}
#endif

	return 0;
}

static int
readSourceUntil(FILE *fp, char *sentinel)
{
	int c, i;
	char token[4096];
	int len = 0;
	int sentinelLen = strlen(sentinel);

	memset(token, 0, sizeof(token));
	while (((c = fgetc(fp)) != EOF) && (ferror(fp) == 0)) {
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
static char *
readSourceToken(FILE *fp, struct Grammar *grammar)
{
	int c, i;
	char token[4096];
	int len = 0;
	int partialMatch = 0;

	memset(token, 0, sizeof(token));

	while (((c = fgetc(fp)) != EOF) && (ferror(fp) == 0)) {
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
		 * Add the char to the token before we know if it will match.
		 * If it causes a mismatch, we will remove it.
		 */
		token[len] = c;
		len++;

		if (strcmp(token, "/*") == 0) {
			/*
			 * Skip over multiline comment.
			 */
			if (readSourceUntil(fp, "*/") < 0) {
				fprintf(stderr, "Error: Failed to read until '*/'\n");
				return NULL;
			}
			len = 0;
			memset(token, 0, sizeof(token));
			continue;
		}
		if (strcmp(token, "//") == 0) {
			/*
			 * Skip over single line comment.
			 */
			if (readSourceUntil(fp, "\n") < 0) {
				fprintf(stderr, "Error: Failed to read until '\\n'\n");
				return NULL;
			}
			len = 0;
			memset(token, 0, sizeof(token));
			continue;
		}

		/*
		 * Find longest matching terminal.
		 */
		for (i = 0; i < grammar->numTerminals; i++) {
			if (strcmp(grammar->terminals[i], token) == 0) {
				//fprintf(stderr, "Match: (%s) -> %s\n", grammar->terminals[i], token);
				break;
			}
		}
		if (i < grammar->numTerminals) {
			continue;
		}

		if (isIdentifier(token) != 0) {
			//fprintf(stderr, "Match ident: %s\n", token);
			continue;
		}

		i = isLiteral(token);
		if (i != 0) {
			partialMatch = 0;
			if (i == 2) {
				partialMatch = 1;
			}
			continue;
		}

		/*
		 * We didn't find a match. Remove the last char.
		 * Seek back and break.
		 */
		fseek(fp, -1, SEEK_CUR);
		len--;
		token[len] = '\0';
		break;
	}

	if (partialMatch != 0) {
		fprintf(stderr, "Error: Incomplete match: %s\n", token);
		return (char *)-1;
	}
	if (ferror(fp) != 0) {
		fprintf(stderr, "File error.\n");
		return (char *)-1;
	}
	if (len == 0) {
		if (feof(fp) != 0) {
			return NULL;
		}
		fprintf(stderr, "Error: Failed to match next token.\n");
		return (char *)-1;
	}

	return strdup(token);
}

/*
 * -1 An error occurred.
 *  0 Symbol matches token.
 *  1 Symbol doesn't match token.
 */
static int
matchSymbol(struct Symbol *symbol, char *token)
{
	if (symbol->type == SYMBOL_TERMINAL) {
		if (strcmp(token, symbol->token) == 0) {
			fprintf(stderr, "T:%s\n", token);
			return 0;
		}
	} else if (symbol->type == SYMBOL_REFERENCE) {
		;
	} else if (symbol->type == SYMBOL_LITERAL) {
		if (isLiteral(token) == 1) {
			fprintf(stderr, "L:%s\n", token);
			return 0;
		}
	} else if (symbol->type == SYMBOL_IDENTIFIER) {
		if (isIdentifier(token) == 1) {
			fprintf(stderr, "I:%s\n", token);
			return 0;
		}
	} else {
		fprintf(stderr, "Bad symbol type?!\n");
		return -1;
	}

	return 1;
}

/*
 * -1 Encountered an error.
 *  0 Rule matches.
 *  1 Rule does not match.
 */
static int
matchRule(struct Grammar *grammar, struct Rule *rule, struct Stack *stack, char ***tokenArray, int tokenCount)
{
	int match;

	if (findInStack(stack, rule->name) == 0) {
		/*
		 * This rule is already in the rule stack. That means we are
		 * infinitely recursing without getting closer to matching a
		 * full rule.
		 */
		return 1;
		
	}
	stackPush(stack, rule->name);

	dumpRule(rule);

	int j, k;
	for (j = 0; j < rule->numStrings; j++) {
		struct SymbolString *string = &rule->strings[j];
		char **tokens = *tokenArray;
		int tokensLeft = tokenCount;

		/*
		 * Try to match each Symbol within the SymbolString.
		 */
		for (k = 0; k < string->numSymbols; k++) {
			struct Symbol *symbol = &string->symbols[k];

			fprintf(stderr, "Matching token: %s\n", *tokens);

			if (symbol->type == SYMBOL_REFERENCE) {
				struct Rule *ref;
				
				ref = findRuleByName(grammar, symbol->token);
				match = matchRule(grammar, ref, stack, &tokens, tokensLeft);
				if (match != 0) {
					break;
				}

				/*
				 * We don't need to advance the tokens array.
				 * The matchRule() call did that for us.
				 */
				continue;
			}

			/*
			 * Does symbol match?
			 */
			match = matchSymbol(symbol, *tokens);
			if (match != 0) {
				break;
			}

			tokensLeft--;
			tokens++;
		}

		if (match < 0) {
			stackPop(stack);
			return -1;
		} else if (match == 0) {
			/*
			 * Symbol string matches.
			 */
			fprintf(stderr, "Full symbol string matches!\n");
			*tokenArray = tokens;
			stackPop(stack);
			return 0;
		}
	}

	stackPop(stack);
	return 1;
}

static int
printSourceToken(char *token)
{
	/*
	 * Pretty print.
	 */
	fprintf(stderr, "%s", token);
	if (!strcmp(token, ";") ||
		!strcmp(token, "{") ||
		!strcmp(token, "}")) {
		fprintf(stderr, "\n");
	} else {
		fprintf(stderr, " ");
	}

	return 0;
}

static int
parseSourceFile(char *path, struct Grammar *grammar)
{
	FILE *fp;
	char *token;
	char **tokenArray = NULL;
	int tokenCount = 0;

	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Can't open %s: %s\n", path, strerror(errno));
		return -1;
	}


	while (((token = readSourceToken(fp, grammar)) != NULL) &&
		   (token != (char *)-1)) {

		printSourceToken(token);

		/*
		 * Increase the token array size.
		 */
		tokenCount++;
		tokenArray = realloc(tokenArray, sizeof(*tokenArray) * tokenCount);
		if (tokenArray == NULL) {
			fprintf(stderr, "Can't grow tokenArray: %s\n", strerror(errno));
			return -1;
		}

		tokenArray[tokenCount-1] = token;
	}

	fprintf(stderr, "Parsed %d tokens.\n", tokenCount);
	int i;
	for (i = 0; i < tokenCount; i++) {
		printSourceToken(tokenArray[i]);
	}

	struct Stack *stack;

	if ((stack = stackInit()) == NULL) {
		return -1;
	}

	struct Rule *rule;

	rule = findRuleByName(grammar, "stmt");
	matchRule(grammar, rule, stack, &tokenArray, tokenCount);

	return 0;
}

int main(int argc, char **argv)
{
	struct Grammar *grammar;

	parseArgs(argc, argv);

	grammar = loadGrammar(grammarFile);
	parseSourceFile(file, grammar);

	return 0;
}
