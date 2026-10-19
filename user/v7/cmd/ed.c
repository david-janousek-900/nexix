/* Native V7 ed; retained editor/regex/rotor logic with bounded LP64 adaptations. */
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <crypt.h>
/*
 * Editor
 */

#include <signal.h>
#include <sgtty.h>
#include <setjmp.h>
typedef void (*ed_sighandler_t)(int);
#define	FNSIZE	64
#define	LBSIZE	512
#define	ESIZE	128
#define	GBSIZE	256
#define	NBRA	5
#define	EOF	-1
#define	KSIZE	9

#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11
#define	CKET	12
#define	CBACK	14

#define	STAR	01

char	Q[]	= "";
char	T[]	= "TMP";
#define	READ	0
#define	WRITE	1

int	peekc;
int	lastc;
char	savedfile[FNSIZE];
char	file[FNSIZE];
char	linebuf[LBSIZE];
char	rhsbuf[LBSIZE/2];
char	expbuf[ESIZE+4];
int	circfl;
int	*zero;
int	*dot;
int	*dol;
int	*addr1;
int	*addr2;
char	genbuf[LBSIZE];
long	count;
char	*nextip;
char	*linebp;
int	ninbuf;
int	io;
int	pflag;
long	lseek();
ed_sighandler_t oldhup;
ed_sighandler_t oldquit;
int	vflag	= 1;
int	xflag;
int	xtflag;
int	kflag;
char	key[KSIZE + 1];
char	crbuf[512];
char	perm[768];
char	tperm[768];
int	listf;
int	col;
char	*globp;
int	tfile	= -1;
int	tline;
char	*tfname;
char temp_name[]="/tmp/eXXXXXX";
int owns_temp, ready;
char	*loc1;
char	*loc2;
char	*locs;
char	ibuff[512];
int	iblock	= -1;
char	obuff[512];
int	oblock	= -1;
int	ichanged;
int	nleft;
char	WRERR[]	= "WRITE ERROR";
int	names[26];
int	anymarks;
char	*braslist[NBRA];
char	*braelist[NBRA];
int	nbra;
int	subnewa;
int	subolda;
int	fchange;
int	wrapp;
unsigned nlall = 128;

int	*address();
char	*ed_getline();
char	*getblock();
char	*place();
char	*mktemp();
jmp_buf	savej;
static void onintr(int),onhup(int),onterm(int);

main(argc, argv)
char **argv;
{
	register char *p1, *p2;
	int quit();
	ed_sighandler_t oldintr;

	oldquit = signal(SIGQUIT, SIG_IGN);
	oldhup = signal(SIGHUP, SIG_IGN);
	oldintr = signal(SIGINT, SIG_IGN);
	if ((uintptr_t)signal(SIGTERM, SIG_IGN) == 0)
		signal(SIGTERM, onterm);
	argv++;
	while (argc > 1 && **argv=='-') {
		switch((*argv)[1]) {

		case '\0':
			vflag = 0;
			break;

		case 'q':
			signal(SIGQUIT, SIG_DFL);
			vflag = 1;
			break;

		case 'x':
			xflag = 1;
			break;
		}
		argv++;
		argc--;
	}
	if(xflag){
		getkey();
		kflag = crinit(key, perm);
	}

	if (argc>1) {
		p1 = *argv;
		p2 = savedfile;
        if(strlen(p1)>=sizeof(savedfile)){write(2,"ed: filename too long\n",22);exit(1);}
		while (*p2++ = *p1++)
			;
		globp = "r";
	}
	zero = (int *)malloc(nlall*sizeof(int));
    if(!zero){write(2,"ed: out of memory\n",18);exit(1);}
    tfname=mktemp(temp_name);
    if(tfname!=temp_name){write(2,"ed: no temporary name\n",22);exit(1);}
	init();
	if (((uintptr_t)oldintr&01) == 0)
		signal(SIGINT, onintr);
	if (((uintptr_t)oldhup&01) == 0)
		signal(SIGHUP, onhup);
	setjmp(savej);
    ready=1;
	commands();
	quit();
}

