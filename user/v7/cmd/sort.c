#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define	L	512
#define	N	7
#define	C	20
/* Native in-memory workspace for one sorted run (V7: 32 KiB from brk). */
#define	MEM	(512*1024)
#define NF	10

FILE	*is, *os;
char	*dirtry[] = {"/usr/tmp", "/tmp", NULL};
char	**dirs;
/* Temporary names are sized for -T and use three letters (V7: 30 bytes, two). */
char	*file;
char	*filep;
int	nfiles;
unsigned	nlines;
unsigned	ntext;
int	*lspace;
char	*tspace;
int	cmp(), cmpa();
int	(*compare)() = cmpa;
char	*eol();
int	term();
int	sortq();
int 	mflg;
int	cflg;
int	uflg;
char	*outfil;
int unsafeout;	/*kludge to assure -m -o works*/
char	tabchar;
int 	eargc;
char	**eargv;

char zero[256];

char	fold[256] = {
	0200,0201,0202,0203,0204,0205,0206,0207,
	0210,0211,0212,0213,0214,0215,0216,0217,
	0220,0221,0222,0223,0224,0225,0226,0227,
	0230,0231,0232,0233,0234,0235,0236,0237,
	0240,0241,0242,0243,0244,0245,0246,0247,
	0250,0251,0252,0253,0254,0255,0256,0257,
	0260,0261,0262,0263,0264,0265,0266,0267,
	0270,0271,0272,0273,0274,0275,0276,0277,
	0300,0301,0302,0303,0304,0305,0306,0307,
	0310,0311,0312,0313,0314,0315,0316,0317,
	0320,0321,0322,0323,0324,0325,0326,0327,
	0330,0331,0332,0333,0334,0335,0336,0337,
	0340,0341,0342,0343,0344,0345,0346,0347,
	0350,0351,0352,0353,0354,0355,0356,0357,
	0360,0361,0362,0363,0364,0365,0366,0367,
	0370,0371,0372,0373,0374,0375,0376,0377,
	0000,0001,0002,0003,0004,0005,0006,0007,
	0010,0011,0012,0013,0014,0015,0016,0017,
	0020,0021,0022,0023,0024,0025,0026,0027,
	0030,0031,0032,0033,0034,0035,0036,0037,
	0040,0041,0042,0043,0044,0045,0046,0047,
	0050,0051,0052,0053,0054,0055,0056,0057,
	0060,0061,0062,0063,0064,0065,0066,0067,
	0070,0071,0072,0073,0074,0075,0076,0077,
	0100,0101,0102,0103,0104,0105,0106,0107,
	0110,0111,0112,0113,0114,0115,0116,0117,
	0120,0121,0122,0123,0124,0125,0126,0127,
	0130,0131,0132,0133,0134,0134,0136,0137,
	0140,0101,0102,0103,0104,0105,0106,0107,
	0110,0111,0112,0113,0114,0115,0116,0117,
	0120,0121,0122,0123,0124,0125,0126,0127,
	0130,0131,0132,0173,0174,0175,0176,0177
};
char nofold[256] = {
	0200,0201,0202,0203,0204,0205,0206,0207,
	0210,0211,0212,0213,0214,0215,0216,0217,
	0220,0221,0222,0223,0224,0225,0226,0227,
	0230,0231,0232,0233,0234,0235,0236,0237,
	0240,0241,0242,0243,0244,0245,0246,0247,
	0250,0251,0252,0253,0254,0255,0256,0257,
	0260,0261,0262,0263,0264,0265,0266,0267,
	0270,0271,0272,0273,0274,0275,0276,0277,
	0300,0301,0302,0303,0304,0305,0306,0307,
	0310,0311,0312,0313,0314,0315,0316,0317,
	0320,0321,0322,0323,0324,0325,0326,0327,
	0330,0331,0332,0333,0334,0335,0336,0337,
	0340,0341,0342,0343,0344,0345,0346,0347,
	0350,0351,0352,0353,0354,0355,0356,0357,
	0360,0361,0362,0363,0364,0365,0366,0367,
	0370,0371,0372,0373,0374,0375,0376,0377,
	0000,0001,0002,0003,0004,0005,0006,0007,
	0010,0011,0012,0013,0014,0015,0016,0017,
	0020,0021,0022,0023,0024,0025,0026,0027,
	0030,0031,0032,0033,0034,0035,0036,0037,
	0040,0041,0042,0043,0044,0045,0046,0047,
	0050,0051,0052,0053,0054,0055,0056,0057,
	0060,0061,0062,0063,0064,0065,0066,0067,
	0070,0071,0072,0073,0074,0075,0076,0077,
	0100,0101,0102,0103,0104,0105,0106,0107,
	0110,0111,0112,0113,0114,0115,0116,0117,
	0120,0121,0122,0123,0124,0125,0126,0127,
	0130,0131,0132,0133,0134,0135,0136,0137,
	0140,0141,0142,0143,0144,0145,0146,0147,
	0150,0151,0152,0153,0154,0155,0156,0157,
	0160,0161,0162,0163,0164,0165,0166,0167,
	0170,0171,0172,0173,0174,0175,0176,0177
};

