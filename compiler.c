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

typedef struct node {
	char *token;

	struct node *child;
	uint64_t childCount;
} node;

static char *file;
static char *grammarFile = "grammar";
static int dumpParseTree;

static struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"parse-tree", no_argument, NULL, 'p'}
};

static char *optdesc[] = {
	"This help.",
	"Dump parse tree."
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
			case 'p':
				dumpParseTree = 1;
				break;
			case 'h':
				usage(argc, argv);
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
		fprintf(stderr, "Expected a source file path.\n");
		exit(1);
	}
}

int readLine(FILE *fp)
{
	return 0;
}

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
	 * For now, we will just use a linked list of rules.
	 */
	struct Rule *rules;
	int numRules;
};

struct Grammar *gGrammar;

static void
dumpRule(struct Rule *rule)
{
	int i, j;

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
			fprintf(stderr, "%s", symbol->token);
		}
	}
	fprintf(stderr, ";\n");
}

#if 0
/*
 * 
 */
static int
readToken(FILE *fp, char *token, int maxTokenLen)
{
	static char delim[] = " :;";
	int tokenLen = 0;

	memset(token, 0, maxTokenLen);

	while ((c = fgetc(fp)) != EOF) {
		if (tokenLen <= 0) {
			if (c == " ") {
				/*
				 * We only break on spaces once we have a token.
				 */
				continue;
			}
			if (c == ';' || c == '|') {
				/*
				 * The token is currently empty, so this must be the token.
				 */
				token[len] = c;
				len++;
				break;
			}
		}

		if (c == " ") {
			/*
			 * Found a delimiter.
			 */
			break;
		}

		if ((c == ':') || (c == '|')) {
			/*
			 * Found a delimiter, but it's a special one.
			 * Seek back and break.
			 */
			fseek(fp, -1, SEEK_CUR);
			break;
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
#endif

static void
freeRule(struct Rule *rule)
{
	return;
}

static int
addSymbol(struct SymbolString *string, char *token)
{
	struct Symbol *symbol;

	/*
	 * Allocate a new SymbolString.
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
		(token[strlen(token)] == '\'')) {
		/*
		 * This is a terminal symbol.
		 */
		symbol->type = 0;
		symbol->value = strdup(token);
	} else {
		/*
		 * This is a non-terminal symbol.
		 * TODO: Lookup reference.
		 */
		symbol->type = 1;
		symbol->value = NULL;
	}

	//TODO: Special terminal symbols (IDENTIFIER, LITERAL)

	return 0;
}

static struct Rule *
readRule(FILE *fp)
{
	char token[4096];
	int len;
	struct Rule *rule;
	struct SymbolString *string = NULL;
	int state;

	if ((rule = malloc(sizeof(*rule))) == NULL) {
		fprintf(stderr, "Can't allocate new rule: %s\n", strerror(errno));
		return (void *)-1;
	}
	memset(rule, 0, sizeof(*rule));

	//TODO: Skip comment lines.

	/*
	 * Simple state machine to parse rules.
	 */
	state = STATE_NAME;
	while ((len = fscanf(fp, "%s", token)) > 0) {
		fprintf(stderr, "Token: %s\n", token);
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
				state = STATE_DONE;
				break;
			}
			if (strcmp(token, "|") == 0) {
				if (string == NULL) {
					fprintf(stderr, "Expected symbol string before '|'\n");
					break;
				}
				string = NULL;
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

			if (addSymbol(string, token) < 0) {
				fprintf(stderr, "Can't add symbol '%s' to symbol string.\n", token);
				break;
			}
			continue;
		}

		fprintf(stderr, "Unknown state?!\n");
	}

	if (state != STATE_DONE) {
		fprintf(stderr, "Failed to read rule.\n");
		freeRule(rule);
		return (void *)-1;
	}

	return rule;
}

static int
loadGrammar(char *path)
{
	FILE	*fp;
	struct Rule *rule;

	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Can't open %s: %s\n", path, strerror(errno));
		return -1;
	}

	while (((rule = readRule(fp)) != NULL) && (rule != (void *)-1)) {
		dumpRule(rule);
	}

	fclose(fp);
	return 0;
}

int main(int argc, char **argv)
{
	parseArgs(argc, argv);

	loadGrammar(grammarFile);
	//parseSourceFile(file);

	return 0;
}
