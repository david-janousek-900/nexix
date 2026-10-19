/*
 */

#include <setjmp.h>
#define	INTR	2
#define	QUIT	3
#define LINSIZ 1000
#define ARGSIZ 50
#define TRESIZ 100

#define QUOTE 0200
#define FAND 1
#define FCAT 2
#define FPIN 4
#define FPOU 8
#define FPAR 16
#define FINT 32
#define FPRS 64
#define TCOM 1
#define TPAR 2
#define TFIL 3
#define TLST 4
#define DTYP 0
#define DLEF 1
#define DRIT 2
#define DFLG 3
#define DSPR 4
#define DCOM 5


#include "../osh-native.h"
static intptr_t *tree(int), *syntax(char **,char **), *syn1(char **,char **), *syn2(char **,char **), *syn3(char **,char **);
int errval;
char	*dolp;
char	pidp[16];
char	**dolv;
jmp_buf	jmpbuf;
int	dolc;
char	*promp;
char	*linep;
char	*elinep;
char	**argp;
char	**eargp;
intptr_t	*treep;
intptr_t	*treeend;
char	peekc;
char	gflg;
char	error;
uid_t	uid;
char	setintr;
char	*arginp;
int	onelflg;
int	stoperr;

#define	OMSG	16
char	*mesg[OMSG] = {
	0,
	"Hangup",
	0,
	"Quit",
	"Illegal instruction",
	"Trace/BPT trap",
	"IOT trap",
	"EMT trap",
	"Floating exception",
	"Killed",
	"Bus error",
	"Memory fault",
	"Bad system call",
	0,
	"Alarm clock",
	"Terminated",
};

char	line[LINSIZ];
char	*args[ARGSIZ];
intptr_t	trebuf[TRESIZ];

main(c, av)
int c;
char **av;
{
	register f;
	register char *acname, **v;

	for(f=3; f<15; f++)
		close(f);
	snprintf(pidp,sizeof(pidp),"%u",(unsigned)getpid());
	v = av;
	acname = "<none>";
	promp = "% ";
	if((uid = getuid()) == 0)
		promp = "# ";
	if(c>1 && v[1][0]=='-' && v[1][1]=='e') {
		++stoperr;
		v[1] = v[0];
		++v;
		--c;
	}
	if(c > 1) {
		promp = 0;
		if (*v[1]=='-') {
			**v = '-';
			if (v[1][1]=='c' && c>2)
				arginp = v[2];
			else if (v[1][1]=='t')
				onelflg = 2;
		} else {
			close(0);
			f = open(v[1], 0);
			if(f < 0) {
				prs(v[1]);
				err(": cannot open",255);
			}
		}
	}
	if(**v == '-') {
		signal(QUIT,SIG_IGN);
		sighandler_t old_handler = signal(INTR,SIG_IGN);
		if ((arginp==0&&onelflg==0) || old_handler!=SIG_IGN)
			setintr++;
	}
	dolv = v+1;
	dolc = c-1;

loop:
	if(promp != 0)
		prs(promp);
	peekc = osh_getc();
	main1();
	goto loop;
}

main1()
{
	register char  *cp;
	register intptr_t *t;

	argp = args;
	eargp = args+ARGSIZ-5;
	linep = line;
	elinep = line+LINSIZ-5;
	error = 0;
	gflg = 0;
	do {
		cp = linep;
		word();
	} while(*cp != '\n');
	treep = trebuf;
	treeend = &trebuf[TRESIZ];
	if(gflg == 0) {
		if(error == 0) {
			setjmp(jmpbuf);
			if (error)
				return;
			t = syntax(args, argp);
		}
		if(error != 0)
			err("syntax error",255); else
			execute(t);
	}
}

word()
{
	register char c, c1;

	*argp++ = linep;

loop:
	switch(c = osh_getc()) {

	case ' ':
	case '\t':
		goto loop;

	case '\'':
	case '"':
		c1 = c;
		while((c=readc()) != c1) {
			if(c == '\n') {
				error++;
				peekc = c;
				return;
			}
			*linep++ = c|QUOTE;
		}
		goto pack;

	case '&':
	case ';':
	case '<':
	case '>':
	case '(':
	case ')':
	case '|':
	case '^':
	case '\n':
		*linep++ = c;
		*linep++ = '\0';
		return;
	}

	peekc = c;

pack:
	for(;;) {
		c = osh_getc();
		if(any(c, " '\"\t;&<>()|^\n")) {
			peekc = c;
			if(any(c, "\"'"))
				goto loop;
			*linep++ = '\0';
			return;
		}
		*linep++ = c;
	}
}