char	nonprint[256] = {
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1
};

char	dict[256] = {
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,
	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1
};

/*
 * The collation tables are indexed from -128 as on the PDP-11; native
 * characters are unsigned, so lookups convert explicitly.
 */
#define	SC(c)	((signed char)(c))

struct	field {
	char *code;
	char *ignore;
	int nflg;
	int rflg;
	int bflg[2];
	int m[2];
	int n[2];
}	fields[NF];
struct field proto = {
	nofold+128,
	zero+128,
	0,
	1,
	0,0,
	0,-1,
	0,0
};
int	nfields;
int 	exitstatus = 1;
char	*setfil();

main(argc, argv)
char **argv;
{
	register a;
	char *arg;
	size_t size;
	struct field *p, *q;
	int i;

	copyproto();
	eargv = argv;
	while (--argc > 0) {
		if(**++argv == '-') for(arg = *argv;;) {
			switch(*++arg) {
			case '\0':
				if(arg[-1] == '-')
					eargv[eargc++] = "-";
				break;

			case 'o':
				if(--argc > 0)
					outfil = *++argv;
				continue;

			case 'T':
				if (--argc > 0)
					dirtry[0] = *++argv;
				continue;

			default:
				field(++*argv,nfields>0);
				break;
			}
			break;
		} else if (**argv == '+') {
			if(++nfields>=NF) {
				diag("too many keys","");
				exit(1);
			}
			copyproto();
			field(++*argv,0);
		} else
			eargv[eargc++] = *argv;
	}
	q = &fields[0];
	for(a=1; a<=nfields; a++) {
		p = &fields[a];
		if(p->code != proto.code) continue;
		if(p->ignore != proto.ignore) continue;
		if(p->nflg != proto.nflg) continue;
		if(p->rflg != proto.rflg) continue;
		if(p->bflg[0] != proto.bflg[0]) continue;
		if(p->bflg[1] != proto.bflg[1]) continue;
		p->code = q->code;
		p->ignore = q->ignore;
		p->nflg = q->nflg;
		p->rflg = q->rflg;
		p->bflg[0] = p->bflg[1] = q->bflg[0];
	}
	if(eargc == 0)
		eargv[eargc++] = "-";
	if(cflg && eargc>1) {
		diag("can check only 1 file","");
		exit(1);
	}
	safeoutfil();

	/* One allocation replaces V7's brk probing; pointers are native width. */
	if((lspace = (int *)malloc(MEM)) == NULL) {
		diag("out of memory","");
		exit(1);
	}
	nlines = (MEM-L) / (sizeof(char *) + 24);
	ntext = MEM - nlines*sizeof(char *);
	tspace = (char *)lspace + nlines*sizeof(char *);
	a = -1;
	for(dirs=dirtry; *dirs; dirs++) {
		size = strlen(*dirs) + 32;
		if((file = realloc(file, size)) == NULL) {
			diag("out of memory","");
			exit(1);
		}
		snprintf(file, size, "%s/stm%05uaaa", *dirs, (unsigned)getpid());
		filep = file + strlen(file) - 3;
		/* Exclusive: V7 truncated whatever the predictable name named. */
		if ( (a=open(file, O_WRONLY|O_CREAT|O_EXCL, 0600)) >=0)
			break;
	}
	if(a < 0) {
		diag("can't locate temp","");
		exit(1);
	}
	close(a);
	signal(SIGHUP, term);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, term);
	signal(SIGPIPE,term);
	signal(SIGTERM,term);
	nfiles = eargc;
	if(!mflg && !cflg)
		sort();	/* closes each input, stdin included */
	for(a = mflg|cflg?0:eargc; a+N<nfiles || unsafeout&&a<eargc; a=i) {
		i = a+N;
		if(i>=nfiles)
			i = nfiles;
		newfile();
		merge(a, i);
	}
	if(a != nfiles) {
		oldfile();
		merge(a, nfiles);
	}
	exitstatus = 0;
	term();
}

