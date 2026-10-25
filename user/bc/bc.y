%{
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#define YYSTYPE intptr_t
intptr_t bundle(int,...), getf(intptr_t), geta(intptr_t);
void output(intptr_t), conout(intptr_t,intptr_t), routput(intptr_t);
void yyerror(char *), getout(void);
int getch(void), yylex(void), yyparse(void);
static char *bc_char(void);
static int bc_break_level(void);
static jmp_buf bc_done;
static int bc_status, bc_lex_mode, bc_last='\n';
%}
%right '='
%left '+' '-'
%left '*' '/' '%'
%right '^'
%left UMINUS

%term LETTER DIGIT SQRT LENGTH _IF  FFF EQ
%term _WHILE _FOR NE LE GE INCR DECR
%term _RETURN _BREAK _DEFINE BASE OBASE SCALE
%term EQPL EQMI EQMUL EQDIV EQREM EQEXP
%term _AUTO DOT
%term QSTR

%{
#include <stdio.h>
FILE *in;
char cary[1000], *cp = { cary };
char string[1000], *str = {string};
int crs = '0';
int rcrs = '0';  /* reset crs */
int bindx = 0;
int lev = 0;
int ln;
char *ss;
int bstack[10] = { 0 };
char *numb[15] = {
  " 0", " 1", " 2", " 3", " 4", " 5",
  " 6", " 7", " 8", " 9", " 10", " 11",
  " 12", " 13", " 14" };
intptr_t pre, post;
%}
%%
start	:
	|  start stat tail
		= output( $2 );
	|  start def dargs ')' '{' dlist slist '}'
		={	bundle(6,(intptr_t)(pre),(intptr_t)($7),(intptr_t)(post),(intptr_t)("0"),(intptr_t)(numb[lev]),(intptr_t)("Q"));
			conout( $$, $2 );
			rcrs = crs;
			output( "" );
			lev = bindx = 0;
			}
	;

dlist	:  tail
	| dlist _AUTO dlets tail
	;

stat	:  e
		={ bundle(2,(intptr_t)($1),(intptr_t)("ps.")); }
	|
		={ bundle(1,(intptr_t)("")); }
	|  QSTR
		={ bundle(3,(intptr_t)("["),(intptr_t)($1),(intptr_t)("]P"));}
	|  LETTER '=' e
		={ bundle(3,(intptr_t)($3),(intptr_t)("s"),(intptr_t)($1)); }
	|  LETTER '[' e ']' '=' e
		={ bundle(4,(intptr_t)($6),(intptr_t)($3),(intptr_t)(":"),(intptr_t)(geta($1))); }
	|  LETTER EQOP e
		={ bundle(6,(intptr_t)("l"),(intptr_t)($1),(intptr_t)($3),(intptr_t)($2),(intptr_t)("s"),(intptr_t)($1)); }
	|  LETTER '[' e ']' EQOP e
		={ bundle(8,(intptr_t)($3),(intptr_t)(";"),(intptr_t)(geta($1)),(intptr_t)($6),(intptr_t)($5),(intptr_t)($3),(intptr_t)(":"),(intptr_t)(geta($1)));}
	|  _BREAK
		={ bundle(2,(intptr_t)(numb[bc_break_level()]),(intptr_t)("Q")); }
	|  _RETURN '(' e ')'
		= bundle(4,(intptr_t)($3),(intptr_t)(post),(intptr_t)(numb[lev]),(intptr_t)("Q"));
	|  _RETURN '(' ')'
		= bundle(4,(intptr_t)("0"),(intptr_t)(post),(intptr_t)(numb[lev]),(intptr_t)("Q"));
	| _RETURN
		= bundle(4,(intptr_t)("0"),(intptr_t)(post),(intptr_t)(numb[lev]),(intptr_t)("Q"));
	| SCALE '=' e
		= bundle(2,(intptr_t)($3),(intptr_t)("k"));
	| SCALE EQOP e
		= bundle(4,(intptr_t)("K"),(intptr_t)($3),(intptr_t)($2),(intptr_t)("k"));
	| BASE '=' e
		= bundle(2,(intptr_t)($3),(intptr_t)("i"));
	| BASE EQOP e
		= bundle(4,(intptr_t)("I"),(intptr_t)($3),(intptr_t)($2),(intptr_t)("i"));
	| OBASE '=' e
		= bundle(2,(intptr_t)($3),(intptr_t)("o"));
	| OBASE EQOP e
		= bundle(4,(intptr_t)("O"),(intptr_t)($3),(intptr_t)($2),(intptr_t)("o"));
	|  '{' slist '}'
		={ $$ = $2; }
	|  FFF
		={ bundle(1,(intptr_t)("fY")); }
	|  error
		={ bundle(1,(intptr_t)("c")); }
	|  _IF CRS BLEV '(' re ')' stat
		={	conout( $7, $2 );
			bundle(3,(intptr_t)($5),(intptr_t)($2),(intptr_t)(" "));
			}
	|  _WHILE CRS '(' re ')' stat BLEV
		={	bundle(3,(intptr_t)($6),(intptr_t)($4),(intptr_t)($2));
			conout( $$, $2 );
			bundle(3,(intptr_t)($4),(intptr_t)($2),(intptr_t)(" "));
			}
	|  fprefix CRS re ';' e ')' stat BLEV
		={	bundle(5,(intptr_t)($7),(intptr_t)($5),(intptr_t)("s."),(intptr_t)($3),(intptr_t)($2));
			conout( $$, $2 );
			bundle(5,(intptr_t)($1),(intptr_t)("s."),(intptr_t)($3),(intptr_t)($2),(intptr_t)(" "));
			}
	|  '~' LETTER '=' e
		={	bundle(3,(intptr_t)($4),(intptr_t)("S"),(intptr_t)($2)); }
	;

