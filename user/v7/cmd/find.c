/*	find	COMPILE:	cc -o find -s -O -i find.c -lS	*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#define A_DAY	86400L /* a day full of seconds */
#define EQ(x, y)	(strcmp(x, y)==0)

int	Randlast;
#define	PATHSIZE	1024	/* V7: 200, unchecked */
char	Pathname[PATHSIZE];

/*
 * V7 stored predicate operands in the pointer fields and read them back
 * through overlaid int structures, which assumed 16-bit pointers.
 */
#define	NNODES	100
struct anode {
	int (*F)();
	struct anode *L, *R;
	long	N;	/* numeric operand */
	int	S;	/* '+', '-' or other sign character */
	char	*P;	/* name pattern */
} Node[NNODES];
int Nn;  /* number of nodes */
char	*Fname;
long	Now;
int	Argc,
	Ai,
	Pi;
char	**Argv;
/* cpio stuff */
int	Cpio;
short	*Buf, *Dbuf, *Wp;
int	Bufsize = 5120;
int	Wct = 2560;

long	Newer;

struct stat Statb;

struct	anode	*oexp(),
		*e1(),
		*e2(),
		*e3(),
		*mk(),
		*mkp();
char	*nxtarg();
char	Home[PATHSIZE];
long	Blocks;
main(argc, argv) char *argv[];
{
	struct anode *exlist;
	int paths;
	register char *cp, *sp = 0;
	FILE *pwd, *popen();

	time(&Now);
	pwd = popen("pwd", "r");
	if(pwd == NULL || fgets(Home, sizeof Home, pwd) == NULL
	|| Home[0] != '/' || Home[strlen(Home) - 1] != '\n') {
		pr("find: cannot determine current directory\n");
		exit(2);
	}
	pclose(pwd);
	Home[strlen(Home) - 1] = '\0';
	Argc = argc; Argv = argv;
	if(argc<3) {
usage:		pr("Usage: find path-list predicate-list\n");
		exit(1);
	}
	for(Ai = paths = 1; Ai < (argc-1); ++Ai, ++paths)
		if(*Argv[Ai] == '-' || EQ(Argv[Ai], "(") || EQ(Argv[Ai], "!"))
			break;
	if(paths == 1) /* no path-list */
		goto usage;
	if(!(exlist = oexp())) { /* parse and compile the arguments */
		pr("find: parsing error\n");
		exit(1);
	}
	if(Ai<argc) {
		pr("find: missing conjunction\n");
		exit(1);
	}
	for(Pi = 1; Pi < paths; ++Pi) {
		sp = 0;
		chdir(Home);
		if(strlen(Argv[Pi]) >= PATHSIZE) {
			pr("find: pathname too long\n");
			exit(2);
		}
		strcpy(Pathname, Argv[Pi]);
		if(cp = rindex(Pathname, '/')) {
			sp = cp + 1;
			*cp = '\0';
			if(chdir(*Pathname? Pathname: "/") == -1) {
				pr("find: bad starting directory\n");
				exit(2);
			}
			*cp = '/';
		}
		/* V7's kernel read an empty name ("/", "dir/") as "."; ours does not. */
		Fname = sp? (*sp? sp: "."): Pathname;
		descend(Pathname, Fname, exlist); /* to find files that match  */
	}
	if(Cpio) {
		strcpy(Pathname, "TRAILER!!!");
		Statb.st_size = 0;
		cpio();
	}
	if(fflush(stdout) == EOF || ferror(stdout))
		exit(1);
	exit(0);
}

/* compile time functions:  priority is  exp()<e1()<e2()<e3()  */

struct anode *oexp() { /* parse ALTERNATION (-o)  */
	int or();
	register struct anode * p1;

	p1 = e1() /* get left operand */ ;
	if(EQ(nxtarg(), "-o")) {
		Randlast--;
		return(mk(or, p1, oexp()));
	}
	else if(Ai <= Argc) --Ai;
	return(p1);
}
struct anode *e1() { /* parse CONCATENATION (formerly -a) */
	int and();
	register struct anode * p1;
	register char *a;