sort()
{
	register char *cp;
	register char **lp;
	register c;
	int done;
	int i;
	char *f;
	char *partial;
	long carry;

	done = 0;
	i = 0;
	c = EOF;
	carry = 0;
	do {
		cp = tspace + carry;
		lp = (char **)lspace;
		while(lp < (char **)lspace+nlines && cp < tspace+ntext) {
			/* A line that did not fit last time continues at the start. */
			*lp++ = carry ? tspace : cp;
			carry = 0;
			while(c != '\n') {
				if(c != EOF) {
					/*
					 * V7 kept storing past the workspace; end the run
					 * before this line and carry it to the next one.
					 */
					if(cp >= tspace+ntext-1) {
						if(--lp == (char **)lspace) {
							diag("line too long","");
							term();
						}
						partial = *lp;
						carry = cp - partial;
						goto run;
					}
					*cp++ = c;
					c = getc(is);
					continue;
				} else if(is) {
					if(ferror(is)) {
						diag("read error","");
						term();
					}
					fclose(is);
					is = NULL;
				}
				if(i < eargc) {
					if((f = setfil(i++)) == 0)
						is = stdin;
					else if((is = fopen(f, "r")) == NULL)
						cant(f);
					c = getc(is);
				} else
					break;
			}
			*cp++ = '\n';
			if(c == EOF) {
				done++;
				/* Only an empty line is dropped; V7 lost an unterminated one. */
				if(cp-1 == lp[-1])
					lp--;
				break;
			}
			c = getc(is);
		}
run:
		sortq((char **)lspace, lp);
		if(done == 0 || nfiles != eargc)
			newfile();
		else
			oldfile();
		while(lp > (char **)lspace) {
			cp = *--lp;
			if(*cp)
				do
				putc(*cp, os);
				while(*cp++ != '\n');
		}
		closeout();
		if(carry)
			memmove(tspace, partial, carry);
	} while(done == 0);
}

/*
 * Merge lines grow on demand; V7 kept L (512) bytes and overwrote the
 * last byte of longer lines. The structures no longer live in lspace.
 */
struct merg
{
	char	*l;
	size_t	size;
	FILE	*b;
} *ibuf[256], mbuf[N+1];