EQOP	:  EQPL
		={ $$ = "+"; }
	|  EQMI
		={ $$ = "-"; }
	|  EQMUL
		={ $$ = "*"; }
	|  EQDIV
		={ $$ = "/"; }
	|  EQREM
		={ $$ = "%"; }
	|  EQEXP
		={ $$ = "^"; }
	;

fprefix	:  _FOR '(' e ';'
		={ $$ = $3; }
	;

BLEV	:
		={ --bindx; }
	;

slist	:  stat
	|  slist tail stat
		={ bundle(2,(intptr_t)($1),(intptr_t)($3)); }
	;

tail	:  '\n'
		={ln++;}
	|  ';'
	;

re	:  e EQ e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("="));
	|  e '<' e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)(">"));
	|  e '>' e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("<"));
	|  e NE e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("!="));
	|  e GE e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("!>"));
	|  e LE e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("!<"));
	|  e
		= bundle(2,(intptr_t)($1),(intptr_t)(" 0!="));
	;

e	:  e '+' e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("+"));
	|  e '-' e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("-"));
	| '-' e		%prec UMINUS
		= bundle(3,(intptr_t)(" 0"),(intptr_t)($2),(intptr_t)("-"));
	|  e '*' e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("*"));
	|  e '/' e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("/"));
	|  e '%' e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("%"));
	|  e '^' e
		= bundle(3,(intptr_t)($1),(intptr_t)($3),(intptr_t)("^"));
	|  LETTER '[' e ']'
		={ bundle(3,(intptr_t)($3),(intptr_t)(";"),(intptr_t)(geta($1))); }
	|  LETTER INCR
		= bundle(4,(intptr_t)("l"),(intptr_t)($1),(intptr_t)("d1+s"),(intptr_t)($1));
	|  INCR LETTER
		= bundle(4,(intptr_t)("l"),(intptr_t)($2),(intptr_t)("1+ds"),(intptr_t)($2));
	|  DECR LETTER
		= bundle(4,(intptr_t)("l"),(intptr_t)($2),(intptr_t)("1-ds"),(intptr_t)($2));
	|  LETTER DECR
		= bundle(4,(intptr_t)("l"),(intptr_t)($1),(intptr_t)("d1-s"),(intptr_t)($1));
	| LETTER '[' e ']' INCR
		= bundle(7,(intptr_t)($3),(intptr_t)(";"),(intptr_t)(geta($1)),(intptr_t)("d1+"),(intptr_t)($3),(intptr_t)(":"),(intptr_t)(geta($1)));
	| INCR LETTER '[' e ']'
		= bundle(7,(intptr_t)($4),(intptr_t)(";"),(intptr_t)(geta($2)),(intptr_t)("1+d"),(intptr_t)($4),(intptr_t)(":"),(intptr_t)(geta($2)));
	| LETTER '[' e ']' DECR
		= bundle(7,(intptr_t)($3),(intptr_t)(";"),(intptr_t)(geta($1)),(intptr_t)("d1-"),(intptr_t)($3),(intptr_t)(":"),(intptr_t)(geta($1)));
	| DECR LETTER '[' e ']'
		= bundle(7,(intptr_t)($4),(intptr_t)(";"),(intptr_t)(geta($2)),(intptr_t)("1-d"),(intptr_t)($4),(intptr_t)(":"),(intptr_t)(geta($2)));
	| SCALE INCR
		= bundle(1,(intptr_t)("Kd1+k"));
	| INCR SCALE
		= bundle(1,(intptr_t)("K1+dk"));
	| SCALE DECR
		= bundle(1,(intptr_t)("Kd1-k"));
	| DECR SCALE
		= bundle(1,(intptr_t)("K1-dk"));
	| BASE INCR
		= bundle(1,(intptr_t)("Id1+i"));
	| INCR BASE
		= bundle(1,(intptr_t)("I1+di"));
	| BASE DECR
		= bundle(1,(intptr_t)("Id1-i"));
	| DECR BASE
		= bundle(1,(intptr_t)("I1-di"));
	| OBASE INCR
		= bundle(1,(intptr_t)("Od1+o"));
	| INCR OBASE
		= bundle(1,(intptr_t)("O1+do"));
	| OBASE DECR
		= bundle(1,(intptr_t)("Od1-o"));
	| DECR OBASE
		= bundle(1,(intptr_t)("O1-do"));
	|  LETTER '(' cargs ')'
		= bundle(4,(intptr_t)($3),(intptr_t)("l"),(intptr_t)(getf($1)),(intptr_t)("x"));
	|  LETTER '(' ')'
		= bundle(3,(intptr_t)("l"),(intptr_t)(getf($1)),(intptr_t)("x"));
	|  cons
		={ bundle(2,(intptr_t)(" "),(intptr_t)($1)); }
	|  DOT cons
		={ bundle(2,(intptr_t)(" ."),(intptr_t)($2)); }
	|  cons DOT cons
		={ bundle(4,(intptr_t)(" "),(intptr_t)($1),(intptr_t)("."),(intptr_t)($3)); }
	|  cons DOT
		={ bundle(3,(intptr_t)(" "),(intptr_t)($1),(intptr_t)(".")); }
	|  DOT
		={ $$ = "l."; }
	|  LETTER
		= { bundle(2,(intptr_t)("l"),(intptr_t)($1)); }
	|  LETTER '=' e
		={ bundle(3,(intptr_t)($3),(intptr_t)("ds"),(intptr_t)($1)); }
	|  LETTER EQOP e	%prec '='
		={ bundle(6,(intptr_t)("l"),(intptr_t)($1),(intptr_t)($3),(intptr_t)($2),(intptr_t)("ds"),(intptr_t)($1)); }
	| LETTER '[' e ']' '=' e
		= { bundle(5,(intptr_t)($6),(intptr_t)("d"),(intptr_t)($3),(intptr_t)(":"),(intptr_t)(geta($1))); }
	| LETTER '[' e ']' EQOP e
		= { bundle(9,(intptr_t)($3),(intptr_t)(";"),(intptr_t)(geta($1)),(intptr_t)($6),(intptr_t)($5),(intptr_t)("d"),(intptr_t)($3),(intptr_t)(":"),(intptr_t)(geta($1))); }
	| LENGTH '(' e ')'
		= bundle(2,(intptr_t)($3),(intptr_t)("Z"));
	| SCALE '(' e ')'
		= bundle(2,(intptr_t)($3),(intptr_t)("X"));	/* must be before '(' e ')' */
	|  '(' e ')'
		= { $$ = $2; }
	|  '?'
		={ bundle(1,(intptr_t)("?")); }
	|  SQRT '(' e ')'
		={ bundle(2,(intptr_t)($3),(intptr_t)("v")); }
	| '~' LETTER
		={ bundle(2,(intptr_t)("L"),(intptr_t)($2)); }
	| SCALE '=' e
		= bundle(2,(intptr_t)($3),(intptr_t)("dk"));
	| SCALE EQOP e		%prec '='
		= bundle(4,(intptr_t)("K"),(intptr_t)($3),(intptr_t)($2),(intptr_t)("dk"));
	| BASE '=' e
		= bundle(2,(intptr_t)($3),(intptr_t)("di"));
	| BASE EQOP e		%prec '='
		= bundle(4,(intptr_t)("I"),(intptr_t)($3),(intptr_t)($2),(intptr_t)("di"));
	| OBASE '=' e
		= bundle(2,(intptr_t)($3),(intptr_t)("do"));
	| OBASE EQOP e		%prec '='
		= bundle(4,(intptr_t)("O"),(intptr_t)($3),(intptr_t)($2),(intptr_t)("do"));
	| SCALE
		= bundle(1,(intptr_t)("K"));
	| BASE
		= bundle(1,(intptr_t)("I"));
	| OBASE
		= bundle(1,(intptr_t)("O"));
	;

