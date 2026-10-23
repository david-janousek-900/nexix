#include <stdio.h>
#include <stdlib.h>
int	one;
int	two;
int	three;

char	ldr0[1], ldr1[2] = "\t", ldr2[3] = "\t\t";
char	*ldr[3] = { ldr0, ldr1, ldr2 };
/* Lines grow on demand; V7 split them at 255 bytes. */
struct line { char *s; size_t size; } lb1, lb2;

FILE *ib1;
FILE *ib2;
FILE *openfil();
main(argc,argv)
char	*argv[];
{
	int	l;
	if(argc > 1)  {
		if(*argv[1] == '-' && argv[1][1] != 0) {
			l = 1;
			while(*++argv[1]) {
				switch(*argv[1]) {
				case'1':
					if(!one) {
						one = 1;
						ldr[1][0] =
						ldr[2][l--] = '\0';
					}
					break;
				case '2':
					if(!two) {
						two = 1;
						ldr[2][l--] = '\0';
					}
					break;
				case '3':
					three = 1;
					break;
				default:
				fprintf(stderr,"comm: illegal flag\n");
				exit(1);
				}
			}
			argv++;
			argc--;
		}
	}

	if(argc < 3) {
		fprintf(stderr,"comm: arg count\n");
		exit(1);
	}

	ib1 = openfil(argv[1]);
	ib2 = openfil(argv[2]);


	if(rd(ib1,&lb1) < 0) {
		if(rd(ib2,&lb2) < 0)	done();
		copy(ib2,&lb2,2);
	}
	if(rd(ib2,&lb2) < 0)	copy(ib1,&lb1,1);

	while(1) {

		switch(compare(lb1.s,lb2.s)) {

			case 0:
				wr(lb1.s,3);
				if(rd(ib1,&lb1) < 0) {
					if(rd(ib2,&lb2) < 0)	done();
					copy(ib2,&lb2,2);
				}
				if(rd(ib2,&lb2) < 0)	copy(ib1,&lb1,1);
				continue;

			case 1:
				wr(lb1.s,1);
				if(rd(ib1,&lb1) < 0)	copy(ib2,&lb2,2);
				continue;

			case 2:
				wr(lb2.s,2);
				if(rd(ib2,&lb2) < 0)	copy(ib1,&lb1,1);
				continue;
		}
	}
}

rd(file,buf)
FILE *file;
struct line *buf;
{

	register int c;
	size_t i;
	i = 0;
	for(;;) {
		if(i+1 >= buf->size) {
			char *grown = realloc(buf->s, buf->size ? buf->size*2 : 256);
			if(grown == NULL) {
				fprintf(stderr,"comm: out of memory\n");
				exit(1);
			}
			buf->s = grown;
			buf->size = buf->size ? buf->size*2 : 256;
		}
		if((c = getc(file)) == EOF)
			break;
		if(c == '\n') {
			buf->s[i] = '\0';
			return(0);
		}
		buf->s[i++] = c;
	}
	if(ferror(file)) {
		fprintf(stderr,"comm: read error\n");
		exit(1);
	}
	/* An unterminated final line still counts. */
	buf->s[i] = '\0';
	return(i ? 0 : -1);
}

done()
{
	if(fflush(stdout) == EOF || ferror(stdout))
		exit(1);
	exit(0);
}

wr(str,n)
	char	*str;
{

	switch(n) {

		case 1:
			if(one)	return;
			break;

		case 2:
			if(two)	return;
			break;

		case 3:
			if(three)	return;
	}
	printf("%s%s\n",ldr[n-1],str);
}

copy(ibuf,lbuf,n)
FILE *ibuf;
struct line *lbuf;
{
	do {
		wr(lbuf->s,n);
	} while(rd(ibuf,lbuf) >= 0);

	done();
}

compare(a,b)
	char	*a,*b;
{
	register char *ra,*rb;

	ra = --a;
	rb = --b;
	while(*++ra == *++rb)
		if(*ra == '\0')	return(0);
	if(*ra < *rb)	return(1);
	return(2);
}
FILE *openfil(s)
char *s;
{
	FILE *b;
	if(s[0]=='-' && s[1]==0)
		b = stdin;
	else if((b=fopen(s,"r")) == NULL) {
		fprintf(stderr,"comm: cannot open %s\n",s);
		exit(1);
	}
	return(b);
}