	p1 = e2();
	a = nxtarg();
	if(EQ(a, "-a")) {
And:
		Randlast--;
		return(mk(and, p1, e1()));
	} else if(EQ(a, "(") || EQ(a, "!") || (*a=='-' && !EQ(a, "-o"))) {
		--Ai;
		goto And;
	} else if(Ai <= Argc) --Ai;
	return(p1);
}
struct anode *e2() { /* parse NOT (!) */
	int not();

	if(Randlast) {
		pr("find: operand follows operand\n");
		exit(1);
	}
	Randlast++;
	if(EQ(nxtarg(), "!"))
		return(mk(not, e3(), (struct anode *)0));
	else if(Ai <= Argc) --Ai;
	return(e3());
}
struct anode *e3() { /* parse parens and predicates */
	int exeq(), ok(), glob(),  mtime(), atime(), ctim(), user(),
		group(), size(), perm(), links(), print(),
		type(), ino(), cpio(), newer();
	struct anode *p1;
	int i;
	register char *a, *b, s;

	a = nxtarg();
	if(EQ(a, "(")) {
		Randlast--;
		p1 = oexp();
		a = nxtarg();
		if(!EQ(a, ")")) goto err;
		return(p1);
	}
	else if(EQ(a, "-print")) {
		return(mk(print, (struct anode *)0, (struct anode *)0));
	}
	b = nxtarg();
	s = *b;
	if(s=='+') b++;
	if(EQ(a, "-name"))
		return(mkp(glob, 0L, 0, b));
	else if(EQ(a, "-mtime"))
		return(mkp(mtime, (long)atoi(b), s, (char *)0));
	else if(EQ(a, "-atime"))
		return(mkp(atime, (long)atoi(b), s, (char *)0));
	else if(EQ(a, "-ctime"))
		return(mkp(ctim, (long)atoi(b), s, (char *)0));
	else if(EQ(a, "-user")) {
		if((i=getunum("/etc/passwd", b)) == -1) {
			if(gmatch(b, "[0-9][0-9][0-9]*")
			|| gmatch(b, "[0-9][0-9]")
			|| gmatch(b, "[0-9]"))
				return mkp(user, (long)atoi(b), s, (char *)0);
			pr("find: cannot find -user name\n");
			exit(1);
		}
		return(mkp(user, (long)i, s, (char *)0));
	}
	else if(EQ(a, "-inum"))
		return(mkp(ino, (long)atoi(b), s, (char *)0));
	else if(EQ(a, "-group")) {
		if((i=getunum("/etc/group", b)) == -1) {
			if(gmatch(b, "[0-9][0-9][0-9]*")
			|| gmatch(b, "[0-9][0-9]")
			|| gmatch(b, "[0-9]"))
				return mkp(group, (long)atoi(b), s, (char *)0);
			pr("find: cannot find -group name\n");
			exit(1);
		}
		return(mkp(group, (long)i, s, (char *)0));
	} else if(EQ(a, "-size"))
		return(mkp(size, (long)atoi(b), s, (char *)0));
	else if(EQ(a, "-links"))
		return(mkp(links, (long)atoi(b), s, (char *)0));
	else if(EQ(a, "-perm")) {
		for(i=0; *b ; ++b) {
			if(*b=='-') continue;
			if(*b < '0' || *b > '7' || i > 07777)
				goto err;	/* V7 accepted any digits */
			i <<= 3;
			i = i + (*b - '0');
		}
		return(mkp(perm, (long)i, s, (char *)0));
	}
	else if(EQ(a, "-type")) {
		i = s=='d' ? S_IFDIR :
		    s=='b' ? S_IFBLK :
		    s=='c' ? S_IFCHR :
		    s=='f' ? 0100000 :
		    0;
		return(mkp(type, (long)i, 0, (char *)0));
	}
	else if (EQ(a, "-exec")) {
		i = Ai - 1;
		while(!EQ(nxtarg(), ";"));
		return(mkp(exeq, (long)i, 0, (char *)0));
	}
	else if (EQ(a, "-ok")) {
		i = Ai - 1;
		while(!EQ(nxtarg(), ";"));
		return(mkp(ok, (long)i, 0, (char *)0));
	}
	else if(EQ(a, "-cpio")) {
		if((Cpio = creat(b, 0666)) < 0) {
			pr("find: cannot create "), pr(b), pr("\n");	/* V7: pr(s) */
			exit(1);
		}
		Buf = (short *)calloc(1, 512);
		Wp = Dbuf = (short *)malloc(5120);
		if(Buf == 0 || Dbuf == 0) {
			pr("find: out of memory\n");
			exit(1);
		}
		return(mk(cpio, (struct anode *)0, (struct anode *)0));
	}
	else if(EQ(a, "-newer")) {
		if(stat(b, &Statb) < 0) {
			pr("find: cannot access "), pr(b), pr("\n");
			exit(1);
		}
		Newer = Statb.st_mtime;
		return mk(newer, (struct anode *)0, (struct anode *)0);
	}
err:	pr("find: bad option "), pr(a), pr("\n");
	exit(1);
}
struct anode *mk(f, l, r)
int (*f)();
struct anode *l, *r;
{
	if(Nn >= NNODES) {
		pr("find: expression too long\n");
		exit(1);
	}
	Node[Nn].F = f;
	Node[Nn].L = l;
	Node[Nn].R = r;
	return(&(Node[Nn++]));
}