cargs	:  eora
	|  cargs ',' eora
		= bundle(2,(intptr_t)($1),(intptr_t)($3));
	;
eora:	  e
	| LETTER '[' ']'
		=bundle(2,(intptr_t)("l"),(intptr_t)(geta($1)));
	;

cons	:  constant
		={ *bc_char() = '\0'; }

constant:
	  '_'
		={ $$ = (intptr_t)cp; *bc_char() = '_'; }
	|  DIGIT
		={ $$ = (intptr_t)cp; *bc_char() = $1; }
	|  constant DIGIT
		={ *bc_char() = $2; }
	;

CRS	:
		={ $$ = (intptr_t)cp; *bc_char() = crs++; *bc_char() = '\0';
			if(crs == '[')crs+=3;
			if(crs == 'a')crs='{';
			if(crs >= 0241){yyerror("program too big");
				getout();
			}
			if (bindx >= 10 || lev >= 14) yyerror("nesting too deep");
            bstack[bindx++] = lev++; }
	;

def	:  _DEFINE LETTER '('
		={	$$ = getf($2);
			pre = (intptr_t)"";
			post = (intptr_t)"";
			lev = 1;
			bstack[bindx=0] = 0;
			}
	;

dargs	:
	|  lora
		={ pp( $1 ); }
	|  dargs ',' lora
		={ pp( $3 ); }
	;

