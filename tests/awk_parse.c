/* Parse an awk program with the native front end and print its tree.
 * awk-parse 'program' | awk-parse -f file
 * Linked with every awk object except main.o. */
#include <stdio.h>
#include "awk.def"
#include "awk.h"
extern FILE *yyin;
char *lexprog;
extern int errorflag;
int dbg, svargc;
yywrap() { return(1); }
char **svargv, **xargv;
struct fa;
int match();
static char *probes[] = { "", "a", "abc", "x1", "hello world", "\t", "b+c", "Z9.", "ax", "ab", "a.c", "Hello", 0 };

extern cell fldtab[];
static void strout(char *p)
{
	putchar('"');
	for (; *p; p++)
		if (*p == '\n') printf("\\n");
		else if (*p == '\t') printf("\\t");
		else if (*p == '"' || *p == '\\') printf("\\%c", *p);
		else if (*p < 040 || *p >= 0177) printf("\\%03o", *p);
		else putchar(*p);
	putchar('"');
}
static void cellout(cell *c)
{
	if (c == NULL) { printf("nil"); return; }
	if (c->nval == NULL) printf("$%d", (int)(c - fldtab));
	else if (c->tval & CON) {
		if (c->tval & STR) strout(c->sval);
		else printf("%g", c->fval);
	} else
		printf("%s", c->nval);
	if (c->tval & ARR) printf("[]");
}
static void dfaout(struct fa *f)
{
	int i;
	printf("dfa{");
	for (i = 0; probes[i]; i++)
		putchar(match(f, probes[i]) ? '1' : '0');
	printf("}");
}
static void dump(node *n, int depth);
static void list(node *n, int depth)
{
	if (n == NULL) { printf(" nil"); return; }
	if (n->nnext == NULL && n->ntype == NVALUE) { putchar(' '); dump(n, depth); return; }
	for (; n != NULL; n = n->nnext) {
		printf("\n%*s", 2 * depth, "");
		dump(n, depth);
	}
}
/* argument kinds: n node list, c cell, d dfa, f function code, r print
 * redirection token followed by its target when present */
static char *kinds(int op)
{
	switch (op) {
	case PROGRAM: return "nnn";
	case BOR: case AND: case LT: case LE: case GT: case GE: case EQ: case NE:
	case ADD: case MINUS: case MULT: case DIVIDE: case MOD: case CAT:
	case PASTAT: case INDEX: case WHILE:
	case ASSIGN: case ADDEQ: case SUBEQ: case MULTEQ: case DIVEQ: case MODEQ:
		return "nn";
	case NOT: case INDIRECT: case SPRINTF: case UMINUS:
	case PREINCR: case POSTINCR: case PREDECR: case POSTDECR:
		return "n";
	case MATCH: case NOTMATCH: return "nd";
	case ARRAY: return "cn";
	case FNCN: return "fn";
	case SUBSTR: case IF: return "nnn";
	case SPLIT: return "ncn";
	case PRINT: case PRINTF: return "nr";	/* r: redirection and its target */
	case FOR: return "nnnn";
	case IN: return "ccn";
	}
	return "";
}
static void dump(node *n, int depth)
{
	char *k;
	int i, f;
	if (n == NULL) { printf("nil"); return; }
	if (n->ntype == NVALUE) {
		printf("%s:", n->subtype == CCON ? "con" : n->subtype == CVAR ? "var" : n->subtype == CFLD ? "fld" : "val");
		cellout((cell *)n->nobj);
		return;
	}
	if (n->ntype == NPA2) {
		printf("(range%ld", n->nobj);
		for (i = 0; i < 3; i++) list(n->narg[i], depth + 1);
		printf(")");
		return;
	}
	printf("(%s%s", n->ntype == NSTAT ? "" : "=", tokname((int)n->nobj));
	k = kinds((int)n->nobj);
	for (i = 0; k[i]; i++) {
		switch (k[i]) {
		case 'n': list(n->narg[i], depth + 1); break;
		case 'c': putchar(' '); cellout((cell *)n->narg[i]); break;
		case 'd': putchar(' '); dfaout((struct fa *)n->narg[i]); break;
		case 'f':
			f = (int)(long)n->narg[i];
			printf(" %s", f == FLENGTH ? "length" : f == FSQRT ? "sqrt" : f == FEXP ? "exp" :
				f == FLOG ? "log" : f == FINT ? "int" : "?");
			break;
		case 'r':
			if (n->narg[i] == NULL) { printf(" -"); break; }
			if ((long)n->narg[i] < 256) printf(" '%c'", (int)(long)n->narg[i]);
			else printf(" %s", tokname((int)(long)n->narg[i]));
			list(n->narg[i + 1], depth + 1);
			break;
		}
	}
	printf(")");
}
main(argc, argv) char **argv;
{
	syminit();
	if (argc == 3 && !strcmp(argv[1], "-f")) {
		if ((yyin = fopen(argv[2], "r")) == NULL)
			error(FATAL, "can't open %s", argv[2]);
	} else if (argc == 2)
		lexprog = argv[1];
	else
		error(FATAL, "usage: awk-parse 'program' | -f file");
	yyparse();
	if (errorflag) {
		printf("errorflag %d\n", errorflag);
		return(2);
	}
	dump(winner, 0);
	printf("\n");
	return(0);
}