commands()
{
	int getfile(), gettty();
	register *a1, c;

	for (;;) {
	if (pflag) {
		pflag = 0;
		addr1 = addr2 = dot;
		goto print;
	}
	addr1 = 0;
	addr2 = 0;
	do {
		addr1 = addr2;
		if ((a1 = address())==0) {
			c = getchr();
			break;
		}
		addr2 = a1;
		if ((c=getchr()) == ';') {
			c = ',';
			dot = a1;
		}
	} while (c==',');
	if (addr1==0)
		addr1 = addr2;
	switch(c) {

	case 'a':
		setdot();
		newline();
		append(gettty, addr2);
		continue;

	case 'c':
		delete();
		append(gettty, addr1-1);
		continue;

	case 'd':
		delete();
		continue;

	case 'E':
		fchange = 0;
		c = 'e';
	case 'e':
		setnoaddr();
		if (vflag && fchange) {
			fchange = 0;
			error(Q);
		}
		filename(c);
		init();
		addr2 = zero;
		goto caseread;

	case 'f':
		setnoaddr();
		filename(c);
		ed_puts(savedfile);
		continue;

	case 'g':
		global(1);
		continue;

	case 'i':
		setdot();
		nonzero();
		newline();
		append(gettty, addr2-1);
		continue;


	case 'j':
		if (addr2==0) {
			addr1 = dot;
			addr2 = dot+1;
		}
		setdot();
		newline();
		nonzero();
		join();
		continue;

	case 'k':
		if ((c = getchr()) < 'a' || c > 'z')
			error(Q);
		newline();
		setdot();
		nonzero();
		names[c-'a'] = *addr2 & ~01;
		anymarks |= 01;
		continue;

	case 'm':
		move(0);
		continue;

	case '\n':
		if (addr2==0)
			addr2 = dot+1;
		addr1 = addr2;
		goto print;

	case 'l':
		listf++;
	case 'p':
	case 'P':
		newline();
	print:
		setdot();
		nonzero();
		a1 = addr1;
		do {
			ed_puts(ed_getline(*a1++));
		} while (a1 <= addr2);
		dot = addr2;
		listf = 0;
		continue;

	case 'Q':
		fchange = 0;
	case 'q':
		setnoaddr();
		newline();
		quit();

	case 'r':
		filename(c);
	caseread:
		if ((io = open(file, 0)) < 0) {
			lastc = '\n';
			error(file);
		}
		setall();
		ninbuf = 0;
		c = zero != dol;
		append(getfile, addr2);
		exfile();
		fchange = c;
		continue;

	case 's':
		setdot();
		nonzero();
		substitute(globp!=0);
		continue;

	case 't':
		move(1);
		continue;

	case 'u':
		setdot();
		nonzero();
		newline();
		if ((*addr2&~01) != subnewa)
			error(Q);
		*addr2 = subolda;
		dot = addr2;
		continue;

	case 'v':
		global(0);
		continue;

	case 'W':
		wrapp++;
	case 'w':
		setall();
		nonzero();
		filename(c);
		if(!wrapp ||
		  ((io = open(file,1)) == -1) ||
		  ((lseek(io, 0L, 2)) == -1))
			if ((io = creat(file, 0666)) < 0)
				error(file);
		wrapp = 0;
		putfile();
		exfile();
		if (addr1==zero+1 && addr2==dol)
			fchange = 0;
		continue;

	case 'x':
		setnoaddr();
		newline();
		xflag = 1;
		ed_puts("Entering encrypting mode!");
		getkey();
		kflag = crinit(key, perm);
		continue;


	case '=':
		setall();
		newline();
		count = (addr2-zero)&077777;
		putd();
		putchr('\n');
		continue;

	case '!':
		callunix();
		continue;

	case EOF:
		return;

	}
	error(Q);
	}
}

int *
address()
{
    int c,minus=0,relerr,have=0;
    int64_t index=0,n,last=dol-zero;
    int *marked;
    for(;;){
        c=getchr();
        if(c>='0' && c<='9'){
            n=0;
            do {
                if(n>(INT32_MAX-(c-'0'))/10)error(Q);
                n=n*10+c-'0';
            } while((c=getchr())>='0' && c<='9');
            peekc=c;
            if(!have){index=0;have=1;}
            index+=minus<0?-n:n;
            if(index<INT32_MIN || index>INT32_MAX)error(Q);
            minus=0;continue;
        }
        relerr=have || minus;
        switch(c){
        case ' ':case '\t':continue;
        case '+':
            if(minus==INT32_MAX)error(Q);
            ++minus;if(!have){index=dot-zero;have=1;}continue;
        case '-':case '^':
            if(minus==INT32_MIN)error(Q);
            --minus;if(!have){index=dot-zero;have=1;}continue;
        case '?':case '/':
            compile(c);index=dot-zero;have=1;
            for(;;){
                if(c=='/')index=index==last?0:index+1;
                else index=index==0?last:index-1;
                if(execute(0,zero+index))break;
                if(index==dot-zero)error(Q);
            }
            break;
        case '$':index=last;have=1;break;
        case '.':index=dot-zero;have=1;break;
        case '\'':
            c=getchr();if(c<'a' || c>'z')error(Q);
            for(marked=zero;marked<=dol;++marked)
                if(names[c-'a']==(*marked&~01))break;
            index=marked-zero;have=1;break;
        default:
            peekc=c;if(!have)return(0);
            index+=minus;
            if(index<0 || index>last)error(Q);
            return(zero+index);
        }
        if(relerr)error(Q);
    }
}