dlets	:  lora
		={ tp($1); }
	|  dlets ',' lora
		={ tp($3); }
	;
lora	:  LETTER
	|  LETTER '[' ']'
		={ $$ = geta($1); }
	;

%%
# define error 256

int peekc = -1;
int sargc;
int ifile;
char **sargv;

char funtab[52] = {
	01,0,02,0,03,0,04,0,05,0,06,0,07,0,010,0,011,0,012,0,013,0,014,0,015,0,016,0,017,0,
	020,0,021,0,022,0,023,0,024,0,025,0,026,0,027,0,030,0,031,0,032,0 };
char atab[52] = {
	0241,0,0242,0,0243,0,0244,0,0245,0,0246,0,0247,0,0250,0,0251,0,0252,0,0253,0,
	0254,0,0255,0,0256,0,0257,0,0260,0,0261,0,0262,0,0263,0,0264,0,0265,0,0266,0,
	0267,0,0270,0,0271,0,0272,0};
char *letr[26] = {
  "a","b","c","d","e","f","g","h","i","j",
  "k","l","m","n","o","p","q","r","s","t",
  "u","v","w","x","y","z" } ;
char *dot = { "." };
yylex(){
	int c, ch;
restart:
	c = getch();
	peekc = -1;
	while( c == ' ' || c == '\t' ) c = getch();
	if(c == '\\'){
		getch();
		goto restart;
	}
	if( c<= 'z' && c >= 'a' ) {
		/* look ahead to look for reserved words */
		peekc = getch();
		if( peekc >= 'a' && peekc <= 'z' ){ /* must be reserved word */
			if( c=='i' && peekc=='f' ){ c=_IF; goto skip; }
			if( c=='w' && peekc=='h' ){ c=_WHILE; goto skip; }
			if( c=='f' && peekc=='o' ){ c=_FOR; goto skip; }
			if( c=='s' && peekc=='q' ){ c=SQRT; goto skip; }
			if( c=='r' && peekc=='e' ){ c=_RETURN; goto skip; }
			if( c=='b' && peekc=='r' ){ c=_BREAK; goto skip; }
			if( c=='d' && peekc=='e' ){ c=_DEFINE; goto skip; }
			if( c=='s' && peekc=='c' ){ c= SCALE; goto skip; }
			if( c=='b' && peekc=='a' ){ c=BASE; goto skip; }
			if( c=='i' && peekc == 'b'){ c=BASE; goto skip; }
			if( c=='o' && peekc=='b' ){ c=OBASE; goto skip; }
			if( c=='d' && peekc=='i' ){ c=FFF; goto skip; }
			if( c=='a' && peekc=='u' ){ c=_AUTO; goto skip; }
			if( c == 'l' && peekc=='e'){ c=LENGTH; goto skip; }
			if( c == 'q' && peekc == 'u'){getout();}
			/* could not be found */
			return( error );
		skip:	/* skip over rest of word */
			peekc = -1;
			while( (ch = getch()) >= 'a' && ch <= 'z' );
			peekc = ch;
			return( c );
		}

		/* usual case; just one single letter */

		yylval = (intptr_t)letr[c-'a'];
		return( LETTER );
	}
	if( c>= '0' && c <= '9' || c>= 'A' && c<= 'F' ){
		yylval = c;
		return( DIGIT );
	}
	switch( c ){
	case '.':	return( DOT );
	case '=':
		switch( peekc = getch() ){
		case '=': c=EQ; goto gotit;
		case '+': c=EQPL; goto gotit;
		case '-': c=EQMI; goto gotit;
		case '*': c=EQMUL; goto gotit;
		case '/': c=EQDIV; goto gotit;
		case '%': c=EQREM; goto gotit;
		case '^': c=EQEXP; goto gotit;
		default:   return( '=' );
			  gotit:     peekc = -1; return(c);
		  }
	case '+':	return( cpeek( '+', INCR, '+' ) );
	case '-':	return( cpeek( '-', DECR, '-' ) );
	case '<':	return( cpeek( '=', LE, '<' ) );
	case '>':	return( cpeek( '=', GE, '>' ) );
	case '!':	return( cpeek( '=', NE, '!' ) );
	case '/':
		if((peekc = getch()) == '*'){
            bc_lex_mode=2;
			peekc = -1;
			while((getch() != '*') || ((peekc = getch()) != '/'));
            bc_lex_mode=0;
			peekc = -1;
			goto restart;
		}
		else return(c);
	case '"':
         bc_lex_mode=1;
		 yylval = (intptr_t)str;
		 while((c=getch()) != '"'){*str++ = c;
			if(str >= &string[999]){yyerror("string space exceeded");
			getout();
		}
	}
	 *str++ = '\0';
    bc_lex_mode=0;
	return(QSTR);
	default:	 return( c );
	}
}

