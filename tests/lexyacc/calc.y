%token NUMBER NAME
%left '+' '-'
%left '*' '/' '%'
%right UMINUS
%%
lines	:	/* empty */
	|	lines line
	;
line	:	'\n'
	|	expr '\n'		{ printf("%d\n", $1); }
	|	NAME '=' expr '\n'	{ reg[$1] = $3; }
	|	error '\n'		{ yyerrok; printf("error\n"); }
	;
expr	:	'(' expr ')'		{ $$ = $2; }
	|	expr '+' expr		{ $$ = $1 + $3; }
	|	expr '-' expr		{ $$ = $1 - $3; }
	|	expr '*' expr		{ $$ = $1 * $3; }
	|	expr '/' expr		{ if ($3 == 0) { printf("divide by zero\n"); $$ = 0; } else $$ = $1 / $3; }
	|	expr '%' expr		{ $$ = $3 ? $1 % $3 : 0; }
	|	'-' expr %prec UMINUS	{ $$ = -$2; }
	|	NUMBER
	|	NAME			{ $$ = reg[$1]; }
	;
%%
int reg[26];
extern int ntokens, yylineno;
yyerror(s)
char *s;
{
	return(0);
}
main()
{
	yyparse();
	printf("tokens %d lines %d\n", ntokens, yylineno);
	return(0);
}
