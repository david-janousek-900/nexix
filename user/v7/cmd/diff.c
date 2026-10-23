#
/*	diff - differential file comparison
*
*	Uses an algorithm due to Harold Stone, which finds
*	a pair of longest identical subsequences in the two
*	files.
*
*	The major goal is to generate the match vector J.
*	J[i] is the index of the line in file1 corresponding
*	to line i file0. J[i] = 0 if there is no
*	such line in file1.
*
*	Lines are hashed so as to work in core. All potential
*	matches are located by sorting the lines of each file
*	on the hash (called value_____). In particular, this
*	collects the equivalence classes in file1 together.
*	Subroutine equiv____  replaces the value of each line in
*	file0 by the index of the first element of its 
*	matching equivalence in (the reordered) file1.
*	To save space equiv_____ squeezes file1 into a single
*	array member______ in which the equivalence classes
*	are simply concatenated, except that their first
*	members are flagged by changing sign.
*
*	Next the indices that point into member______ are unsorted_______   into
*	array class_____ according to the original order of file0.
*
*	The cleverness lies in routine stone______. This marches
*	through the lines of file0, developing a vector klist_____
*	of "k-candidates". At step i a k-candidate is a matched
*	pair of lines x,y (x in file0 y in file1) such that
*	there is a common subsequence of lenght k
*	between the first i lines of file0 and the first y 
*	lines of file1, but there is no such subsequence for
*	any smaller y. x is the earliest possible mate to y
*	that occurs in such a subsequence.
*
*	Whenever any of the members of the equivalence class of
*	lines in file1 matable to a line in file0 has serial number 
*	less than the y of some k-candidate, that k-candidate 
*	with the smallest such y is replaced. The new 
*	k-candidate is chained (via pred____) to the current
*	k-1 candidate so that the actual subsequence can
*	be recovered. When a member has serial number greater
*	that the y of all k-candidates, the klist is extended.
*	At the end, the longest subsequence is pulled out
*	and placed in the array J by unravel_______.
*
*	With J in hand, the matches there recorded are
*	check_____ed against reality to assure that no spurious
*	matches have crept in due to hashing. If they have,
*	they are broken, and "jackpot " is recorded--a harmless
*	matter except that a true match for a spuriously
*	mated line may now be unnecessarily reported as a change.
*
*	Much of the complexity of the program comes simply
*	from trying to minimize core utilization and
*	maximize the range of doable problems by dynamically
*	allocating what is needed and reusing what is not.
*	The core requirements for problems larger than somewhat
*	are (in words) 2*length(file0) + length(file1) +
*	3*(number of k-candidates installed),  typically about
*	6n words for files of length n. 
*/
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#define	prints(s)	fputs(s,stdout)

#define HALFLONG 16
#define low(x)	(x&((1L<<HALFLONG)-1))
#define high(x)	(x>>HALFLONG)
FILE *input[2];

struct cand {
	int x;
	int y;
	int pred;
} cand;
struct line {
	int serial;
	int value;
} *file[2], line;
int len[2];
struct line *sfile[2];	/*shortened by pruning common prefix and suffix*/
int slen[2];
int pref, suff;	/*length of prefix and suffix*/
int *class;	/*will be overlaid on file[0]*/
int *member;	/*will be overlaid on file[1]*/
int *klist;		/*will be overlaid on file[0] after class*/
struct cand *clist;	/* merely a free storage pot for candidates */
int clen = 0;
int *J;		/*will be overlaid on class*/
long *ixold;	/*will be overlaid on klist*/
long *ixnew;	/*will be overlaid on file[1]*/
int opt;	/* -1,0,1 = -e,normal,-f */
int status = 2;
int anychange = 0;
char *empty = "";
int bflag;

char *tempfile;	/*used when comparing against std input*/

done()
{
	if(tempfile)
		unlink(tempfile);
	exit(status);
}

char *talloc(n)
{
	register char *p;
	p = malloc((unsigned)n);
	if(p!=NULL)
		return(p);
	noroom();
}

char *ralloc(p,n)	/* V7 freed p first to compact; that is a use after free */
char *p;
{
	register char *q;
	q = realloc(p, (unsigned)n);
	if(q==NULL)
		noroom();
	return(q);
}

noroom()
{
	mesg("files too big, try -h\n",empty);
	done();
}