intptr_t *
tree(n)
int n;
{
	register intptr_t *t;

	t = treep;
	if(n<0 || n>treeend-treep){prs("Command line overflow\n");error++;longjmp(jmpbuf,1);}
	treep += n;
	if (treep>treeend) {
		prs("Command line overflow\n");
		error++;
		longjmp(jmpbuf, 1);
	}
	return(t);
}

osh_getc()
{
	register char c;

	if(peekc) {
		c = peekc;
		peekc = 0;
		return(c);
	}
	if(argp > eargp) {
		argp -= 10;
		while((c=osh_getc()) != '\n');
		argp += 10;
		err("Too many args",255);
		gflg++;
		return(c);
	}
	if(linep > elinep) {
		linep -= 10;
		while((c=osh_getc()) != '\n');
		linep += 10;
		err("Too many characters",255);
		gflg++;
		return(c);
	}
getd:
	if(dolp) {
		c = *dolp++;
		if(c != '\0')
			return(c);
		dolp = 0;
	}
	c = readc();
	if(c == '\\') {
		c = readc();
		if(c == '\n')
			return(' ');
		return(c|QUOTE);
	}
	if(c == '$') {
		c = readc();
		if(c>='0' && c<='9') {
			if(c-'0' < dolc)
				dolp = dolv[c-'0'];
			goto getd;
		}
		if(c == '$') {
			dolp = pidp;
			goto getd;
		}
	}
	return(c&0177);
}

readc()
{
	int rdstat;
	char cc;
	register c;

	if (arginp) {
		if (arginp == (char *)1)
			exit(errval);
		if ((c = *arginp++) == 0) {
			arginp = (char *)1;
			c = '\n';
		}
		return(c);
	}
	if (onelflg==1)
		exit(255);
	if((rdstat = read(0, &cc, 1)) != 1)
		if(rdstat==0) exit(errval); /* end of file*/
		else exit(255); /* error */
	if (cc=='\n' && onelflg)
		onelflg--;
	return(cc);
}

/*
 * syntax
 *	empty
 *	syn1
 */

intptr_t *
syntax(p1, p2)
char **p1, **p2;
{

	while(p1 != p2) {
		if(any(**p1, ";&\n"))
			p1++; else
			return(syn1(p1, p2));
	}
	return(0);
}

/*
 * syn1
 *	syn2
 *	syn2 & syntax
 *	syn2 ; syntax
 */

intptr_t *
syn1(p1, p2)
char **p1, **p2;
{
	register char **p;
	register intptr_t *t, *t1;
	int l;

	l = 0;
	for(p=p1; p!=p2; p++)
	switch(**p) {

	case '(':
		l++;
		continue;

	case ')':
		l--;
		if(l < 0)
			error++;
		continue;

	case '&':
	case ';':
	case '\n':
		if(l == 0) {
			l = **p;
			t = tree(4);
			t[DTYP] = TLST;
			t[DLEF] = syn2(p1, p);
			t[DFLG] = 0;
			if(l == '&') {
				t1 = t[DLEF];
				t1[DFLG] |= FAND|FPRS|FINT;
			}
			t[DRIT] = syntax(p+1, p2);
			return(t);
		}
	}
	if(l == 0)
		return(syn2(p1, p2));
	error++;
	return(0);
}

/*
 * syn2
 *	syn3
 *	syn3 | syn2
 */

intptr_t *
syn2(p1, p2)
char **p1, **p2;
{
	register char **p;
	register int l;
    register intptr_t *t;

	l = 0;
	for(p=p1; p!=p2; p++)
	switch(**p) {

	case '(':
		l++;
		continue;

	case ')':
		l--;
		continue;

	case '|':
	case '^':
		if(l == 0) {
			t = tree(4);
			t[DTYP] = TFIL;
			t[DLEF] = syn3(p1, p);
			t[DRIT] = syn2(p+1, p2);
			t[DFLG] = 0;
			return(t);
		}
	}
	return(syn3(p1, p2));
}

/*
 * syn3
 *	( syn1 ) [ < in  ] [ > out ]
 *	word word* [ < in ] [ > out ]
 */