setdot()
{
	if (addr2 == 0)
		addr1 = addr2 = dot;
	if (addr1 > addr2)
		error(Q);
}

setall()
{
	if (addr2==0) {
		addr1 = zero+1;
		addr2 = dol;
		if (dol==zero)
			addr1 = zero;
	}
	setdot();
}

setnoaddr()
{
	if (addr2)
		error(Q);
}

nonzero()
{
	if (addr1<=zero || addr2>dol)
		error(Q);
}

newline()
{
	register c;

	if ((c = getchr()) == '\n')
		return;
	if (c=='p' || c=='l') {
		pflag++;
		if (c=='l')
			listf++;
		if (getchr() == '\n')
			return;
	}
	error(Q);
}

filename(comm)
{
	register char *p1, *p2;
	register c;

	count = 0;
	c = getchr();
	if (c=='\n' || c==EOF) {
		p1 = savedfile;
		if (*p1==0 && comm!='f')
			error(Q);
		p2 = file;
		while (*p2++ = *p1++)
			;
		return;
	}
	if (c!=' ')
		error(Q);
	while ((c = getchr()) == ' ')
		;
	if (c=='\n')
		error(Q);
	p1 = file;
	do {
        if(p1>=file+sizeof(file)-1)error(Q);
		*p1++ = c;
		if (c==' ' || c==EOF)
			error(Q);
	} while ((c = getchr()) != '\n');
	*p1++ = 0;
	if (savedfile[0]==0 || comm=='e' || comm=='f') {
		p1 = savedfile;
		p2 = file;
		while (*p1++ = *p2++)
			;
	}
}

exfile()
{
	close(io);
	io = -1;
	if (vflag) {
		putd();
		putchr('\n');
	}
}

static void onintr(int unused)
{
	signal(SIGINT, onintr);
	putchr('\n');
	lastc = '\n';
	error(Q);
}

static void onhup(int unused)
{
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	if (dol > zero) {
		addr1 = zero+1;
		addr2 = dol;
		io = creat("ed.hup", 0666);
		if (io > 0)
			putfile();
	}
	fchange = 0;
	quit();
}

static void onterm(int unused)
{quit();}

error(s)
char *s;
{
	register c;

	wrapp = 0;
	listf = 0;
	putchr('?');
	ed_puts(s);
	count = 0;
	lseek(0, (long)0, 2);
	pflag = 0;
	if (globp)
		lastc = '\n';
	globp = 0;
	peekc = lastc;
	if(lastc)
		while ((c = getchr()) != '\n' && c != EOF)
			;
	if (io > 0) {
		close(io);
		io = -1;
	}
    if(!ready){if(owns_temp)unlink(tfname);exit(1);}
	longjmp(savej, 1);
}

getchr()
{
	char c;
	if (lastc=peekc) {
		peekc = 0;
		return(lastc);
	}
	if (globp) {
		if ((lastc = *globp++) != 0)
			return(lastc);
		globp = 0;
		return(EOF);
	}
	if (read(0, &c, 1) <= 0)
		return(lastc = EOF);
	lastc = c&0177;
	return(lastc);
}

gettty()
{
	register c;
	register char *gf;
	register char *p;

	p = linebuf;
	gf = globp;
	while ((c = getchr()) != '\n') {
		if (c==EOF) {
			if (gf)
				peekc = c;
			return(c);
		}
		if ((c &= 0177) == 0)
			continue;
		*p++ = c;
		if (p >= &linebuf[LBSIZE-2])
			error(Q);
	}
	*p++ = 0;
	if (linebuf[0]=='.' && linebuf[1]==0)
		return(EOF);
	return(0);
}

getfile()
{
	register c;
	register char *lp, *fp;

	lp = linebuf;
	fp = nextip;
	do {
		if (--ninbuf < 0) {
			if ((ninbuf = read(io, genbuf, LBSIZE)-1) < 0)
				return(EOF);
			fp = genbuf;
			while(fp < &genbuf[ninbuf]) {
				if (*fp++ & 0200) {
					if (kflag)
						crblock(perm, genbuf, ninbuf+1, count);
					break;
				}
			}
			fp = genbuf;
		}
		c = *fp++;
		if (c=='\0')
			continue;
		if (c&0200 || lp >= &linebuf[LBSIZE]) {
			lastc = '\n';
			error(Q);
		}
		*lp++ = c;
		count++;
	} while (c != '\n');
	*--lp = 0;
	nextip = fp;
	return(0);
}

