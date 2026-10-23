/*	join F1 F2 on stuff */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#define F1 0
#define F2 1
#define	NFLD	20	/* max -o entries per file */
#define comp() cmp(field(F1,j1),field(F2,j2))

FILE *f[2];
/* Lines and field tables grow on demand; V7 used BUFSIZ lines and 20 fields. */
char *buf[2];	/*input lines */
size_t bufsize[2];
char **ppi[2];	/* pointers to fields in lines */
int nppi[2];
int nf[2];	/* fields in the current lines */
int	j1	= 1;	/* join of this field of file 1 */
int	j2	= 1;	/* join of this field of file 2 */
int	olist[2*NFLD];	/* output these fields */
int	olistf[2*NFLD];	/* from these files */
int	no;	/* number of entries in olist */
int	sep1	= ' ';	/* default field separator */
int	sep2	= '\t';
char*	null	= "";
char	*field();
int	unpub1;
int	unpub2;
int	aflg;

main(argc, argv)
char *argv[];
{
	int i;
	int n1, n2;
	long top2, bot2;
	long ftell();

	while (argc > 1 && argv[1][0] == '-') {
		if (argv[1][1] == '\0')
			break;
		switch (argv[1][1]) {
		case 'a':
			switch(argv[1][2]) {
			case '1':
				aflg |= 1;
				break;
			case '2':
				aflg |= 2;
				break;
			default:
				aflg |= 3;
			}
			break;
		case 'e':
			if (argc < 3)
				usage();
			null = argv[2];
			argv++;
			argc--;
			break;
		case 't':
			sep1 = sep2 = argv[1][2];
			break;
		case 'o':
			for (no = 0; no < 2*NFLD && argc > 2; no++) {
				if (argv[2][0] == '1' && argv[2][1] == '.') {
					olistf[no] = F1;
					olist[no] = number(&argv[2][2]);
				} else if (argv[2][0] == '2' && argv[2][1] == '.') {
					olist[no] = number(&argv[2][2]);
					olistf[no] = F2;
				} else
					break;
				argc--;
				argv++;
			}
			break;
		case 'j':
			if (argc < 3)
				usage();
			if (argv[1][2] == '1')
				j1 = number(argv[2]);
			else if (argv[1][2] == '2')
				j2 = number(argv[2]);
			else
				j1 = j2 = number(argv[2]);
			argc--;
			argv++;
			break;
		}
		argc--;
		argv++;
	}
	for (i = 0; i < no; i++)
		olist[i]--;	/* 0 origin */
	if (argc != 3)
		usage();
	j1--;
	j2--;	/* everyone else believes in 0 origin */
	if (argv[1][0] == '-')
		f[F1] = stdin;
	else if ((f[F1] = fopen(argv[1], "r")) == NULL)
		error("can't open %s", argv[1]);
	if ((f[F2] = fopen(argv[2], "r")) == NULL)
		error("can't open %s", argv[2]);

#define get1() n1=input(F1)
#define get2() n2=input(F2)
	get1();
	bot2 = ftell(f[F2]);
	get2();
	while(n1>0 && n2>0 || aflg!=0 && n1+n2>0) {
		if(n1>0 && n2>0 && comp()>0 || n1==0) {
			if(aflg&2) output(0, n2);
			bot2 = ftell(f[F2]);
			get2();
		} else if(n1>0 && n2>0 && comp()<0 || n2==0) {
			if(aflg&1) output(n1, 0);
			get1();
		} else /*(n1>0 && n2>0 && comp()==0)*/ {
			while(n2>0 && comp()==0) {
				output(n1, n2);
				top2 = ftell(f[F2]);
				get2();
			}
			fseek(f[F2], bot2, 0);
			get2();
			get1();
			for(;;) {
				if(n1>0 && n2>0 && comp()==0) {
					output(n1, n2);
					get2();
				} else if(n1>0 && n2>0 && comp()<0 || n2==0) {
					fseek(f[F2], bot2, 0);
					get2();
					get1();
				} else /*(n1>0 && n2>0 && comp()>0 || n1==0)*/{
					fseek(f[F2], top2, 0);
					bot2 = top2;
					get2();
					break;
				}
			}
		}
	}
	if (ferror(f[F1]) || ferror(f[F2]))
		error("read error");
	if (fflush(stdout) == EOF || ferror(stdout))
		error("write error");
	return(0);
}

usage()
{
	error("usage: join [-j1 x -j2 y] [-o list] file1 file2");
}

number(s)
char *s;
{
	long n = 0;

	while (*s >= '0' && *s <= '9')
		if ((n = n*10 + *s++ - '0') > 1000000)
			usage();
	if (*s || n < 1)
		usage();
	return(n);
}

/* A missing field is empty rather than a stale pointer. */
char *
field(n, i)
{
	return(i < nf[n] ? ppi[n][i] : "");
}

more(p, size, unit)
char **p;
size_t *size;
{
	size_t grown = *size ? *size*2 : 256;
	char *q = realloc(*p, grown*unit);

	if (q == NULL)
		error("out of memory");
	*p = q;
	*size = grown;
}

input(n)		/* get input line and split into fields */
{
	register int i, c;
	char *bp;
	char **pp;
	size_t len = 0, slots;

	for (;;) {
		if (len+1 >= bufsize[n])
			more(&buf[n], &bufsize[n], 1);
		if ((c = getc(f[n])) == EOF) {
			if (len == 0)
				return(nf[n] = 0);
			break;
		}
		buf[n][len++] = c;
		if (c == '\n')
			break;
	}
	buf[n][len] = '\0';	/* the final line may lack a newline */
	bp = buf[n];
	for (i = 0; ; i++) {
		if (i >= nppi[n]) {
			slots = nppi[n];
			more((char **)&ppi[n], &slots, sizeof *ppi[n]);
			nppi[n] = slots;
		}
		pp = ppi[n] + i;
		if (sep1 == ' ')	/* strip multiples */
			while ((c = *bp) == sep1 || c == sep2)
				bp++;	/* skip blanks */
		else
			c = *bp;
		if (c == '\n' || c == '\0')
			break;
		*pp = bp;	/* record beginning */
		while ((c = *bp) != sep1 && c != '\n' && c != sep2 && c != '\0')
			bp++;
		if (*bp == '\0') {	/* unterminated final line */
			i++;
			break;
		}
		*bp++ = '\0';	/* mark end by overwriting blank */
	}
	return(nf[n] = i);
}

output(on1, on2)	/* print items from olist */
int on1, on2;
{
	int i;
	char *temp;

	if (no <= 0) {	/* default case */
		printf("%s", on1? field(F1,j1): field(F2,j2));
		for (i = 0; i < on1; i++)
			if (i != j1)
				printf("%c%s", sep1, ppi[F1][i]);
		for (i = 0; i < on2; i++)
			if (i != j2)
				printf("%c%s", sep1, ppi[F2][i]);
		printf("\n");
	} else {
		for (i = 0; i < no; i++) {
			temp = field(olistf[i], olist[i]);
			if(olistf[i]==F1 && on1<=olist[i] ||
			   olistf[i]==F2 && on2<=olist[i] ||
			   *temp==0)
				temp = null;
			printf("%s", temp);
			if (i == no - 1)
				printf("\n");
			else
				printf("%c", sep1);
		}
	}
}

error(s1, s2)
char *s1, *s2;
{
	fprintf(stderr, "join: ");
	fprintf(stderr, s1, s2);
	fprintf(stderr, "\n");
	exit(1);
}

cmp(s1, s2)
char *s1, *s2;
{
	return(strcmp(s1, s2));
}
