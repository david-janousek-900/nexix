/*
 * list file or directory
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>

#define	NFILES	1024
FILE	*dirf;
char	stdbuf[BUFSIZ];

struct lbuf {
	union {
		char	lname[15];
		char	*namep;
	} ln;
	char	ltype;
	short	lnum;
	short	lflags;
	short	lnl;
	unsigned short	luid;
	unsigned short	lgid;
	long	lsize;
	long	lmtime;
};

int	aflg, dflg, lflg, sflg, tflg, uflg, iflg, fflg, gflg, cflg;
int	oneflg, name_pending;
int	rflg	= 1;
long	year;
int	flags;
long	tblocks;
int	statreq;
struct	lbuf	*flist[NFILES];
struct	lbuf	**lastp = flist;
struct	lbuf	**firstp = flist;
char	*dotp	= ".";

char	*makename();
struct	lbuf *gstat();
char	*ctime();
long	nblock();

#define	ISARG	0100000

main(argc, argv)
char *argv[];
{
	int i;
	register struct lbuf *ep, **ep1;
	register struct lbuf **slastp;
	struct lbuf **epp;
	struct lbuf lb;
	char *t;
	int compar();

	setbuf(stdout, stdbuf);
	time(&lb.lmtime);
	year = lb.lmtime - 6L*30L*24L*60L*60L; /* 6 months ago */
	if (--argc > 0 && *argv[1] == '-') {
		argv++;
		while (*++*argv) switch (**argv) {

		case '1':
			oneflg++;
			continue;

		case 'a':
			aflg++;
			continue;

		case 's':
			sflg++;
			statreq++;
			continue;

		case 'd':
			dflg++;
			continue;

		case 'g':
			gflg++;
			continue;

		case 'l':
			lflg++;
			statreq++;
			continue;

		case 'r':
			rflg = -1;
			continue;

		case 't':
			tflg++;
			statreq++;
			continue;

		case 'u':
			uflg++;
			continue;

		case 'c':
			cflg++;
			continue;

		case 'i':
			iflg++;
			continue;

		case 'f':
			fflg++;
			continue;

		default:
			continue;
		}
		argc--;
	}
	if (fflg) {
		aflg++;
		lflg = 0;
		sflg = 0;
		tflg = 0;
		statreq = 0;
	}
	if (argc==0) {
		argc++;
		argv = &dotp - 1;
	}
	for (i=0; i < argc; i++) {
		if ((ep = gstat(*++argv, 1))==NULL)
			continue;
		ep->ln.namep = *argv;
		ep->lflags |= ISARG;
	}
	qsort(firstp, lastp - firstp, sizeof *lastp, compar);
	slastp = lastp;
	for (epp=firstp; epp<slastp; epp++) {
		ep = *epp;
		if (ep->ltype=='d' && dflg==0 || fflg) {
			finish_names();
			if (argc>1)
				printf("\n%s:\n", ep->ln.namep);
			lastp = slastp;
			readdir(ep->ln.namep);
			if (fflg==0)
				qsort(slastp,lastp - slastp,sizeof *lastp,compar);
			if (lflg || sflg)
				printf("total %D\n", tblocks);
			for (ep1=slastp; ep1<lastp; ep1++)
				pentry(*ep1);
			finish_names();
		} else 
			pentry(ep);
	}
	finish_names();
	exit(0);
}

/* Finish each directory/operand group without trailing separators. */
finish_names()
{
	if (name_pending) putchar('\n');
	name_pending = 0;
}

pentry(ap)
struct lbuf *ap;
{
	struct { char dminor, dmajor;};
	struct passwd *owner;
	struct group *group;
	register struct lbuf *p;
	register char *cp;

	p = ap;
	if (p->lnum == -1)
		return;
	if (!lflg && !sflg && !iflg && !oneflg && name_pending)
		putchar(' ');
	if (iflg)
		printf("%5u ", p->lnum);
	if (sflg)
	printf("%4D ", nblock(p->lsize));
	if (lflg) {
		putchar(p->ltype);
		pmode(p->lflags);
		printf("%2d ", p->lnl);
		/* -g retains V7's group-only long listing; -l shows both. */
		if (!gflg) {
			owner = getpwuid(p->luid);
			if (owner) printf("%-8s ", owner->pw_name);
			else printf("%-8u ", (unsigned)p->luid);
		}
		group = getgrgid(p->lgid);
		if (group) printf("%-8s ", group->gr_name);
		else printf("%-8u ", (unsigned)p->lgid);
		if (p->ltype=='b' || p->ltype=='c')
			printf("%3d,%3d", major((int)p->lsize), minor((int)p->lsize));
		else
			printf("%7ld", p->lsize);
		cp = ctime(&p->lmtime);
		if(p->lmtime < year)
			printf(" %-7.7s %-4.4s ", cp+4, cp+20); else
			printf(" %-12.12s ", cp+4);
	}
	if (p->lflags&ISARG)
		printf("%s", p->ln.namep);
	else
		printf("%.14s", p->ln.lname);
	if (lflg || sflg || iflg || oneflg) putchar('\n');
	else name_pending = 1;
}

long
nblock(size)
long size;
{
	return((size+511)>>9);
}