merge(a,b)
{
	struct	merg	*p;
	register char	*cp, *dp;
	register	i;
	struct merg **ip, *jp;
	char	*f;
	int	j;
	int	k, l;
	int	muflg;

	p = mbuf;
	j = 0;
	for(i=a; i < b; i++) {
		f = setfil(i);
		if(f == 0)
			p->b = stdin;
		else if((p->b = fopen(f, "r")) == NULL)
			cant(f);
		ibuf[j] = p;
		if(!rline(p))	j++;
		p++;
	}

	do {
		i = j;
		msort(i);
		l = 0;
		while(i--) {
			cp = ibuf[i]->l;
			if(*cp == '\0') {
				l = 1;
				if(rline(ibuf[i])) {
					k = i;
					while(++k < j)
						ibuf[k-1] = ibuf[k];
					j--;
				}
			}
		}
	} while(l);

	muflg = mflg & uflg | cflg;
	i = j;
	if(i == 1)	/* V7 compared with ibuf[-1] */
		muflg |= uflg;
	while(i > 0) {
		cp = ibuf[i-1]->l;
		if(!cflg && (uflg == 0 || muflg ||
			(*compare)(ibuf[i-1]->l,ibuf[i-2]->l))) {
			/*
			 * Remember the last line written for -u: once one input
			 * remains, V7 compared with stale workspace and could drop
			 * a final duplicate's only copy.
			 */
			if(uflg && !muflg)
				copyline(p, cp);
			do
				putc(*cp, os);
			while(*cp++ != '\n');
		}
		if(muflg)
			copyline(p, ibuf[i-1]->l);
		for(;;) {
			if(rline(ibuf[i-1])) {
				i--;
				if(i == 0)
					break;
				if(i == 1)
					muflg = uflg;
			}
			ip = &ibuf[i];
			while(--ip>ibuf&&(*compare)(ip[0]->l,ip[-1]->l)<0){
				jp = *ip;
				*ip = *(ip-1);
				*(ip-1) = jp;
			}
			if(!muflg)
				break;
			j = (*compare)(ibuf[i-1]->l,p->l);
			if(cflg) {
				if(j > 0)
					disorder("disorder:",ibuf[i-1]->l);
				else if(uflg && j==0)
					disorder("nonunique:",ibuf[i-1]->l);
			} else if(j == 0)
				continue;
			break;
		}
	}
	p = mbuf;
	for(i=a; i<b; i++) {
		fclose(p->b);
		p++;
		if(i >= eargc)
			unlink(setfil(i));
	}
	closeout();
}

/* Order the merge heads by their lines with sortq, which may -u mark them. */
msort(n)
{
	char *lines[N+1];
	struct merg *m[N+1];
	int k, x;

	for(k=0; k<n; k++) {
		lines[k] = ibuf[k]->l;
		m[k] = ibuf[k];
	}
	sortq(lines, lines+n);
	for(k=0; k<n; k++)
		for(x=0; x<n; x++)
			if(m[x] && m[x]->l == lines[k]) {
				ibuf[k] = m[x];
				m[x] = 0;
				break;
			}
}

grow(mp, need)
struct merg *mp;
size_t need;
{
	size_t size = mp->size ? mp->size : L;
	char *l;

	while(size < need)
		size *= 2;
	if(size == mp->size)
		return;
	if((l = realloc(mp->l, size)) == NULL) {
		diag("out of memory","");
		term();
	}
	mp->l = l;
	mp->size = size;
}

copyline(mp, s)
struct merg *mp;
char *s;
{
	size_t n = eol(s) - s + 1;

	grow(mp, n);
	memcpy(mp->l, s, n);
}

rline(mp)
struct merg *mp;
{
	FILE *bp;
	register c;
	size_t n = 0;

	bp = mp->b;
	do {
		c = getc(bp);
		if(c == EOF) {
			if(ferror(bp)) {
				diag("read error","");
				term();
			}
			if(n == 0)
				return(1);
			c = '\n';	/* an unterminated final line still counts */
		}
		grow(mp, n+1);
		mp->l[n++] = c;
	} while(c!='\n');
	return(0);
}

disorder(s,t)
char *s, *t;
{
	register char *u;
	for(u=t; *u!='\n';u++) ;
	*u = 0;
	diag(s,t);
	term();
}

newfile()
{
	register char *f;
	int fd;

	/*
	 * The first name was created exclusively at startup; later ones are
	 * created exclusively here. Counting first lets term() remove a file
	 * an interrupt left behind.
	 */
	f = setfil(nfiles++);
	if(nfiles-1 == eargc)
		fd = open(f, O_WRONLY|O_TRUNC);
	else if((fd = open(f, O_WRONLY|O_CREAT|O_EXCL, 0600)) < 0)
		nfiles--;	/* not ours to remove */
	if(fd < 0 || (os = fdopen(fd, "w")) == NULL) {
		diag("can't create ",f);
		term();
	}
}

closeout()
{
	if(fflush(os) == EOF || ferror(os)) {
		diag("write error","");
		term();
	}
	fclose(os);
}

char *
setfil(i)
{

	if(i < eargc)
		if(eargv[i][0] == '-' && eargv[i][1] == '\0')
			return(0);
		else
			return(eargv[i]);
	i -= eargc;
	if(i >= 26*26*26) {
		diag("too many temporary files","");
		exit(1);
	}
	filep[0] = i/(26*26) + 'a';
	filep[1] = i/26%26 + 'a';
	filep[2] = i%26 + 'a';
	return(file);
}

