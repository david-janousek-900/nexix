# include <stdio.h>
/* Nexix: the state type matches the generated scanner's struct yysvf. */
struct yysvf {struct yywork *yystoff; struct yysvf *yyother; int *yystops;};
extern struct yysvf *yylstate [], **yylsp, **yyolsp;
/* Nexix: V7 let these block-scope declarations reach yyracc; they are file scope. */
extern FILE *yyout, *yyin;
extern int yyprevious , *yyfnd;
extern char yyextra[];
extern char yytext[];
extern int yyleng;
yyreject ()
{
for( ; yylsp < yyolsp; yylsp++)
	yytext[yyleng++] = yyinput();
if (*yyfnd > 0)
	return(yyracc(*yyfnd++));
while (yylsp-- > yylstate)
	{
	yyunput(yytext[yyleng-1]);
	yytext[--yyleng] = 0;
	if (*yylsp != 0 && (yyfnd= (*yylsp)->yystops) && *yyfnd > 0)
		return(yyracc(*yyfnd++));
	}
if (yytext[0] == 0)
	return(0);
yyoutput(yyprevious = yyinput());
yyleng=0;
return(-1);
}
yyracc(m)
{
yyolsp = yylsp;
if (yyextra[m])
	{
	while (yyback((*yylsp)->yystops, -m) != 1 && yylsp>yylstate)
		{
		yylsp--;
		yyunput(yytext[--yyleng]);
		}
	}
yyprevious = yytext[yyleng-1];
yytext[yyleng] = 0;
return(m);
}