struct anode *mkp(f, n, s, pat)
int (*f)();
long n;
char *pat;
{
	struct anode *p = mk(f, (struct anode *)0, (struct anode *)0);

	p->N = n;
	p->S = s;
	p->P = pat;
	return(p);
}

char *nxtarg() { /* get next arg from command line */
	static strikes = 0;

	if(strikes==3) {
		pr("find: incomplete statement\n");
		exit(1);
	}
	if(Ai>=Argc) {
		strikes++;
		Ai = Argc + 1;
		return("");
	}
	return(Argv[Ai++]);
}

/* execution time functions */
and(p)
register struct anode *p;
{
	return(((*p->L->F)(p->L)) && ((*p->R->F)(p->R))?1:0);
}
or(p)
register struct anode *p;
{
	 return(((*p->L->F)(p->L)) || ((*p->R->F)(p->R))?1:0);
}
not(p)
register struct anode *p;
{
	return( !((*p->L->F)(p->L)));
}
glob(p)
register struct anode *p;
{
	return(gmatch(Fname, p->P));
}
print()
{
	puts(Pathname);
	return(1);
}
mtime(p)
register struct anode *p;
{
	return(scomp((int)((Now - Statb.st_mtime) / A_DAY), (int)p->N, p->S));
}
atime(p)
register struct anode *p;
{
	return(scomp((int)((Now - Statb.st_atime) / A_DAY), (int)p->N, p->S));
}
ctim(p)
register struct anode *p;
{
	return(scomp((int)((Now - Statb.st_ctime) / A_DAY), (int)p->N, p->S));
}
user(p)
register struct anode *p;
{
	return(scomp(Statb.st_uid, (int)p->N, p->S));
}
ino(p)
register struct anode *p;
{
	return(scomp((int)Statb.st_ino, (int)p->N, p->S));
}
group(p)
register struct anode *p;
{
	return(p->N == Statb.st_gid);
}
links(p)
register struct anode *p;
{
	return(scomp(Statb.st_nlink, (int)p->N, p->S));
}
size(p)
register struct anode *p;
{
	return(scomp((int)((Statb.st_size+511)>>9), (int)p->N, p->S));
}
perm(p)
register struct anode *p;
{
	register i;
	i = (p->S=='-') ? p->N : 07777; /* '-' means only arg bits */
	return((Statb.st_mode & i & 07777) == p->N);
}
type(p)
register struct anode *p;
{
	return((Statb.st_mode&S_IFMT)==p->N);
}
exeq(p)
register struct anode *p;
{
	fflush(stdout); /* to flush possible `-print' */
	return(doex((int)p->N));
}
ok(p)
struct anode *p;
{
	int c;  int yes;
	yes = 0;
	fflush(stdout); /* to flush possible `-print' */
	pr("< "), pr(Argv[p->N]), pr(" ... "), pr(Pathname), pr(" >?   ");
	fflush(stderr);
	if((c=getchar())=='y') yes = 1;
	while(c!='\n')
		if(c==EOF)
			exit(2);
		else
			c = getchar();
	if(yes) return(doex((int)p->N));
	return(0);
}

