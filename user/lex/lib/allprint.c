/* Nexix: libln printing helpers; an unset yyout means standard output, since
 * generated scanners bind their streams only when yylex first runs. */
# include <stdio.h>
static FILE *yyprfile(){
	extern FILE *yyout;
	return(yyout ? yyout : stdout);
	}
allprint(c)
  char c; {
	FILE *f = yyprfile();
	switch(c){
		case '\n':
			fprintf(f,"\\n");
			break;
		case '\t':
			fprintf(f,"\\t");
			break;
		case '\b':
			fprintf(f,"\\b");
			break;
		case ' ':
			fprintf(f,"\\\bb");
			break;
		default:
			if(!printable(c))
				fprintf(f,"\\%-3o",c);
			else 
				putc(c,f);
			break;
		}
	return;
	}
sprint(s)
  char *s; {
	while(*s)
		allprint(*s++);
	return;
	}
printable(c)
  int c;
	{
	return(040 < c && c < 0177);
	}