oldfile()
{

	if(outfil) {
		if((os=fopen(outfil, "w")) == NULL) {
			diag("can't create ",outfil);
			term();
		}
	} else
		os = stdout;
}

safeoutfil()
{
	register int i;
	struct stat obuf,ibuf;

	if(!mflg||outfil==0)
		return;
	if(stat(outfil,&obuf)==-1)
		return;
	for(i=eargc>N?eargc-N:0;i<eargc;i++) {	/*-N is suff., not nec.*/
		if(stat(eargv[i],&ibuf)==-1)
			continue;
		if(obuf.st_dev==ibuf.st_dev&&
		   obuf.st_ino==ibuf.st_ino)
			unsafeout++;
	}
}

cant(f)
char *f;
{

	diag("can't open ",f);
	term();
}

diag(s,t)
char *s, *t;
{
	fputs("sort: ",stderr);
	fputs(s,stderr);
	fputs(t,stderr);
	fputs("\n",stderr);
}

term()
{
	register i;

	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	if(file == 0)
		exit(exitstatus);
	if(nfiles == eargc)
		nfiles++;	/* the startup name */
	for(i=eargc; i<nfiles; i++)
		unlink(setfil(i));
	exit(exitstatus);
}

cmp(i, j)
char *i, *j;
{
	register char *pa, *pb;
	char *skip();
	char *code, *ignore;
	int a, b;
	int k;
	char *la, *lb;
	register int sa;
	int sb;
	char *ipa, *ipb, *jpa, *jpb;
	struct field *fp;

	for(k = nfields>0; k<=nfields; k++) {
		fp = &fields[k];
		pa = i;
		pb = j;
		if(k) {
			la = skip(pa, fp, 1);
			pa = skip(pa, fp, 0);
			lb = skip(pb, fp, 1);
			pb = skip(pb, fp, 0);
		} else {
			la = eol(pa);
			lb = eol(pb);
		}
		if(fp->nflg) {
			while(blank(*pa))
				pa++;
			while(blank(*pb))
				pb++;
			sa = sb = fp->rflg;
			if(*pa == '-') {
				pa++;
				sa = -sa;
			}
			if(*pb == '-') {
				pb++;
				sb = -sb;
			}
			for(ipa = pa; ipa<la&&isdigit(*ipa); ipa++) ;
			for(ipb = pb; ipb<lb&&isdigit(*ipb); ipb++) ;
			jpa = ipa;
			jpb = ipb;
			a = 0;
			if(sa==sb)
				while(ipa > pa && ipb > pb)
					if(b = *--ipb - *--ipa)
						a = b;
			while(ipa > pa)
				if(*--ipa != '0')
					return(-sa);
			while(ipb > pb)
				if(*--ipb != '0')
					return(sb);
			if(a) return(a*sa);
			if(*(pa=jpa) == '.')
				pa++;
			if(*(pb=jpb) == '.')
				pb++;
			if(sa==sb)
				while(pa<la && isdigit(*pa)
				   && pb<lb && isdigit(*pb))
					if(a = *pb++ - *pa++)
						return(a*sa);
			while(pa<la && isdigit(*pa))
				if(*pa++ != '0')
					return(-sa);
			while(pb<lb && isdigit(*pb))
				if(*pb++ != '0')
					return(sb);
			continue;
		}
		code = fp->code;
		ignore = fp->ignore;
loop: 
		while(ignore[SC(*pa)])
			pa++;
		while(ignore[SC(*pb)])
			pb++;
		if(pa>=la || *pa=='\n')
			if(pb<lb && *pb!='\n')
				return(fp->rflg);
			else continue;
		if(pb>=lb || *pb=='\n')
			return(-fp->rflg);
		if((sa = code[SC(*pb++)]-code[SC(*pa++)]) == 0)
			goto loop;
		return(sa*fp->rflg);
	}
	if(uflg)
		return(0);
	return(cmpa(i, j));
}