#define MKSHORT(v, lv) {U.l=1L;if(U.c[0]) U.l=lv, v[0]=U.s[1], v[1]=U.s[0]; else U.l=lv, v[0]=U.s[0], v[1]=U.s[1];}
union { long l; short s[2]; char c[4]; } U;
long mklong(v)
short v[];
{
	U.l = 1;
	if(U.c[0] /* VAX */)
		U.s[0] = v[1], U.s[1] = v[0];
	else
		U.s[0] = v[0], U.s[1] = v[1];
	return U.l;
}
cpio()
{
#define MAGIC 070707
	struct header {
		short	h_magic,
			h_dev,
			h_ino,
			h_mode,
			h_uid,
			h_gid,
			h_nlink,
			h_rdev;
		short	h_mtime[2];
		short	h_namesize;
		short	h_filesize[2];
		char	h_name[256];
	} hdr;
	register ifile, ct;
	static long fsz;
	register i;

	hdr.h_magic = MAGIC;
	if(strlen(!strncmp(Pathname, "./", 2)? Pathname+2: Pathname) >= sizeof hdr.h_name) {
		pr("find: name too long for cpio "), pr(Pathname), pr("\n");
		return;	/* V7 overran the 256-byte header name */
	}
	strcpy(hdr.h_name, !strncmp(Pathname, "./", 2)? Pathname+2: Pathname);
	hdr.h_namesize = strlen(hdr.h_name) + 1;
	hdr.h_uid = Statb.st_uid;
	hdr.h_gid = Statb.st_gid;
	hdr.h_dev = Statb.st_dev;
	hdr.h_ino = Statb.st_ino;
	hdr.h_mode = Statb.st_mode;
	MKSHORT(hdr.h_mtime, Statb.st_mtime);
	hdr.h_nlink = Statb.st_nlink;
	fsz = hdr.h_mode & S_IFREG? Statb.st_size: 0L;
	MKSHORT(hdr.h_filesize, fsz);
	hdr.h_rdev = Statb.st_rdev;
	if(EQ(hdr.h_name, "TRAILER!!!")) {
		bwrite((short *)&hdr, (sizeof hdr-256)+hdr.h_namesize);
		for(i = 0; i < 10; ++i)
			bwrite(Buf, 512);
		return;
	}
	if(!mklong(hdr.h_filesize)) {
		bwrite((short *)&hdr, (sizeof hdr-256)+hdr.h_namesize);
		return;
	}
	if((ifile = open(Fname, 0)) < 0) {
cerror:
		pr("find: cannot copy "), pr(hdr.h_name), pr("\n");
		return;
	}
	bwrite((short *)&hdr, (sizeof hdr-256)+hdr.h_namesize);
	for(fsz = mklong(hdr.h_filesize); fsz > 0; fsz -= 512) {
		ct = fsz>512? 512: fsz;
		/* A short read is padded so the archive keeps its stated size. */
		if((i = read(ifile, (char *)Buf, ct)) < 0) {
			close(ifile);
			goto cerror;
		}
		if(i < ct)
			memset((char *)Buf + i, 0, ct - i);
		bwrite(Buf, ct);
	}
	memset(Buf, 0, 512);
	close(ifile);
	return;
}
newer()
{
	return Statb.st_mtime > Newer;
}

