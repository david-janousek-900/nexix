%{
# include "y.tab.h"
extern int yylval;
int ntokens;
%}
%%
[0-9]+		{ ntokens++; yylval = atoi(yytext); return(NUMBER); }
[a-z]		{ ntokens++; yylval = yytext[0] - 'a'; return(NAME); }
"#".*		;
[ \t]+		;
\n		{ ntokens++; return('\n'); }
.		{ ntokens++; return(yytext[0]); }
%%