putfile()
{
	int *a1, n;
	register char *fp, *lp;
	register nib;

	nib = 512;
	fp = genbuf;
	a1 = addr1;
	do {
		lp = ed_getline(*a1++);
		for (;;) {
			if (--nib < 0) {
				n = fp-genbuf;
				if(kflag)
					crblock(perm, genbuf, n, count-n);
				if(write(io, genbuf, n) != n) {
					ed_puts(WRERR);
					error(Q);
				}
				nib = 511;
				fp = genbuf;
			}
			count++;
			if ((*fp++ = *lp++) == 0) {
				fp[-1] = '\n';
				break;
			}
		}
	} while (a1 <= addr2);
	n = fp-genbuf;
	if(kflag)
		crblock(perm, genbuf, n, count-n);
	if(write(io, genbuf, n) != n) {
		ed_puts(WRERR);
		error(Q);
	}
}

append(f, a)
int *a;
int (*f)();
{
	register *a1, *a2, *rdot;
	int nline, tl;

	nline = 0;
	dot = a;
	while ((*f)() == 0) {
		if ((dol-zero)+1 >= nlall) {
            ptrdiff_t dot_index=dot-zero,dol_index=dol-zero;
            ptrdiff_t first=addr1?addr1-zero:-1,last=addr2?addr2-zero:-1;
            int *grown;
            if(nlall>32768-512){lastc='\n';error("MEM?");}
            grown=realloc(zero,(nlall+512)*sizeof(*zero));
            if(!grown){lastc='\n';error("MEM?");}
            zero=grown;nlall+=512;
            dot=zero+dot_index;dol=zero+dol_index;
            if(first>=0)addr1=zero+first;
            if(last>=0)addr2=zero+last;
		}
		tl = putline();
		nline++;
		a1 = ++dol;
		a2 = a1+1;
		rdot = ++dot;
		while (a1 > rdot)
			*--a2 = *--a1;
		*rdot = tl;
	}
	return(nline);
}

callunix()
{
	ed_sighandler_t savint;
	register int pid,rpid;
	int retcode;

	setnoaddr();
	if ((pid = fork()) == 0) {
		signal(SIGHUP, oldhup);
		signal(SIGQUIT, oldquit);
		execl("/bin/sh", "sh", "-t", 0);
		exit(0100);
	}
	savint = signal(SIGINT, SIG_IGN);
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;
	signal(SIGINT, savint);
	ed_puts("!");
}

quit()
{
	if (vflag && fchange && dol!=zero) {
		fchange = 0;
		error(Q);
	}
	unlink(tfname);
	exit(0);
}

delete()
{
	setdot();
	newline();
	nonzero();
	rdelete(addr1, addr2);
}

rdelete(ad1, ad2)
int *ad1, *ad2;
{
	register *a1, *a2, *a3;

	a1 = ad1;
	a2 = ad2+1;
	a3 = dol;
	dol -= a2 - a1;
	do {
		*a1++ = *a2++;
	} while (a2 <= a3);
	a1 = ad1;
	if (a1 > dol)
		a1 = dol;
	dot = a1;
	fchange = 1;
}

gdelete()
{
	register *a1, *a2, *a3;

	a3 = dol;
	for (a1=zero+1; (*a1&01)==0; a1++)
		if (a1>=a3)
			return;
	for (a2=a1+1; a2<=a3;) {
		if (*a2&01) {
			a2++;
			dot = a1;
		} else
			*a1++ = *a2++;
	}
	dol = a1-1;
	if (dot>dol)
		dot = dol;
	fchange = 1;
}

char *
ed_getline(tl)
{
	register char *bp, *lp;
	register nl;

	lp = linebuf;
	bp = getblock(tl, READ);
	nl = nleft;
	tl &= ~0377;
	while (*lp++ = *bp++)
		if (--nl == 0) {
			bp = getblock(tl+=0400, READ);
			nl = nleft;
		}
	return(linebuf);
}

putline()
{
	register char *bp, *lp;
	register nl;
	int tl;

	fchange = 1;
	lp = linebuf;
	tl = tline;
	bp = getblock(tl, WRITE);
	nl = nleft;
	tl &= ~0377;
	while (*bp = *lp++) {
		if (*bp++ == '\n') {
			*--bp = 0;
			linebp = lp;
			break;
		}
		if (--nl == 0) {
			bp = getblock(tl+=0400, WRITE);
			nl = nleft;
		}
	}
	nl = tline;
	tline += (((lp-linebuf)+03)>>1)&077776;
	return(nl);
}

