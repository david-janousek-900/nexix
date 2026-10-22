/* Nexix: the argument is a count of characters to keep. V7 also accepted a
 * pointer into yytext, which an int cannot hold here; a count outside
 * 0..yyleng leaves the token unchanged. */
yyless(x)
{
extern char yytext[];
register char *lastch, *ptr;
extern int yyleng;
extern int yyprevious;
if (x < 0 || x > yyleng)
	return;
lastch = yytext+yyleng;
ptr = x + yytext;
while (lastch > ptr)
	yyunput(*--lastch);
*lastch = 0;
if (ptr >yytext)
	yyprevious = *--lastch;
yyleng = ptr-yytext;
}