cpeek( c, yes, no ){
	if( (peekc=getch()) != c ) return( no );
	else {
		peekc = -1;
		return( yes );
	}
}

getch(){
	int ch;
loop:
	ch = (peekc < 0) ? getc(in) : peekc;
	peekc = -1;
	if(ch != EOF) {bc_last=ch;return ch;}
    if(bc_lex_mode) yyerror(bc_lex_mode==1 ? "unterminated string" : "unterminated comment");
    if(bc_last!='\n') {bc_last='\n';return '\n';}
    if(ferror(in)) yyerror("read error");
	if(in!=stdin) fclose(in);
	if(++ifile > sargc){
		if(ifile >= sargc+2)getout();
		in = stdin;
		ln = 0;
		goto loop;
	}
	if((in = fopen(sargv[ifile],"r")) != NULL){
		ln = 0;
		ss = sargv[ifile];
		goto loop;
	}
	yyerror("cannot open input file");
}
#define b_sp_max 3000
static intptr_t b_space[b_sp_max], *b_sp_nxt = b_space;
int bdebug;
intptr_t bundle(int count, ...)
{
    intptr_t *result=b_sp_nxt;
    va_list ap;
    if (count < 0 || b_sp_nxt > b_space+b_sp_max-count-1)
        yyerror("bundling space exceeded");
    va_start(ap,count);
    while(count--) *b_sp_nxt++=va_arg(ap,intptr_t);
    va_end(ap);
    *b_sp_nxt++=0;
    yyval=(intptr_t)result;
    return yyval;
}