char *
getblock(atl, iof)
{
	/* read/write use the native ssize_t prototypes. */
	register bno, off;
	register char *p1, *p2;
	register int n;
	
	bno = (atl>>8)&0377;
	off = (atl<<1)&0774;
	if (bno >= 255) {
		lastc = '\n';
		error(T);
	}
	nleft = 512 - off;
	if (bno==iblock) {
		ichanged |= iof;
		return(ibuff+off);
	}
	if (bno==oblock)
		return(obuff+off);
	if (iof==READ) {
		if (ichanged) {
			if(xtflag)
				crblock(tperm, ibuff, 512, (long)0);
			blkio(iblock, ibuff, WRITE);
		}
		ichanged = 0;
		iblock = bno;
		blkio(bno, ibuff, READ);
		if(xtflag)
			crblock(tperm, ibuff, 512, (long)0);
		return(ibuff+off);
	}
	if (oblock>=0) {
		if(xtflag) {
			p1 = obuff;
			p2 = crbuf;
			n = 512;
			while(n--)
				*p2++ = *p1++;
			crblock(tperm, crbuf, 512, (long)0);
			blkio(oblock, crbuf, WRITE);
		} else
			blkio(oblock, obuff, WRITE);
	}
	oblock = bno;
	return(obuff+off);
}

blkio(b, buf, iofcn)
char *buf;
int iofcn;
{
    if(lseek(tfile,(long)b<<9,0)<0)error(T);
	if ((iofcn==WRITE?write(tfile,buf,512):read(tfile,buf,512)) != 512) {
		error(T);
	}
}

init()
{
	register *markp;

	close(tfile);
	tline = 2;
	for (markp = names; markp < &names[26]; )
		*markp++ = 0;
	subnewa = 0;
	anymarks = 0;
	iblock = -1;
	oblock = -1;
	ichanged = 0;
    if(owns_temp)unlink(tfname);
    tfile=open(tfname,O_RDWR|O_CREAT|O_EXCL,0600);
    if(tfile<0)error(T);
    owns_temp=1;
	if(xflag) {
		xtflag = 1;
		makekey(key, tperm);
	}
    zero[0]=0;
	dot = dol = zero;
}

global(k)
{
	register char *gp;
	register c;
	register int *a1;
	char globuf[GBSIZE];

	if (globp)
		error(Q);
	setall();
	nonzero();
	if ((c=getchr())=='\n')
		error(Q);
	compile(c);
	gp = globuf;
	while ((c = getchr()) != '\n') {
		if (c==EOF)
			error(Q);
		if (c=='\\') {
			c = getchr();
			if (c!='\n')
				*gp++ = '\\';
		}
		*gp++ = c;
		if (gp >= &globuf[GBSIZE-2])
			error(Q);
	}
	*gp++ = '\n';
	*gp++ = 0;
	for (a1=zero; a1<=dol; a1++) {
		*a1 &= ~01;
		if (a1>=addr1 && a1<=addr2 && execute(0, a1)==k)
			*a1 |= 01;
	}
	/*
	 * Special case: g/.../d (avoid n^2 algorithm)
	 */
	if (globuf[0]=='d' && globuf[1]=='\n' && globuf[2]=='\0') {
		gdelete();
		return;
	}
	for (a1=zero; a1<=dol; a1++) {
		if (*a1 & 01) {
			*a1 &= ~01;
			dot = a1;
			globp = globuf;
			commands();
			a1 = zero;
		}
	}
}

join()
{
	register char *gp, *lp;
	register *a1;

	gp = genbuf;
	for (a1=addr1; a1<=addr2; a1++) {
		lp = ed_getline(*a1);
		while (*gp = *lp++)
			if (gp++ >= &genbuf[LBSIZE-2])
				error(Q);
	}
	lp = linebuf;
	gp = genbuf;
	while (*lp++ = *gp++)
		;
	*addr1 = putline();
	if (addr1<addr2)
		rdelete(addr1+1, addr2);
	dot = addr1;
}

substitute(inglob)
{
	register *markp, *a1, nl;
	int gsubf;
	int getsub();

	gsubf = compsub();
	for (a1 = addr1; a1 <= addr2; a1++) {
        ptrdiff_t index;
		if (execute(0, a1)==0)
			continue;
		inglob |= 01;
		dosub();
		if (gsubf) {
			while (*loc2) {
				if (execute(1, (int *)0)==0)
					break;
				dosub();
			}
		}
		subnewa = putline();
		*a1 &= ~01;
		if (anymarks) {
			for (markp = names; markp < &names[26]; markp++)
				if (*markp == *a1)
					*markp = subnewa;
		}
		subolda = *a1;
		*a1 = subnewa;
        index=a1-zero;
		nl = append(getsub, a1);
        a1=zero+index+nl;
		addr2 += nl;
	}
	if (inglob==0)
		error(Q);
}