/* support functions */
scomp(a, b, s) /* funny signed compare */
register a, b;
register char s;
{
	if(s == '+')
		return(a > b);
	if(s == '-')
		return(a < (b * -1));
	return(a == b);
}

doex(com)
{
	register np;
	register char *na;
	static char *nargv[50];
	static int ccode;
	int pid;

	ccode = np = 0;
	while (na=Argv[com++]) {
		if(strcmp(na, ";")==0) break;
		if(np >= 49) {
			pr("find: too many -exec arguments\n");
			exit(1);
		}
		if(strcmp(na, "{}")==0) nargv[np++] = Pathname;
		else nargv[np++] = na;
	}
	nargv[np] = 0;
	if (np==0) return(9);
	fflush(stderr);
	if((pid = fork()) < 0) {
		pr("find: cannot fork\n");
		return(0);
	}
	if(pid) /*parent*/ {
		while(wait(&ccode) != pid)
			;
	} else { /*child*/
		chdir(Home);
		execvp(nargv[0], nargv);
		_exit(1);	/* not exit: the parent's stdio buffers are not ours */
	}
	return(ccode ? 0:1);
}

getunum(f, s) char *f, *s; { /* find user/group name and return number */
	register i;
	register char *sp;
	register c;
	char str[20];
	FILE *pin;

	/* Fields are bounded; V7 overran str and used a null stream. */
	i = -1;
	if((pin = fopen(f, "r")) == NULL)
		return(-1);
	c = '\n'; /* prime with a CR */
	do {
		if(c=='\n') {
			sp = str;
			while((c = getc(pin)) != ':') {
				if(c == EOF) goto RET;
				if(c == '\n') goto nextline;
				if(sp < &str[sizeof str - 1]) *sp++ = c;
			}
			*sp = '\0';
			if(EQ(str, s)) {
				while((c=getc(pin)) != ':')
					if(c == EOF || c == '\n') goto RET;
				sp = str;
				while((c = getc(pin)) != ':' && c != EOF && c != '\n')
					if(sp < &str[sizeof str - 1]) *sp++ = c;
				*sp = '\0';
				i = atoi(str);
				goto RET;
			}
		}
		continue;
	nextline:
		c = '\n';
		continue;
	} while((c = getc(pin)) != EOF);
 RET:
	fclose(pin);
	return(i);
}

