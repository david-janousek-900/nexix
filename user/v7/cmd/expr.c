/* Native parser for the retained expr.y grammar. The V7 BRE engine follows. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#define ESIZE 256
#define error(c) errxx(c)
static int argc_,arg_;
static char **argv_;
int nbra;
char Mstring[1][128];
static void yyerror(const char *message) { fprintf(stderr,"%s\n",message);exit(2); }
static void errxx(int error) { (void)error;yyerror("RE error"); }
int ematch(char *,char *);
static char *copy(const char *s,size_t n)
{char *p=malloc(n+1);if(!p)yyerror("out of memory");memcpy(p,s,n);p[n]=0;return p;}
static char *number(long value)
{char b[32];sprintf(b,"%ld",value);return copy(b,strlen(b));}
static int integer(const char *s,long *value)
{char *end;errno=0;if(!*s || (*s!='-' && (*s<'0'||*s>'9')) || (*s=='-'&&!s[1]))return 0;*value=strtol(s,&end,10);return !*end && errno!=ERANGE;}
static int truth(const char *s){return *s && strcmp(s,"0");}
static char *match(char *s,char *p)
{int length;Mstring[0][0]=0;length=ematch(s,p);return nbra?copy(Mstring[0],strlen(Mstring[0])):number(length);}
static char *evaluate(const char *op,char *a,char *b)
{
 long x,y,z;int c;
 if(!strcmp(op,"|"))return truth(a)?a:truth(b)?b:"0";
 if(!strcmp(op,"&"))return truth(a)&&truth(b)?a:"0";
 if(!strcmp(op,":"))return match(a,b);
 if(!strcmp(op,"+")||!strcmp(op,"-")||!strcmp(op,"*")||!strcmp(op,"/")||!strcmp(op,"%")){
  if(!integer(a,&x)||!integer(b,&y))yyerror("non-numeric argument");
  if(!strcmp(op,"+")){if(__builtin_add_overflow(x,y,&z))yyerror("arithmetic overflow");}
  else if(!strcmp(op,"-")){if(__builtin_sub_overflow(x,y,&z))yyerror("arithmetic overflow");}
  else if(!strcmp(op,"*")){if(__builtin_mul_overflow(x,y,&z))yyerror("arithmetic overflow");}
  else {if(!y)yyerror("division by zero");if(x==LONG_MIN&&y==-1)yyerror("arithmetic overflow");z=*op=='/'?x/y:x%y;}
  return number(z);
 }
 if(integer(a,&x)&&integer(b,&y))c=x<y?-1:x>y;
 else c=strcmp(a,b);
 if(!strcmp(op,"=")||!strcmp(op,"=="))return c==0?"1":"0";
 if(!strcmp(op,"!="))return c!=0?"1":"0";
 if(!strcmp(op,"<"))return c<0?"1":"0";
 if(!strcmp(op,"<="))return c<=0?"1":"0";
 if(!strcmp(op,">"))return c>0?"1":"0";
 return c>=0?"1":"0";
}
static int precedence(const char *op)
{
 if(!strcmp(op,"|"))return 1;if(!strcmp(op,"&"))return 2;
 if(!strcmp(op,"=")||!strcmp(op,"==")||!strcmp(op,"!=")||!strcmp(op,"<")||!strcmp(op,"<=")||!strcmp(op,">")||!strcmp(op,">="))return 3;
 if(!strcmp(op,"+")||!strcmp(op,"-"))return 4;
 if(!strcmp(op,"*")||!strcmp(op,"/")||!strcmp(op,"%"))return 5;
 if(!strcmp(op,":"))return 6;return 0;
}
static char *parse(int min)
{
 char *a,*b,*c,*token;int p;long start,width;
 if(arg_>=argc_)yyerror("syntax error");token=argv_[arg_++];
 if(!strcmp(token,"(")){a=parse(1);if(arg_>=argc_||strcmp(argv_[arg_++],")"))yyerror("syntax error");}
 else if(!strcmp(token,"length")){a=number(strlen(parse(10)));}
 else if(!strcmp(token,"index")){a=parse(10);b=parse(10);size_t i;for(i=0;a[i]&&!strchr(b,a[i]);++i);a=number(a[i]?i+1:0);}
 else if(!strcmp(token,"substr")){a=parse(9);b=parse(9);c=parse(9);if(!integer(b,&start)||!integer(c,&width))yyerror("non-numeric argument");size_t n=strlen(a);if(start<=0||width<=0||(unsigned long)start>n)a="";else{a+=start-1;n-=start-1;a=copy(a,(unsigned long)width<n?(size_t)width:n);}}
 else if(!strcmp(token,"match")){a=parse(8);b=parse(8);a=match(a,b);}
 else {if(precedence(token)||!strcmp(token,")"))yyerror("syntax error");a=token;}
 while(arg_<argc_ && (p=precedence(argv_[arg_]))>=min){token=argv_[arg_++];b=parse(p+1);a=evaluate(token,a,b);}
 return a;
}
int main(int argc,char **argv)
{char *result;argc_=argc;argv_=argv;arg_=1;result=parse(1);if(arg_!=argc_)yyerror("syntax error");puts(result);return truth(result)?0:1;}

#define INIT	register char *sp = instring;
#define GETC()		(*sp++)
#define PEEKC()		(*sp)
#define UNGETC(c)	(--sp)
#define RETURN(c)	return
#define ERROR(c)	errxx(c)


ematch(s, p)
char *s;
register char *p;
{
	static char expbuf[ESIZE];
	char *compile();
	register num;
	extern char *braslist[], *braelist[], *loc2;

	const unsigned char *q;
    for (q=(unsigned char *)p; *q; ++q) if (*q>=128) yyerror("RE error");
    compile(p, expbuf, &expbuf[ESIZE], 0);
	if(nbra > 1)
		yyerror("Too many '\\('s");
	if(advance(s, expbuf)) {
		if(nbra == 1) {
			p = braslist[0];
			num = braelist[0] - p;
			if(num >= (int)sizeof(Mstring[0])) yyerror("RE error");
            strncpy(Mstring[0], p, num);
			if(num >= (int)sizeof(Mstring[0])) yyerror("RE error");
            Mstring[0][num] = '\0';
		}
		return(loc2-s);
	}
	return(0);
}

#define	CBRA	2
#define	CCHR	4
#define	CDOT	8
#define	CCL	12
#define	CDOL	20
#define	CEOF	22
#define	CKET	24
#define	CBACK	36

#define	STAR	01
#define RNGE	03

#define	NBRA	9

#define PLACE(c) do { if ((unsigned)(c)>=128) ERROR(49); ep[(c)>>3] |= bittab[(c)&7]; } while(0)
#define ISTHERE(c)	(ep[c >> 3] & bittab[c & 07])

char	*braslist[NBRA];
char	*braelist[NBRA];
int	nbra;
char *loc1, *loc2, *locs;
int	sed;

int	circf;
int	low;
int	size;

char	bittab[] = {
	1,
	2,
	4,
	8,
	16,
	32,
	64,
	128
};

char *
compile(instring, ep, endbuf, seof)
register char *ep;
char *instring, *endbuf;
{
	INIT	/* Dependent declarations and initializations */
	register c;
	register eof = seof;
	char *lastep = instring;
	int cclcnt;
	char bracket[NBRA], *bracketp;
	int closed;
	char neg;
	int lc;
	int i, cflg;

	lastep = 0;
	if((c = GETC()) == eof) {
		if(*ep == 0 && !sed)
			ERROR(41);
		RETURN(ep);
	}
	bracketp = bracket;
	circf = closed = nbra = 0;
	if (c == '^')
		circf++;
	else
		UNGETC(c);
	for (;;) {
		if (ep >= endbuf)
			ERROR(50);
		if((c = GETC()) != '*' && ((c != '\\') || (PEEKC() != '{')))
			lastep = ep;
		if (c == eof) {
			*ep++ = CEOF;
			RETURN(ep);
		}
		switch (c) {

		case '.':
			*ep++ = CDOT;
			continue;

		case '\n':
			ERROR(36);
		case '*':
			if (lastep==0 || *lastep==CBRA || *lastep==CKET)
				goto defchar;
			*lastep |= STAR;
			continue;

		case '$':
			if(PEEKC() != eof)
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			if(&ep[17] >= endbuf)
				ERROR(50);

			*ep++ = CCL;
			lc = 0;
			for(i = 0; i < 16; i++)
				ep[i] = 0;

			neg = 0;
			if((c = GETC()) == '^') {
				neg = 1;
				c = GETC();
			}

			do {
				if(c == '\0' || c == '\n')
					ERROR(49);
				if(c == '-' && lc != 0) {
					if ((c = GETC()) == ']') {
						PLACE('-');
						break;
					}
					while(lc < c) {
						PLACE(lc);
						lc++;
					}
				}
				lc = c;
				PLACE(c);
			} while((c = GETC()) != ']');
			if(neg) {
				for(cclcnt = 0; cclcnt < 16; cclcnt++)
					ep[cclcnt] ^= -1;
				ep[0] &= 0376;
			}

			ep += 16;

			continue;

		case '\\':
			switch(c = GETC()) {
            case 0: ERROR(36);

			case '(':
				if(nbra >= NBRA)
					ERROR(43);
				*bracketp++ = nbra;
				*ep++ = CBRA;
				*ep++ = nbra++;
				continue;

			case ')':
				if(bracketp <= bracket)
					ERROR(42);
				*ep++ = CKET;
				*ep++ = *--bracketp;
				closed++;
				continue;

			case '{':
				if(lastep == (char *) (0))
					goto defchar;
				*lastep |= RNGE;
				cflg = 0;
			nlim:
				c = GETC();
				i = 0;
				do {
					if ('0' <= c && c <= '9')
						i = 10 * i + c - '0';
					else
						ERROR(16);
				} while(((c = GETC()) != '\\') && (c != ','));
				if (i > 255)
					ERROR(11);
				*ep++ = i;
				if (c == ',') {
					if(cflg++)
						ERROR(44);
					if((c = GETC()) == '\\')
						*ep++ = 255;
					else {
						UNGETC(c);
						goto nlim; /* get 2'nd number */
					}
				}
				if(GETC() != '}')
					ERROR(45);
				if(!cflg)	/* one number */
					*ep++ = i;
				else if((ep[-1] & 0377) < (ep[-2] & 0377))
					ERROR(46);
				continue;

			case '\n':
				ERROR(36);

			case 'n':
				c = '\n';
				goto defchar;

			default:
				if(c >= '1' && c <= '9') {
					if((c -= '1') >= closed)
						ERROR(25);
					*ep++ = CBACK;
					*ep++ = c;
					continue;
				}
			}
			/* Drop through to default to use \ to turn off special chars */

		defchar:
		default:
			lastep = ep;
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
}

step(p1, p2)
register char *p1, *p2;
{
	register c;

	if (circf) {
		loc1 = p1;
		return(advance(p1, p2));
	}
	/* fast check for first character */
	if (*p2==CCHR) {
		c = p2[1];
		do {
			if (*p1 != c)
				continue;
			if (advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while (*p1++);
		return(0);
	}
		/* regular algorithm */
	do {
		if (advance(p1, p2)) {
			loc1 = p1;
			return(1);
		}
	} while (*p1++);
	return(0);
}

advance(lp, ep)
register char *lp, *ep;
{
	register char *curlp;
	char c;
	char *bbeg;
	int ct;

	for (;;) switch (*ep++) {

	case CCHR:
		if (*ep++ == *lp++)
			continue;
		return(0);

	case CDOT:
		if (*lp++)
			continue;
		return(0);

	case CDOL:
		if (*lp==0)
			continue;
		return(0);

	case CEOF:
		loc2 = lp;
		return(1);

	case CCL:
		c = *lp++ & 0177;
		if(ISTHERE(c)) {
			ep += 16;
			continue;
		}
		return(0);
	case CBRA:
		braslist[*ep++] = lp;
		continue;

	case CKET:
		braelist[*ep++] = lp;
		continue;

	case CCHR|RNGE:
		c = *ep++;
		getrnge(ep);
		while(low--)
			if(*lp++ != c)
				return(0);
		curlp = lp;
		while(size--) 
			if(*lp++ != c)
				break;
		if(size < 0)
			lp++;
		ep += 2;
		goto star;

	case CDOT|RNGE:
		getrnge(ep);
		while(low--)
			if(*lp++ == '\0')
				return(0);
		curlp = lp;
		while(size--)
			if(*lp++ == '\0')
				break;
		if(size < 0)
			lp++;
		ep += 2;
		goto star;

	case CCL|RNGE:
		getrnge(ep + 16);
		while(low--) {
			c = *lp++ & 0177;
			if(!ISTHERE(c))
				return(0);
		}
		curlp = lp;
		while(size--) {
			c = *lp++ & 0177;
			if(!ISTHERE(c))
				break;
		}
		if(size < 0)
			lp++;
		ep += 18;		/* 16 + 2 */
		goto star;

	case CBACK:
		bbeg = braslist[*ep];
		ct = braelist[*ep++] - bbeg;

		if(ecmp(bbeg, lp, ct)) {
			lp += ct;
			continue;
		}
		return(0);

	case CBACK|STAR:
		bbeg = braslist[*ep];
		ct = braelist[*ep++] - bbeg;
		curlp = lp;
		while(ct && ecmp(bbeg, lp, ct))
			lp += ct;

		while(lp >= curlp) {
			if(advance(lp, ep))	return(1);
			if(!ct)break;
            lp -= ct;
		}
		return(0);


	case CDOT|STAR:
		curlp = lp;
		while (*lp++);
		goto star;

	case CCHR|STAR:
		curlp = lp;
		while (*lp++ == *ep);
		ep++;
		goto star;

	case CCL|STAR:
		curlp = lp;
		do {
			c = *lp++ & 0177;
		} while(ISTHERE(c));
		ep += 16;
		goto star;

	star:
		do {
			if(--lp == locs)
				break;
			if (advance(lp, ep))
				return(1);
		} while (lp > curlp);
		return(0);

	}
}

getrnge(str)
register char *str;
{
	low = *str++ & 0377;
	size = *str == 255 ? 20000 : (*str &0377) - low;
}

ecmp(a, b, count)
register char	*a, *b;
register	count;
{
	if(a == b) /* should have been caught in compile() */
		error(51);
	while(count--)
		if(*a++ != *b++)	return(0);
	return(1);
}