intptr_t *
syn3(p1, p2)
char **p1, **p2;
{
	register char **p;
	char **lp, **rp;
	register intptr_t *t;
	int n,l,c,flg;
    intptr_t i,o;

	flg = 0;
	if(**p2 == ')')
		flg |= FPAR;
	lp = 0;
	rp = 0;
	i = 0;
	o = 0;
	n = 0;
	l = 0;
	for(p=p1; p!=p2; p++)
	switch(c = **p) {

	case '(':
		if(l == 0) {
			if(lp != 0)
				error++;
			lp = p+1;
		}
		l++;
		continue;

	case ')':
		l--;
		if(l == 0)
			rp = p;
		continue;

	case '>':
		p++;
		if(p!=p2 && **p=='>')
			flg |= FCAT; else
			p--;

	case '<':
		if(l == 0) {
			p++;
			if(p == p2) {
				error++;
				p--;
			}
			if(any(**p, "<>("))
				error++;
			if(c == '<') {
				if(i != 0)
					error++;
				i = *p;
				continue;
			}
			if(o != 0)
				error++;
			o = *p;
		}
		continue;

	default:
		if(l == 0)
			p1[n++] = *p;
	}
	if(lp != 0) {
		if(n != 0)
			error++;
		t = tree(5);
		t[DTYP] = TPAR;
		t[DSPR] = syn1(lp, rp);
		goto out;
	}
	if(n == 0)
		error++;
	p1[n++] = 0;
	t = tree(n+5);
	t[DTYP] = TCOM;
	for(l=0; l<n; l++)
		t[l+DCOM] = p1[l];
out:
	t[DFLG] = flg;
	t[DLEF] = i;
	t[DRIT] = o;
	return(t);
}

scan(at, f)
intptr_t *at;
int (*f)();
{
	register char *p, c;
	register intptr_t *t;

	t = at+DCOM;
	while(p = *t++)
		while(c = *p)
			*p++ = (*f)(c);
}

tglob(c)
int c;
{

	if(any(c, "[?*"))
		gflg = 1;
	return(c);
}

trim(c)
int c;
{

	return(c&0177);
}

execute(t, pf1, pf2)
intptr_t *t;
int *pf1, *pf2;
{
	int i, f, pv[2];
	register intptr_t *t1;
	register char *cp1, *cp2;
	

	if(t != 0)
	switch(t[DTYP]) {

	case TCOM:
		cp1 = t[DCOM];
		if(equal(cp1, "chdir")) {
			if(t[DCOM+1] != 0) {
				if(chdir(t[DCOM+1]) < 0)
					err("chdir: bad directory",255);
			} else
				err("chdir: arg count",255);
			return;
		}
		if(equal(cp1, "shift")) {
			if(dolc < 1) {
				prs("shift: no args\n");
				return;
			}
			dolv[1] = dolv[0];
			dolv++;
			dolc--;
			return;
		}
		if(equal(cp1, "login")) {
			if(promp != 0) {
				execv("/bin/login", t+DCOM);
			}
			prs("login: cannot execute\n");
			return;
		}
		if(equal(cp1, "newgrp")) {
			if(promp != 0) {
				execv("/bin/newgrp", t+DCOM);
			}
			prs("newgrp: cannot execute\n");
			return;
		}
		if(equal(cp1, "wait")) {
			pwait(-1, 0);
			return;
		}
		if(equal(cp1, ":"))
			return;

	case TPAR:
		f = t[DFLG];
		i = 0;
		if((f&FPAR) == 0)
			i = fork();
		if(i == -1) {
			err("try again",255);
			return;
		}
		if(i != 0) {
			if((f&FPIN) != 0) {
				close(pf1[0]);
				close(pf1[1]);
			}
			if((f&FPRS) != 0) {
				prn(i);
				prs("\n");
			}
			if((f&FAND) != 0)
				return;
			if((f&FPOU) == 0)
				pwait(i, t);
			return;
		}
		if(t[DLEF] != 0) {
			close(0);
			i = open(t[DLEF], 0);
			if(i < 0) {
				prs(t[DLEF]);
				err(": cannot open",255);
				exit(255);
			}
		}
		if(t[DRIT] != 0) {
			if((f&FCAT) != 0) {
				i = open(t[DRIT], 1);
				if(i >= 0) {
					lseek(i, 0L, 2);
					goto f1;
				}
			}
			i = creat(t[DRIT], 0666);
			if(i < 0) {
				prs(t[DRIT]);
				err(": cannot create",255);
				exit(255);
			}
		f1:
			close(1);
			dup(i);
			close(i);
		}
		if((f&FPIN) != 0) {
			close(0);
			dup(pf1[0]);
			close(pf1[0]);
			close(pf1[1]);
		}
		if((f&FPOU) != 0) {
			close(1);
			dup(pf2[1]);
			close(pf2[0]);
			close(pf2[1]);
		}
		if((f&FINT)!=0 && t[DLEF]==0 && (f&FPIN)==0) {
			close(0);
			open("/dev/null", 0);
		}
		if((f&FINT) == 0 && setintr) {
			signal(INTR,SIG_DFL);
			signal(QUIT,SIG_DFL);
		}
		if(t[DTYP] == TPAR) {
			if(t1 = t[DSPR])
				t1[DFLG] |= f&FINT;
			execute(t1);
			exit(255);
		}
		gflg = 0;
		scan(t, tglob);
		if(expand_arguments((char **)(t+DCOM)))exit(255);
        scan(t,trim);
        texec(execargs[0],t);
        { char candidate[LINSIZ+16];
          snprintf(candidate,sizeof(candidate),"/bin/%s",execargs[0]);texec(candidate,t);
          snprintf(candidate,sizeof(candidate),"/usr/bin/%s",execargs[0]);texec(candidate,t);
        }
		prs(t[DCOM]);
		err(": not found",255);
		exit(255);

	case TFIL:
		f = t[DFLG];
		if(pipe(pv)<0){err("try again",255);return;}
		t1 = t[DLEF];
		t1[DFLG] |= FPOU | (f&(FPIN|FINT|FPRS));
		execute(t1, pf1, pv);
		t1 = t[DRIT];
		t1[DFLG] |= FPIN | (f&(FPOU|FINT|FAND|FPRS));
		execute(t1, pv, pf2);
		return;

	case TLST:
		f = t[DFLG]&FINT;
		if(t1 = t[DLEF])
			t1[DFLG] |= f;
		execute(t1);
		if(t1 = t[DRIT])
			t1[DFLG] |= f;
		execute(t1);
		return;

	}
}