sort(a,n)	/*shellsort CACM #201*/
struct line *a;
{
	struct line w;
	register int j,m;
	int i, im;
	int k;
	/* Indices: V7 stepped pointers below the array and tested wraparound,
	   and used m uninitialized when n was 0. */
	m = 0;
	for(j=1;j<=n;j*= 2)
		m = 2*j - 1;
	for(m/=2;m!=0;m/=2) {
		k = n-m;
		for(j=1;j<=k;j++) {
			for(i = j; i > 0; i -= m) {
				im = i+m;
				if(a[im].value > a[i].value ||
				   a[im].value == a[i].value &&
				   a[im].serial > a[i].serial)
					break;
				w = a[i];
				a[i] = a[im];
				a[im] = w;
			}
		}
	}
}

unsort(f, l, b)
struct line *f;
int *b;
{
	register int *a;
	register int i;
	a = (int *)talloc((l+1)*sizeof(int));
	for(i=1;i<=l;i++)
		a[f[i].serial] = f[i].value;
	for(i=1;i<=l;i++)
		b[i] = a[i];
	free((char *)a);
}

filename(pa1, pa2)
char **pa1, **pa2;
{
	register char *a1, *b1, *a2;
	char buf[512];
	struct stat stbuf;
	int i, f;
	a1 = *pa1;
	a2 = *pa2;
	if(stat(a1,&stbuf)!=-1 && ((stbuf.st_mode&S_IFMT)==S_IFDIR)) {
		/* dir/basename, sized for both (V7: 100 bytes) */
		b1 = *pa1 = talloc(strlen(a1) + strlen(a2) + 2);
		while(*b1++ = *a1++) ;
		b1[-1] = '/';
		a1 = b1;
		while(*a1++ = *a2++)
			if(*a2 && *a2!='/' && a2[-1]=='/')
				a1 = b1;
	}
	else if(a1[0]=='-'&&a1[1]==0&&tempfile==0) {
		signal(SIGHUP,done);
		signal(SIGINT,done);
		signal(SIGPIPE,done);
		signal(SIGTERM,done);
		/* Exclusive: V7 created whatever the predictable name named. */
		for(i = 0; ; i++) {
			tempfile = talloc(16);
			strcpy(tempfile, "/tmp/dXXXXXX");
			if(mktemp(tempfile) == 0 || i > 100) {
				tempfile = 0;
				mesg("cannot create temporary",empty);
				done();
			}
			if((f=open(tempfile,O_WRONLY|O_CREAT|O_EXCL,0600)) >= 0)
				break;
			tempfile = 0;
		}
		*pa1 = tempfile;
		while((i=read(0,buf,512))>0)
			if(write(f,buf,i) != i) {
				mesg("cannot write ",tempfile);
				done();
			}
		close(f);
	}
}

prepare(i, arg)
char *arg;
{
	register struct line *p;
	register j;
	int h;
	if((input[i] = fopen(arg,"r")) == NULL){
		mesg("cannot open ", arg);
		done();
	}
	p = (struct line *)talloc(3*sizeof(line));
	for(j=0; readhash(input[i], &h);) {
		p = (struct line *)ralloc((char *)p,(++j+3)*sizeof(line));
		p[j].value = h;
	}
	len[i] = j;
	file[i] = p;
	fclose(input[i]);
}

prune()
{
	register i,j;
	for(pref=0;pref<len[0]&&pref<len[1]&&
		file[0][pref+1].value==file[1][pref+1].value;
		pref++ ) ;
	for(suff=0;suff<len[0]-pref&&suff<len[1]-pref&&
		file[0][len[0]-suff].value==file[1][len[1]-suff].value;
		suff++) ;
	for(j=0;j<2;j++) {
		sfile[j] = file[j]+pref;
		slen[j] = len[j]-pref-suff;
		for(i=0;i<=slen[j];i++)
			sfile[j][i].serial = i;
	}
}

equiv(a,n,b,m,c)
struct line *a, *b;
int *c;
{
	register int i, j;
	i = j = 1;
	while(i<=n && j<=m) {
		if(a[i].value <b[j].value)
			a[i++].value = 0;
		else if(a[i].value == b[j].value)
			a[i++].value = j;
		else
			j++;
	}
	while(i <= n)
		a[i++].value = 0;
	b[m+1].value = 0;
	j = 0;
	while(++j <= m) {
		c[j] = -b[j].serial;
		while(b[j+1].value == b[j].value) {
			j++;
			c[j] = b[j].serial;
		}
	}
	c[j] = -1;
}