compsub()
{
	register seof, c;
	register char *p;

	if ((seof = getchr()) == '\n' || seof == ' ')
		error(Q);
	compile(seof);
	p = rhsbuf;
	for (;;) {
		c = getchr();
		if (c=='\\')
			c = getchr() | 0200;
		if (c=='\n') {
			if (globp)
				c |= 0200;
			else
				error(Q);
		}
		if (c==seof)
			break;
		*p++ = c;
		if (p >= &rhsbuf[LBSIZE/2])
			error(Q);
	}
	*p++ = 0;
	if ((peekc = getchr()) == 'g') {
		peekc = 0;
		newline();
		return(1);
	}
	newline();
	return(0);
}

getsub()
{
	register char *p1, *p2;

	p1 = linebuf;
	if ((p2 = linebp) == 0)
		return(EOF);
	while (*p1++ = *p2++)
		;
	linebp = 0;
	return(0);
}

dosub()
{
	register char *lp, *sp, *rp;
	int c;

	lp = linebuf;
	sp = genbuf;
	rp = rhsbuf;
	while (lp < loc1)
		*sp++ = *lp++;
	while (c = *rp++&0377) {
		if (c=='&') {
			sp = place(sp, loc1, loc2);
			continue;
		} else if (c&0200 && (c &= 0177) >='1' && c < nbra+'1') {
			sp = place(sp, braslist[c-'1'], braelist[c-'1']);
			continue;
		}
		*sp++ = c&0177;
		if (sp >= &genbuf[LBSIZE])
			error(Q);
	}
	lp = loc2;
	loc2 = sp - genbuf + linebuf;
	while (*sp++ = *lp++)
		if (sp >= &genbuf[LBSIZE])
			error(Q);
	lp = linebuf;
	sp = genbuf;
	while (*lp++ = *sp++)
		;
}

char *
place(sp, l1, l2)
register char *sp, *l1, *l2;
{

	while (l1 < l2) {
		*sp++ = *l1++;
		if (sp >= &genbuf[LBSIZE])
			error(Q);
	}
	return(sp);
}

move(cflag)
{
	register int *adt, *ad1, *ad2;
	int getcopy();

	setdot();
	nonzero();
	if ((adt = address())==0)
		error(Q);
	newline();
	if (cflag) {
        ptrdiff_t end_index=dol-zero,target_index=adt-zero;
        append(getcopy,dol);
        ad1=zero+end_index+1;ad2=dol;
        adt=zero+target_index;
	} else {
		ad2 = addr2;
		for (ad1 = addr1; ad1 <= ad2;)
			*ad1++ &= ~01;
		ad1 = addr1;
	}
	ad2++;
	if (adt<ad1) {
		dot = adt + (ad2-ad1);
		if ((++adt)==ad1)
			return;
		reverse(adt, ad1);
		reverse(ad1, ad2);
		reverse(adt, ad2);
	} else if (adt >= ad2) {
		dot = adt++;
		reverse(ad1, ad2);
		reverse(ad2, adt);
		reverse(ad1, adt);
	} else
		error(Q);
	fchange = 1;
}

reverse(a1, a2)
register int *a1, *a2;
{
	register int t;

	for (;;) {
		t = *--a2;
		if (a2 <= a1)
			return;
		*a2 = *a1;
		*a1++ = t;
	}
}

getcopy()
{
	if (addr1 > addr2)
		return(EOF);
	ed_getline(*addr1++);
	return(0);
}