texec(f, at)
char *f;
intptr_t *at;
{
	
	register intptr_t *t;

	t = at;
    execv(f,execargs);
    if(errno==ENOEXEC){
        char *shellargs[ARGSIZ+2];unsigned j;
        shellargs[0]="/bin/osh";shellargs[1]=f;
        for(j=1;execargs[j];++j)shellargs[j+1]=execargs[j];shellargs[j+1]=NULL;
        execv(shellargs[0],shellargs);prs("No shell!\n");exit(255);
    }
	if (errno==ENOMEM) {
		prs(t[DCOM]);
		err(": too large",255);
		exit(255);
	}
}

err(s, exitno)
char *s;
int exitno;
{

	prs(s);
	prs("\n");
	if(promp == 0) {
		lseek(0, 0L, 2);
		exit(exitno);
	}
}

prs(as)
char *as;
{
	register char *s;

	s = as;
	while(*s)
		osh_putc(*s++);
}

osh_putc(c)
{
	char cc;

	cc = c;
	write(2, &cc, 1);
}

prn(n)
int n;
{
	register a;

	if (a = n/10)
		prn(a);
	osh_putc(n%10 + '0');
}

any(c, as)
int c;
char *as;
{
	register char *s;

	s = as;
	while(*s)
		if(*s++ == c)
			return(1);
	return(0);
}

equal(as1, as2)
char *as1, *as2;
{
	register char *s1, *s2;

	s1 = as1;
	s2 = as2;
	while(*s1++ == *s2)
		if(*s2++ == '\0')
			return(1);
	return(0);
}

pwait(i, t)
int i;
intptr_t *t;
{
	register p, e;
	int s;

	if(i != 0)
	for(;;) {
		p = wait(&s);
		if(p == -1)
			break;
		e = s&0177;
		if (e>=OMSG || mesg[e]) {
			if(p != i) {
				prn(p);
				prs(": ");
			}
			if (e < OMSG)
				prs(mesg[e]);
			else {
				prs("Signal ");
				prn(e);
			}
			if(s&0200)
				prs(" -- Core dumped");
		}
		if (e || s&&stoperr)
			err("", (s>>8)|e );
		errval |= (s>>8);
	}
}