main(argc, argv)
char **argv;
{
	register int k;
	char **args;

	args = argv;
	if(argc>3 && *argv[1]=='-') {
		argc--;
		argv++;
		for(k=1;argv[0][k];k++) {
			switch(argv[0][k]) {
			case 'e':
				opt = -1;
				break;
			case 'f':
				opt = 1;
				break;
			case 'b':
				bflag = 1;
				break;
			case 'h':
				execv("/usr/lib/diffh",args);
				mesg("cannot find diffh",empty);
				done();
			}
		}
	}
	if(argc!=3) {
		mesg("arg count",empty);
		done();
	}

	filename(&argv[1], &argv[2]);
	filename(&argv[2], &argv[1]);
	prepare(0, argv[1]);
	prepare(1, argv[2]);
	prune();
	sort(sfile[0],slen[0]);
	sort(sfile[1],slen[1]);

	member = (int *)file[1];
	equiv(sfile[0], slen[0], sfile[1], slen[1], member);
	member = (int *)ralloc((char *)member,(slen[1]+2)*sizeof(int));

	class = (int *)file[0];
	unsort(sfile[0], slen[0], class);
	class = (int *)ralloc((char *)class,(slen[0]+2)*sizeof(int));

	klist = (int *)talloc((slen[0]+2)*sizeof(int));
	clist = (struct cand *)talloc(sizeof(cand));
	k = stone(class, slen[0], member, klist);
	free((char *)member);
	free((char *)class);
	file[0] = file[1] = 0;	/* their storage was class and member */

	J = (int *)talloc((len[0]+2)*sizeof(int));
	unravel(klist[k]);
	free((char *)clist);
	free((char *)klist);

	ixold = (long *)talloc((len[0]+2)*sizeof(long));
	ixnew = (long *)talloc((len[1]+2)*sizeof(long));
	check(argv);
	output(argv);
	if(fflush(stdout) == EOF || ferror(stdout))
		done();
	status = anychange;
	done();
}

stone(a,n,b,c)
int *a;
int *b;
int *c;
{
	register int i, k,y;
	int j, l;
	int oldc, tc;
	int oldl;
	k = 0;
	c[0] = newcand(0,0,0);
	for(i=1; i<=n; i++) {
		j = a[i];
		if(j==0)
			continue;
		y = -b[j];
		oldl = 0;
		oldc = c[0];
		do {
			if(y <= clist[oldc].y)
				continue;
			l = search(c, k, y);
			if(l!=oldl+1)
				oldc = c[l-1];
			if(l<=k) {
				if(clist[c[l]].y <= y)
					continue;
				tc = c[l];
				c[l] = newcand(i,y,oldc);
				oldc = tc;
				oldl = l;
			} else {
				c[l] = newcand(i,y,oldc);
				k++;
				break;
			}
		} while((y=b[++j]) > 0);
	}
	return(k);
}

newcand(x,y,pred)
{
	register struct cand *q;
	clist = (struct cand *)ralloc((char *)clist,++clen*sizeof(cand));
	q = clist + clen -1;
	q->x = x;
	q->y = y;
	q->pred = pred;
	return(clen-1);
}

search(c, k, y)
int *c;
{
	register int i, j, l;
	int t;
	if(clist[c[k]].y<y)	/*quick look for typical case*/
		return(k+1);
	i = 0;
	j = k+1;
	while((l=(i+j)/2) > i) {
		t = clist[c[l]].y;
		if(t > y)
			j = l;
		else if(t < y)
			i = l;
		else
			return(l);
	}
	return(l+1);
}

unravel(p)
{
	register int i;
	register struct cand *q;
	for(i=0; i<=len[0]; i++)
		J[i] =	i<=pref ? i:
			i>len[0]-suff ? i+len[1]-len[0]:
			0;
	for(q=clist+p;q->y!=0;q=clist+q->pred)
		J[q->x+pref] = q->y+pref;
}

/* check does double duty:
1.  ferret out any fortuitous correspondences due
to confounding by hashing (which result in "jackpot")
2.  collect random access indexes to the two files */

check(argv)
char **argv;
{
	register int i, j;
	int jackpot;
	long ctold, ctnew;
	int c,d;	/* V7's char could not hold EOF */
	input[0] = fopen(argv[1],"r");
	input[1] = fopen(argv[2],"r");
	j = 1;
	ixold[0] = ixnew[0] = 0;
	jackpot = 0;
	ctold = ctnew = 0;
	for(i=1;i<=len[0];i++) {
		if(J[i]==0) {
			ixold[i] = ctold += skipline(0);
			continue;
		}
		while(j<J[i]) {
			ixnew[j] = ctnew += skipline(1);
			j++;
		}
		for(;;) {
			/* A final line without a newline ends at EOF instead. */
			if((c = getc(input[0])) == EOF)
				c = '\n';
			if((d = getc(input[1])) == EOF)
				d = '\n';
			ctold++;
			ctnew++;
			if(bflag && isspace(c) && isspace(d)) {
				do {
					if(c=='\n') break;
					ctold++;
				} while(isspace(c=getc(input[0])));
				if(c == EOF)
					c = '\n';
				do {
					if(d=='\n') break;
					ctnew++;
				} while(isspace(d=getc(input[1])));
				if(d == EOF)
					d = '\n';
			}
			if(c!=d) {
				jackpot++;
				J[i] = 0;
				if(c!='\n')
					ctold += skipline(0);
				if(d!='\n')
					ctnew += skipline(1);
				break;
			}
			if(c=='\n')
				break;
		}
		ixold[i] = ctold;
		ixnew[j] = ctnew;
		j++;
	}
	for(;j<=len[1];j++) {
		ixnew[j] = ctnew += skipline(1);
	}
	fclose(input[0]);
	fclose(input[1]);
/*
	if(jackpot)
		mesg("jackpot",empty);
*/
}