cmpa(pa, pb)
register char *pa, *pb;
{
	while(*pa == *pb) {
		if(*pa++ == '\n')
			return(0);
		pb++;
	}
	return(
		*pa == '\n' ? fields[0].rflg:
		*pb == '\n' ?-fields[0].rflg:
		*pb > *pa   ? fields[0].rflg:
		-fields[0].rflg
	);
}

char *
skip(pp, fp, j)
struct field *fp;
char *pp;
{
	register i;
	register char *p;

	p = pp;
	if( (i=fp->m[j]) < 0)
		return(eol(p));
	while(i-- > 0) {
		if(tabchar != 0) {
			while(*p != tabchar)
				if(*p != '\n')
					p++;
				else goto ret;
			p++;
		} else {
			while(blank(*p))
				p++;
			while(!blank(*p))
				if(*p != '\n')
					p++;
				else goto ret;
		}
	}
	if(fp->bflg[j])
		while(blank(*p))
			p++;
	i = fp->n[j];
	while(i-- > 0) {
		if(*p != '\n')
			p++;
		else goto ret;
	} 
ret:
	return(p);
}

char *
eol(p)
register char *p;
{
	while(*p != '\n') p++;
	return(p);
}

copyproto()
{
	fields[nfields] = proto;	/* V7 copied the structure as ints */
}

field(s,k)
char *s;
{
	register struct field *p;
	register d;
	p = &fields[nfields];
	d = 0;
	for(; *s!=0; s++) {
		switch(*s) {
		case '\0':
			return;

		case 'b':
			p->bflg[k]++;
			break;

		case 'd':
			p->ignore = dict+128;
			break;

		case 'f':
			p->code = fold+128;
			break;
		case 'i':
			p->ignore = nonprint+128;
			break;

		case 'c':
			cflg = 1;
			continue;

		case 'm':
			mflg = 1;
			continue;

		case 'n':
			p->nflg++;
			break;
		case 't':
			tabchar = *++s;
			if(tabchar == 0) s--;
			continue;

		case 'r':
			p->rflg = -1;
			continue;
		case 'u':
			uflg = 1;
			break;

		case '.':
			if(p->m[k] == -1)	/* -m.n with m missing */
				p->m[k] = 0;
			d = 1;	/* V7 indexed m[] past its end to reach n[] */

		default:
			if(d)
				p->n[k] = number(&s);
			else
				p->m[k] = number(&s);
		}
		compare = cmp;
	}
}

number(ppa)
char **ppa;
{
	int n;
	register char *pa;
	pa = *ppa;
	n = 0;
	while(isdigit(*pa)) {
		if(n < 1000000)	/* clamp instead of overflowing */
			n = n*10 + *pa - '0';
		*ppa = pa++;
	}
	return(n);
}

blank(c)
{
	if(c==' ' || c=='\t')
		return(1);
	return(0);
}

#define qsexc(p,q) t= *p;*p= *q;*q=t
#define qstexc(p,q,r) t= *p;*p= *r;*r= *q;*q=t

sortq(a,l)
char **a, **l;
{
	register char **i, **j;
	char **k;
	char **lp, **hp;
	int c;
	char *t;
	unsigned n;



start:
	if((n=l-a) <= 1)
		return;


	n /= 2;
	hp = lp = a+n;
	i = a;
	j = l-1;


	for(;;) {
		if(i < lp) {
			if((c = (*compare)(*i, *lp)) == 0) {
				--lp;
				qsexc(i, lp);
				continue;
			}
			if(c < 0) {
				++i;
				continue;
			}
		}

loop:
		if(j > hp) {
			if((c = (*compare)(*hp, *j)) == 0) {
				++hp;
				qsexc(hp, j);
				goto loop;
			}
			if(c > 0) {
				if(i == lp) {
					++hp;
					qstexc(i, hp, j);
					i = ++lp;
					goto loop;
				}
				qsexc(i, j);
				--j;
				++i;
				continue;
			}
			--j;
			goto loop;
		}


		if(i == lp) {
			if(uflg)
				for(k=lp+1; k<=hp;) **k++ = '\0';
			if(lp-a >= l-hp) {
				sortq(hp+1, l);
				l = lp;
			} else {
				sortq(a, lp);
				a = hp+1;
			}
			goto start;
		}


		--lp;
		qstexc(j, lp, i);
		j = --hp;
	}
}