descend(name, fname, exlist)
struct anode *exlist;
char *name, *fname;
{
	int	dir = 0, /* open directory */
		offset,
		dsize,
		entries,
		dirsize;
	struct direct dentry[32];
	register struct direct	*dp;
	register char *c1, *c2;
	int i;
	int rv = 0;
	char *endofname;

	if(stat(fname, &Statb)<0) {
		pr("find: bad status-- "), pr(name), pr("\n");
		return(0);
	}
	(*exlist->F)(exlist);
	if((Statb.st_mode&S_IFMT)!=S_IFDIR)
		return(1);

	for(c1 = name; *c1; ++c1);
	if(c1 > name && *(c1-1) == '/')
		--c1;
	endofname = c1;
	dirsize = Statb.st_size;

	if(chdir(fname) == -1)
		return(0);
	for(offset=0 ; offset < dirsize ; offset += 512) { /* each block */
		dsize = 512<(dirsize-offset)? 512: (dirsize-offset);
		if(!dir) {
			if((dir=open(".", 0))<0) {
				pr("find: cannot open "), pr(name), pr("\n");
				rv = 0;
				goto ret;
			}
			if(offset) lseek(dir, (long)offset, 0);
			if(read(dir, (char *)dentry, dsize)<0) {
				pr("find: cannot read "), pr(name), pr("\n");
				rv = 0;
				goto ret;
			}
			if(dir > 10) {
				close(dir);
				dir = 0;
			}
		} else 
			if(read(dir, (char *)dentry, dsize)<0) {
				pr("find: cannot read "), pr(name), pr("\n");
				rv = 0;
				goto ret;
			}
		for(dp=dentry, entries=dsize>>4; entries; --entries, ++dp) { /* each directory entry */
			if(dp->d_ino==0
			|| (dp->d_name[0]=='.' && dp->d_name[1]=='\0')
			|| (dp->d_name[0]=='.' && dp->d_name[1]=='.' && dp->d_name[2]=='\0'))
				continue;
			if(endofname + 1 + DIRSIZ >= &Pathname[PATHSIZE]) {
				*endofname = '\0';
				pr("find: pathname too long "), pr(name), pr("\n");
				continue;
			}
			c1 = endofname;
			*c1++ = '/';
			c2 = dp->d_name;
			for(i=0; i<14; ++i)
				if(*c2)
					*c1++ = *c2++;
				else
					break;
			*c1 = '\0';
			if(c1 == endofname) { /* ?? */
				rv = 0;
				goto ret;
			}
			Fname = endofname+1;
			if(!descend(name, Fname, exlist)) {
				*endofname = '\0';
				chdir(Home);
				if(chdir(Pathname) == -1) {
					pr("find: bad directory tree\n");
					exit(1);
				}
			}
		}
	}
	rv = 1;
ret:
	if(dir)
		close(dir);
	if(chdir("..") == -1) {
		*endofname = '\0';
		pr("find: bad directory "), pr(name), pr("\n");
		rv = 1;
	}
	return(rv);
}

gmatch(s, p) /* string match as in glob */
register char *s, *p;
{
	if (*s=='.' && *p!='.') return(0);
	return amatch(s, p);
}

amatch(s, p)
register char *s, *p;
{
	register cc;
	int scc, k;
	int c, lc;

	scc = *s;
	lc = 077777;
	switch (c = *p) {

	case '[':
		k = 0;
		while (cc = *++p) {
			switch (cc) {

			case ']':
				if (k)
					return(amatch(++s, ++p));
				else
					return(0);

			case '-':
				k |= lc <= scc & scc <= (cc=p[1]);
			}
			if (scc==(lc=cc)) k++;
		}
		return(0);

	case '?':
	caseq:
		if(scc) return(amatch(++s, ++p));
		return(0);
	case '*':
		return(umatch(s, ++p));
	case 0:
		return(!scc);
	}
	if (c==scc) goto caseq;
	return(0);
}

umatch(s, p)
register char *s, *p;
{
	if(*p==0) return(1);
	while(*s)
		if (amatch(s++, p)) return(1);
	return(0);
}

bwrite(rp, c)
register short *rp;
register c;
{
	register short *wp = Wp;

	c = (c+1) >> 1;
	while(c--) {
		if(!Wct) {
again:
			if(write(Cpio, (char *)Dbuf, Bufsize)<0) {
				Cpio = chgreel(1, Cpio);
				goto again;
			}
			Wct = Bufsize >> 1;
			wp = Dbuf;
			++Blocks;
		}
		*wp++ = *rp++;
		--Wct;
	}
	Wp = wp;
}
chgreel(x, fl)
{
	register f;
	char str[22];
	FILE *devtty;
	struct stat statb;

	pr("find: can't "), pr(x? "write output": "read input"), pr("\n");
	fstat(fl, &statb);
	if((statb.st_mode&S_IFMT) != S_IFCHR)
		exit(1);
again:
	pr("If you want to go on, type device/file name when ready\n");
	devtty = fopen("/dev/tty", "r");
	fgets(str, 20, devtty);
	str[strlen(str) - 1] = '\0';
	if(!*str)
		exit(1);
	close(fl);
	if((f = open(str, x? 1: 0)) < 0) {
		pr("That didn't work");
		fclose(devtty);
		goto again;
	}
	return f;
}
pr(s)
char *s;
{
	fputs(s, stderr);
}