skipline(f)
{
	register i, c;
	/* An unterminated final line counts its missing newline (V7 looped). */
	for(i=1;(c = getc(input[f]))!='\n' && c != EOF;i++) ;
	return(i);
}

output(argv)
char **argv;
{
	int m;
	register int i0, i1, j1;
	int j0;
	input[0] = fopen(argv[1],"r");
	input[1] = fopen(argv[2],"r");
	m = len[0];
	J[0] = 0;
	J[m+1] = len[1]+1;
	if(opt!=-1) for(i0=1;i0<=m;i0=i1+1) {
		while(i0<=m&&J[i0]==J[i0-1]+1) i0++;
		j0 = J[i0-1]+1;
		i1 = i0-1;
		while(i1<m&&J[i1+1]==0) i1++;
		j1 = J[i1+1]-1;
		J[i1] = j1;
		change(i0,i1,j0,j1);
	} else for(i0=m;i0>=1;i0=i1-1) {
		while(i0>=1&&J[i0]==J[i0+1]-1&&J[i0]!=0) i0--;
		j0 = J[i0+1]-1;
		i1 = i0+1;
		while(i1>1&&J[i1-1]==0) i1--;
		j1 = J[i1-1]+1;
		J[i1] = j1;
		change(i1,i0,j1,j0);
	}
	if(m==0)
		change(1,0,1,len[1]);
}

change(a,b,c,d)
{
	if(a>b&&c>d) return;
	anychange = 1;
	if(opt!=1) {
		range(a,b,",");
		putchar(a>b?'a':c>d?'d':'c');
		if(opt!=-1) range(c,d,",");
	} else {
		putchar(a>b?'a':c>d?'d':'c');
		range(a,b," ");
	}
	putchar('\n');
	if(opt==0) {
		fetch(ixold,a,b,input[0],"< ");
		if(a<=b&&c<=d) prints("---\n");
	}
	fetch(ixnew,c,d,input[1],opt==0?"> ":empty);
	if(opt!=0&&c<=d) prints(".\n");
}

range(a,b,separator)
char *separator;
{
	printf("%d", a>b?b:a);
	if(a<b) {
		printf("%s%d", separator, b);
	}
}

fetch(f,a,b,lb,s)
long *f;
FILE *lb;
char *s;
{
	register int i, j;
	register int nc;
	register int c;
	for(i=a;i<=b;i++) {
		fseek(lb,f[i-1],0);
		nc = f[i]-f[i-1];
		prints(s);
		for(j=0;j<nc;j++)
			putchar((c = getc(lb)) == EOF ? '\n' : c);
	}
}

/* hashing has the effect of
 * arranging line in 7-bit bytes and then
 * summing 1-s complement in 16-bit hunks 
*/

readhash(f, hp)	/* 0 at end of file: a hash may itself be 0 (V7 stopped there) */
FILE *f;
int *hp;
{
	long sum;
	register unsigned shift;
	register space;
	register t;
	int any = 0;
	sum = 1;
	space = 0;
	/* A final line without a newline is hashed as if it had one (V7 dropped it). */
	if(!bflag) for(shift=0;(t=getc(f))!='\n';shift+=7) {
		if(t==-1) {
			if(!any)
				return(0);
			break;
		}
		any = 1;
		sum += (long)t << (shift%=HALFLONG);
	}
	else for(shift=0;;) {
		switch(t=getc(f)) {
		case -1:
			if(!any)
				return(0);
			break;
		case '\t':
		case ' ':
			any = 1;
			space++;
			continue;
		default:
			any = 1;
			if(space) {
				shift += 7;
				space = 0;
			}
			sum += (long)t << (shift%=HALFLONG);
			shift += 7;
			continue;
		case '\n':
			break;
		}
		break;
	}
	sum = low(sum) + high(sum);
	*hp = (short)low(sum) + (short)high(sum);
	return(1);
}

mesg(s,t)
char *s, *t;
{
	fprintf(stderr,"diff: %s%s\n",s,t);
}