void routput(intptr_t value)
{
    uintptr_t addr=(uintptr_t)value;
    if (addr >= (uintptr_t)b_space && addr < (uintptr_t)(b_space+b_sp_max)) {
        intptr_t *p=(intptr_t *)addr;
        while (*p) routput(*p++);
    } else if (value) fputs((char *)value,stdout);
}
void output(intptr_t value)
{
    routput(value);
    b_sp_nxt=b_space;
    putchar('\n');
    if (fflush(stdout)==EOF) { bc_status=1; getout(); }
    cp=cary; crs=rcrs;
}
void conout(intptr_t value,intptr_t reg)
{
    putchar('['); routput(value); printf("]s%s\n",(char *)reg);
    if (fflush(stdout)==EOF) { bc_status=1; getout(); }
    --lev;
}
void yyerror(char *message)
{
    fprintf(stderr,"bc: %s on line %d, %s\n",message,ln+1,ss?ss:"standard input");
    bc_status=1; getout();
}
static char *bc_char(void)
{
    if(cp >= cary+sizeof(cary)) yyerror("constant space exceeded");
    return cp++;
}
static int bc_break_level(void)
{
    if(bindx <= 0) yyerror("break outside loop");
    return lev-bstack[bindx-1];
}

pp( s ) intptr_t s; {
	/* puts the relevant stuff on pre and post for the letter s */

	bundle(3,(intptr_t)("S"),(intptr_t)(s),(intptr_t)(pre));
	pre = yyval;
	bundle(4,(intptr_t)(post),(intptr_t)("L"),(intptr_t)(s),(intptr_t)("s."));
	post = yyval;
}

tp( s ) intptr_t s; { /* same as pp, but for temps */
	bundle(3,(intptr_t)("0S"),(intptr_t)(s),(intptr_t)(pre));
	pre = yyval;
	bundle(4,(intptr_t)(post),(intptr_t)("L"),(intptr_t)(s),(intptr_t)("s."));
	post = yyval;
}

void getout(void)
{
    fputs("q\n",stdout);
    fflush(stdout);
    longjmp(bc_done,1);
}
intptr_t getf(intptr_t value)
{
    char *p=(char *)value;
    return (intptr_t)&funtab[2*(*p-'a')];
}
intptr_t geta(intptr_t value)
{
    char *p=(char *)value;
    return (intptr_t)&atab[2*(*p-'a')];
}
#ifndef BC_DC
#define BC_DC "/bin/dc"
#endif
#ifndef BC_LIBRARY
#define BC_LIBRARY "/usr/lib/lib.b"
#endif
int main(int argc,char **argv)
{
    static char *inputs[66];
    int i,compile=0,pipes[2],backend=-1,saved=-1,status=0;
    inputs[0]=argv[0]; sargc=0;
    for(i=1;i<argc;++i) {
        if(!strcmp(argv[i],"-c") || !strcmp(argv[i],"-d")) compile=1;
        else if(!strcmp(argv[i],"-l")) {
            if(sargc >= 64) { fputs("bc: too many input files\n",stderr); return 1; }
            memmove(inputs+2,inputs+1,sargc*sizeof(*inputs));
            inputs[1]=BC_LIBRARY; ++sargc;
        } else if(argv[i][0]=='-') { fputs("bc: unrecognizable argument\n",stderr); return 1; }
        else { if(sargc >= 64) { fputs("bc: too many input files\n",stderr); return 1; } inputs[++sargc]=argv[i]; }
    }
    setbuf(stdout,NULL);
    if(!compile) {
        if(pipe(pipes)<0) { perror("bc: pipe"); return 1; }
        backend=fork();
        if(backend<0) { close(pipes[0]);close(pipes[1]);perror("bc: fork");return 1; }
        if(!backend) {
            close(pipes[1]);
            if(dup2(pipes[0],0)<0) _exit(1);
            close(pipes[0]); execl(BC_DC,"dc",(char *)0);
            perror("bc: dc");_exit(1);
        }
        close(pipes[0]); saved=dup(1);
        if(saved<0 || dup2(pipes[1],1)<0) { close(pipes[1]);return 1; }
        close(pipes[1]);
    }
    signal(SIGINT,SIG_IGN);
    sargv=inputs; ifile=1; ln=0;
    ss=sargc?inputs[1]:"standard input";
    in=stdin;
    if(!setjmp(bc_done)) {
        if(sargc && (in=fopen(inputs[1],"r"))==NULL) yyerror("cannot open input file");
        if(yyparse()) bc_status=1;
    }
    if(in && in!=stdin) fclose(in);
    if(!compile) {
        fflush(stdout);close(1);dup2(saved,1);close(saved);
        while((i=wait(&status))!=backend) { if(i<0 && errno!=EINTR) {status=1;break;} }
    }
    return bc_status || status ? 1 : 0;
}
