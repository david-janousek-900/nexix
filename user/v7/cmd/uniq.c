/*
 * Deal with duplicated lines in a file
 */
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
int	fields;
int	letters;
int	linec;
char	mode;
int	uniq;
char	*skip();
/* Lines grow on demand; V7 overran fixed 1000-byte buffers. */
struct line { char *s; size_t size; };

main(argc, argv)
int argc;
char *argv[];
{
	static struct line l1, l2;
	char **b1 = &l1.s, **b2 = &l2.s;

	while(argc > 1) {
		if(*argv[1] == '-') {
			if (isdigit(argv[1][1]))
				fields = number(&argv[1][1]);
			else mode = argv[1][1];
			argc--;
			argv++;
			continue;
		}
		if(*argv[1] == '+') {
			letters = number(&argv[1][1]);
			argc--;
			argv++;
			continue;
		}
		if (freopen(argv[1], "r", stdin) == NULL)
			printe("cannot open %s\n", argv[1]);
		break;
	}
	if(argc > 2 && freopen(argv[2], "w", stdout) == NULL)
		printe("cannot create %s\n", argv[2]);

	if(gline(&l1))
		done();
	for(;;) {
		linec++;
		if(gline(&l2)) {
			pline(*b1);
			done();
		}
		if(!equal(*b1, *b2)) {
			pline(*b1);
			linec = 0;
			do {
				linec++;
				if(gline(&l1)) {
					pline(*b2);
					done();
				}
			} while(equal(*b1, *b2));
			pline(*b2);
			linec = 0;
		}
	}
}

gline(buf)
register struct line *buf;
{
	register c;
	size_t i = 0;

	for(;;) {
		if(i+1 >= buf->size) {
			char *grown = realloc(buf->s, buf->size ? buf->size*2 : 1000);
			if(grown == NULL)
				printe("out of memory\n", "");
			buf->s = grown;
			buf->size = buf->size ? buf->size*2 : 1000;
		}
		if((c = getchar()) == '\n')
			break;
		if(c == EOF) {
			if(ferror(stdin))
				printe("read error\n", "");
			/* An unterminated final line still counts. */
			if(i == 0)
				return(1);
			break;
		}
		buf->s[i++] = c;
	}
	buf->s[i] = 0;
	return(0);
}

number(s)
char *s;
{
	long n = 0;

	while(isdigit(*s)) {
		n = n*10 + *s++ - '0';
		if(n > 1000000)
			printe("count too large\n", "");
	}
	return(n);
}

done()
{
	if(fflush(stdout) == EOF || ferror(stdout))
		printe("write error\n", "");
	exit(0);
}

pline(buf)
register char buf[];
{

	switch(mode) {

	case 'u':
		if(uniq) {
			uniq = 0;
			return;
		}
		break;

	case 'd':
		if(uniq) break;
		return;

	case 'c':
		printf("%4d ", linec);
	}
	uniq = 0;
	fputs(buf, stdout);
	putchar('\n');
}

equal(b1, b2)
register char b1[], b2[];
{
	register char c;

	b1 = skip(b1);
	b2 = skip(b2);
	while((c = *b1++) != 0)
		if(c != *b2++) return(0);
	if(*b2 != 0)
		return(0);
	uniq++;
	return(1);
}

char *
skip(s)
register char *s;
{
	register nf, nl;

	nf = nl = 0;
	while(nf++ < fields) {
		while(*s == ' ' || *s == '\t')
			s++;
		while( !(*s == ' ' || *s == '\t' || *s == 0) ) 
			s++;
	}
	while(nl++ < letters && *s != 0) 
			s++;
	return(s);
}

printe(p,s)
char *p,*s;
{
	fprintf(stderr, p, s);
	exit(1);
}