compile(aeof)
{
	register eof, c;
	register char *ep;
	char *lastep;
	char bracket[NBRA], *bracketp;
    unsigned char closed[NBRA]={0};
	int cclcnt;

	ep = expbuf;
	eof = aeof;
	bracketp = bracket;
	if ((c = getchr()) == eof) {
		if (*ep==0)
			error(Q);
		return;
	}
	circfl = 0;
	nbra = 0;
	if (c=='^') {
		c = getchr();
		circfl++;
	}
	peekc = c;
	lastep = 0;
	for (;;) {
		if (ep >= &expbuf[ESIZE])
			goto cerror;
		c = getchr();
		if (c==eof) {
			if (bracketp != bracket)
				goto cerror;
			*ep++ = CEOF;
			return;
		}
		if (c!='*')
			lastep = ep;
		switch (c) {

		case '\\':
			if ((c = getchr())=='(') {
				if (nbra >= NBRA)
					goto cerror;
				*bracketp++ = nbra;
				*ep++ = CBRA;
				*ep++ = nbra++;
				continue;
			}
			if (c == ')') {
				if (bracketp <= bracket)
					goto cerror;
				*ep++ = CKET;
				*ep++ = *--bracketp;
                closed[(unsigned char)*bracketp]=1;
				continue;
			}
			if (c>='1' && c<'1'+NBRA) {
                if(!closed[c-'1'])goto cerror;
				*ep++ = CBACK;
				*ep++ = c-'1';
				continue;
			}
			*ep++ = CCHR;
			if (c=='\n')
				goto cerror;
			*ep++ = c;
			continue;

		case '.':
			*ep++ = CDOT;
			continue;

		case '\n':
			goto cerror;

		case '*':
			if (lastep==0 || *lastep==CBRA || *lastep==CKET)
				goto defchar;
			*lastep |= STAR;
			continue;

		case '$':
			if ((peekc=getchr()) != eof)
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			*ep++ = CCL;
			*ep++ = 0;
			cclcnt = 1;
			if ((c=getchr()) == '^') {
				c = getchr();
				ep[-2] = NCCL;
			}
			do {
				if (c=='\n')
					goto cerror;
				if (c=='-' && ep[-1]!=0) {
					if ((c=getchr())==']') {
						*ep++ = '-';
						cclcnt++;
						break;
					}
					while (ep[-1]<c) {
						*ep = ep[-1]+1;
						ep++;
						cclcnt++;
						if (ep>=&expbuf[ESIZE])
							goto cerror;
					}
				}
				*ep++ = c;
				cclcnt++;
				if (ep >= &expbuf[ESIZE])
					goto cerror;
			} while ((c = getchr()) != ']');
			lastep[1] = cclcnt;
			continue;

		defchar:
		default:
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
   cerror:
	expbuf[0] = 0;
	nbra = 0;
	error(Q);
}

execute(gf, addr)
int *addr;
{
	register char *p1, *p2, c;

	for (c=0; c<NBRA; c++) {
		braslist[c] = 0;
		braelist[c] = 0;
	}
	if (gf) {
		if (circfl)
			return(0);
		p1 = linebuf;
		p2 = genbuf;
		while (*p1++ = *p2++)
			;
		locs = p1 = loc2;
	} else {
		if (addr==zero)
			return(0);
		p1 = ed_getline(*addr);
		locs = 0;
	}
	p2 = expbuf;
	if (circfl) {
		loc1 = p1;
		return(advance(p1, p2));
	}
	/* fast check for first character */
	if (*p2==CCHR) {
		c = p2[1];
		do {
			if (*p1!=c)
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
register char *ep, *lp;
{
	register char *curlp;
	int i;

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
        if(locs && lp==locs)return(0);
		loc2 = lp;
		return(1);

	case CCL:
		if (cclass(ep, *lp++, 1)) {
			ep += *ep;
			continue;
		}
		return(0);

	case NCCL:
		if (cclass(ep, *lp++, 0)) {
			ep += *ep;
			continue;
		}
		return(0);

	case CBRA:
		braslist[*ep++] = lp;
		continue;

	case CKET:
		braelist[*ep++] = lp;
		continue;

	case CBACK:
		if (braelist[i = *ep++]==0)
			error(Q);
		if (backref(i, lp)) {
			lp += braelist[i] - braslist[i];
			continue;
		}
		return(0);

	case CBACK|STAR:
		if (braelist[i = *ep++] == 0)
			error(Q);
        if(braelist[i]==braslist[i])continue;
		curlp = lp;
		while (backref(i, lp))
			lp += braelist[i] - braslist[i];
		while (lp >= curlp) {
			if (advance(lp, ep))
				return(1);
			lp -= braelist[i] - braslist[i];
		}
		continue;

	case CDOT|STAR:
		curlp = lp;
		while (*lp++)
			;
		goto star;

	case CCHR|STAR:
		curlp = lp;
		while (*lp++ == *ep)
			;
		ep++;
		goto star;

	case CCL|STAR:
	case NCCL|STAR:
		curlp = lp;
		while (cclass(ep, *lp++, ep[-1]==(CCL|STAR)))
			;
		ep += *ep;
		goto star;

	star:
		do {
			lp--;
			if (lp==locs)
				break;
			if (advance(lp, ep))
				return(1);
		} while (lp > curlp);
		return(0);

	default:
		error(Q);
	}
}

backref(i, lp)
register i;
register char *lp;
{
	register char *bp;

	bp = braslist[i];
    if(bp==braelist[i])return(1);
	while (*bp++ == *lp++)
		if (bp >= braelist[i])
			return(1);
	return(0);
}

cclass(set, c, af)
register char *set, c;
{
	register n;

	if (c==0)
		return(0);
	n = *set++;
	while (--n)
		if (*set++ == c)
			return(af);
	return(!af);
}

putd()
{
	register r;

	r = count%10;
	count /= 10;
	if (count)
		putd();
	putchr(r + '0');
}

ed_puts(sp)
register char *sp;
{
	col = 0;
	while (*sp)
		putchr(*sp++);
	putchr('\n');
}

char	line[70];
char	*linp	= line;

putchr(ac)
{
	register char *lp;
	register c;

	lp = linp;
	c = ac;
	if (listf) {
		col++;
		if (col >= 72) {
			col = 0;
			*lp++ = '\\';
			*lp++ = '\n';
		}
		if (c=='\t') {
			c = '>';
			goto esc;
		}
		if (c=='\b') {
			c = '<';
		esc:
			*lp++ = '-';
			*lp++ = '\b';
			*lp++ = c;
			goto out;
		}
		if (c<' ' && c!= '\n') {
			*lp++ = '\\';
			*lp++ = (c>>3)+'0';
			*lp++ = (c&07)+'0';
			col += 2;
			goto out;
		}
	}
	*lp++ = c;
out:
	if(c == '\n' || lp >= &line[64]) {
		linp = line;
		write(1, line, lp-line);
		return;
	}
	linp = lp;
}
crblock(permp, buf, nchar, startn)
char *permp;
char *buf;
long startn;
{
	register char *p1;
	int n1;
	int n2;
	register char *t1, *t2, *t3;

	t1 = permp;
	t2 = &permp[256];
	t3 = &permp[512];

	n1 = startn&0377;
	n2 = (startn>>8)&0377;
	p1 = buf;
	while(nchar--) {
		*p1 = t2[(t3[(t1[(*p1+n1)&0377]+n2)&0377]-n2)&0377]-n1;
		n1++;
		if(n1==256){
			n1 = 0;
			n2++;
			if(n2==256) n2 = 0;
		}
		p1++;
	}
}

getkey()
{
	struct sgttyb b;
	int save;
	ed_sighandler_t sig;
	register char *p;
	register c;

	sig = signal(SIGINT, SIG_IGN);
    if(gtty(0,&b)<0){signal(SIGINT,sig);error("Input not tty");}
	save = b.sg_flags;
	b.sg_flags &= ~ECHO;
    if(stty(0,&b)<0){signal(SIGINT,sig);error("Input not tty");}
	ed_puts("Key:");
	p = key;
	while(((c=getchr()) != EOF) && (c!='\n')) {
		if(p < &key[KSIZE])
			*p++ = c;
	}
	*p = 0;
	b.sg_flags = save;
	stty(0, &b);
	signal(SIGINT, sig);
	return(key[0] != 0);
}

/*
 * Besides initializing the encryption machine, this routine
 * returns 0 if the key is null, and 1 if it is non-null.
 */
crinit(keyp, permp)
char	*keyp, *permp;
{
	register char *t1, *t2, *t3;
	register i;
	int ic, k, temp;
	uint16_t random;
	char buf[13];
	uint32_t seed;

	t1 = permp;
	t2 = &permp[256];
	t3 = &permp[512];
	if(*keyp == 0)
		return(0);
	strncpy(buf, keyp, 8);
	while (*keyp)
		*keyp++ = '\0';
	buf[8] = buf[0];
	buf[9] = buf[1];
    {
        char password[9],salt[3],*encoded;
        memcpy(password,buf,8);password[8]=0;
        salt[0]=buf[8];salt[1]=buf[9];salt[2]=0;
        encoded=crypt(password,salt);
        memset(password,0,sizeof(password));
        if(!encoded)error("crypt: cannot generate key");
        memcpy(buf,encoded,13);
    }
	seed = 123;
	for (i=0; i<13; i++)
		seed = seed*buf[i] + i;
	for(i=0;i<256;i++){
		t1[i] = i;
		t3[i] = 0;
	}
	for(i=0; i<256; i++) {
		seed = 5*seed + buf[i%13];
        random=(uint16_t)((seed<=INT32_MAX?(int64_t)seed:(int64_t)seed-INT64_C(4294967296))%65521);
		k = 256-1 - i;
		ic = (random&0377) % (k+1);
		random >>= 8;
		temp = t1[k];
		t1[k] = t1[ic];
		t1[ic] = temp;
		if(t3[k]!=0) continue;
		ic = (random&0377) % k;
		while(t3[ic]!=0) ic = (ic+1) % k;
		t3[k] = ic;
		t3[ic] = k;
	}
	for(i=0; i<256; i++)
		t2[t1[i]&0377] = i;
	return(1);
}

makekey(a, b)
char *a, *b;
{
	register int i;
	long t;
	char temp[KSIZE + 1];

	for(i = 0; i < KSIZE; i++)
		temp[i] = *a++;
	time(&t);
	t += getpid();
	for(i = 0; i < 4; i++)
		temp[i] ^= (t>>(8*i))&0377;
    if(!temp[0])temp[0]=1;
	crinit(temp, b);
}