int	m1[] = { 1, S_IREAD>>0, 'r', '-' };
int	m2[] = { 1, S_IWRITE>>0, 'w', '-' };
int	m3[] = { 2, S_ISUID, 's', S_IEXEC>>0, 'x', '-' };
int	m4[] = { 1, S_IREAD>>3, 'r', '-' };
int	m5[] = { 1, S_IWRITE>>3, 'w', '-' };
int	m6[] = { 2, S_ISGID, 's', S_IEXEC>>3, 'x', '-' };
int	m7[] = { 1, S_IREAD>>6, 'r', '-' };
int	m8[] = { 1, S_IWRITE>>6, 'w', '-' };
int	m9[] = { 2, S_ISVTX, 't', S_IEXEC>>6, 'x', '-' };

int	*m[] = { m1, m2, m3, m4, m5, m6, m7, m8, m9};

pmode(aflag)
{
	register int **mp;

	flags = aflag;
	for (mp = &m[0]; mp < &m[sizeof(m)/sizeof(m[0])];)
		select(*mp++);
}

select(pairp)
register int *pairp;
{
	register int n;

	n = *pairp++;
	while (--n>=0 && (flags&*pairp++)==0)
		pairp++;
	putchar(*pairp);
}

char *
makename(dir, file)
char *dir, *file;
{
	static char dfile[100];
	register char *dp, *fp;
	register int i;

	dp = dfile;
	fp = dir;
	while (*fp)
		*dp++ = *fp++;
	*dp++ = '/';
	fp = file;
	for (i=0; i<DIRSIZ; i++)
		*dp++ = *fp++;
	*dp = 0;
	return(dfile);
}

readdir(dir)
char *dir;
{
	static struct direct dentry;
	register int j;
	register struct lbuf *ep;

	if ((dirf = fopen(dir, "r")) == NULL) {
		printf("%s unreadable\n", dir);
		return;
	}
	tblocks = 0;
	for(;;) {
		if (fread((char *)&dentry, sizeof(dentry), 1, dirf) != 1)
			break;
		if (dentry.d_ino==0
		 || aflg==0 && dentry.d_name[0]=='.' &&  (dentry.d_name[1]=='\0'
			|| dentry.d_name[1]=='.' && dentry.d_name[2]=='\0'))
			continue;
		ep = gstat(makename(dir, dentry.d_name), 0);
		if (ep==NULL)
			continue;
		if (ep->lnum != -1)
			ep->lnum = dentry.d_ino;
		for (j=0; j<DIRSIZ; j++)
			ep->ln.lname[j] = dentry.d_name[j];
	}
	fclose(dirf);
}

struct lbuf *
gstat(file, argfl)
char *file;
{
	
	struct stat statb;
	register struct lbuf *rep;
	static int nomocore;

	if (nomocore)
		return(NULL);
	rep = (struct lbuf *)malloc(sizeof(struct lbuf));
	if (rep==NULL) {
		fprintf(stderr, "ls: out of memory\n");
		nomocore = 1;
		return(NULL);
	}
	if (lastp >= &flist[NFILES]) {
		static int msg;
		lastp--;
		if (msg==0) {
			fprintf(stderr, "ls: too many files\n");
			msg++;
		}
	}
	*lastp++ = rep;
	rep->lflags = 0;
	rep->lnum = 0;
	rep->ltype = '-';
	if (argfl || statreq) {
		if (stat(file, &statb)<0) {
			printf("%s not found\n", file);
			statb.st_ino = -1;
			statb.st_size = 0;
			statb.st_mode = 0;
			if (argfl) {
				lastp--;
				return(0);
			}
		}
		rep->lnum = statb.st_ino;
		rep->lsize = statb.st_size;
		switch(statb.st_mode&S_IFMT) {

		case S_IFDIR:
			rep->ltype = 'd';
			break;

		case S_IFBLK:
			rep->ltype = 'b';
			rep->lsize = statb.st_rdev;
			break;

		case S_IFCHR:
			rep->ltype = 'c';
			rep->lsize = statb.st_rdev;
			break;
		}
		rep->lflags = statb.st_mode & ~S_IFMT;
		rep->luid = statb.st_uid;
		rep->lgid = statb.st_gid;
		rep->lnl = statb.st_nlink;
		if(uflg)
			rep->lmtime = statb.st_atime;
		else if (cflg)
			rep->lmtime = statb.st_ctime;
		else
			rep->lmtime = statb.st_mtime;
		tblocks += nblock(statb.st_size);
	}
	return(rep);
}

compar(pp1, pp2)
struct lbuf **pp1, **pp2;
{
	register struct lbuf *p1, *p2;

	p1 = *pp1;
	p2 = *pp2;
	if (dflg==0) {
		if (p1->lflags&ISARG && p1->ltype=='d') {
			if (!(p2->lflags&ISARG && p2->ltype=='d'))
				return(1);
		} else {
			if (p2->lflags&ISARG && p2->ltype=='d')
				return(-1);
		}
	}
	if (tflg) {
		if(p2->lmtime == p1->lmtime)
			return(0);
		if(p2->lmtime > p1->lmtime)
			return(rflg);
		return(-rflg);
	}
	return(rflg * strcmp(p1->lflags&ISARG? p1->ln.namep: p1->ln.lname,
				p2->lflags&ISARG? p2->ln.namep: p2->ln.lname));
}
