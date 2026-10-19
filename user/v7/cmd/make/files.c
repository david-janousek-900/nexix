/* UNIX DEPENDENT PROCEDURES */


/* DEFAULT RULES FOR UNIX */

char *builtin[] =
	{
	".SUFFIXES : .out .o .c .f .e .r .y .yr .ye .l .s",
	"YACC=yacc",
	"YACCR=yacc -r",
	"YACCE=yacc -e",
	"YFLAGS=",
	"LEX=lex",
	"LFLAGS=",
	"CC=cc",
#ifdef vax
	"AS=as".
#else
	"AS=as -",
#endif
	"CFLAGS=",
	"RC=f77",
	"RFLAGS=",
	"EC=f77",
	"EFLAGS=",
	"FFLAGS=",
	"LOADLIBES=",

	".c.o :",
	"\t$(CC) $(CFLAGS) -c $<",

	".e.o .r.o .f.o :",
	"\t$(EC) $(RFLAGS) $(EFLAGS) $(FFLAGS) -c $<",

	".s.o :",
	"\t$(AS) -o $@ $<",

	".y.o :",
	"\t$(YACC) $(YFLAGS) $<",
	"\t$(CC) $(CFLAGS) -c y.tab.c",
	"\trm y.tab.c",
	"\tmv y.tab.o $@",

	".yr.o:",
	"\t$(YACCR) $(YFLAGS) $<",
	"\t$(RC) $(RFLAGS) -c y.tab.r",
	"\trm y.tab.r",
	"\tmv y.tab.o $@",

	".ye.o :",
	"\t$(YACCE) $(YFLAGS) $<",
	"\t$(EC) $(RFLAGS) -c y.tab.e",
	"\trm y.tab.e",
	"\tmv y.tab.o $@",

	".l.o :",
	"\t$(LEX) $(LFLAGS) $<",
	"\t$(CC) $(CFLAGS) -c lex.yy.c",
	"\trm lex.yy.c",
	"\tmv lex.yy.o $@",

	".y.c :",
	"\t$(YACC) $(YFLAGS) $<",
	"\tmv y.tab.c $@",

	".l.c :",
	"\t$(LEX) $<",
	"\tmv lex.yy.c $@",

	".yr.r:",
	"\t$(YACCR) $(YFLAGS) $<",
	"\tmv y.tab.r $@",

	".ye.e :",
	"\t$(YACCE) $(YFLAGS) $<",
	"\tmv y.tab.e $@",

	".s.out .c.out .o.out :",
	"\t$(CC) $(CFLAGS) $< $(LOADLIBES) -o $@",

	".f.out .r.out .e.out :",
	"\t$(EC) $(EFLAGS) $(RFLAGS) $(FFLAGS) $< $(LOADLIBES) -o $@",
	"\t-rm $*.o",

	".y.out :",
	"\t$(YACC) $(YFLAGS) $<",
	"\t$(CC) $(CFLAGS) y.tab.c $(LOADLIBES) -ly -o $@",
	"\trm y.tab.c",

	".l.out :",
	"\t$(LEX) $<",
	"\t$(CC) $(CFLAGS) lex.yy.c $(LOADLIBES) -ll -o $@",
	"\trm lex.yy.c",

	0 };

#include "defs"
#include <sys/types.h>


TIMETYPE exists(filename)
char *filename;
{
#include <sys/stat.h>
struct stat buf;
register char *s;
TIMETYPE lookarch();

for(s = filename ; *s!='\0' && *s!='(' ; ++s)
	;

if(*s == '(')
	return(lookarch(filename));

if(stat(filename,&buf) < 0) 
	return(0);
else	return(buf.st_mtime ? buf.st_mtime : 1);
}


TIMETYPE prestime()
{
TIMETYPE t;
time(&t);
return(t);
}



#ifdef NX_MAKE_HOST
#include <dirent.h>
#else
#include <sys/dir.h>
#endif
static int amatch(char *, char *);
static int umatch(char *, char *);

struct depblock *srchdir(char *pat, int mkchain, struct depblock *nextdbl)
{
    char directory[OUTMAX], fullname[OUTMAX], name[256];
    char *slash = strrchr(pat,'/'), *filepat;
    struct depblock *result = NULL, *dep;
    struct nameblock *q;
    struct pattern *cached;
    size_t prefix = slash ? (size_t)(slash-pat)+1 : 0;
#ifdef NX_MAKE_HOST
    DIR *dir;
    struct dirent *entry;
#else
    int fd;
    struct direct entry;
    ssize_t amount;
#endif
    if (strlen(pat) >= OUTMAX) fatal("pattern too long");
    if (!mkchain) {
        for (cached=firstpat; cached; cached=cached->nxtpattern)
            if (!strcmp(pat,cached->patval)) return NULL;
    }
    cached=ALLOC(pattern); cached->nxtpattern=firstpat;
    firstpat=cached; cached->patval=copys(pat);
    if (slash) { memcpy(directory,pat,prefix); directory[prefix]=0; }
    else strcpy(directory,".");
    filepat=pat+prefix;
#ifdef NX_MAKE_HOST
    dir=opendir(directory);
    if (!dir) fatal1("Cannot open directory %s",directory);
    while ((entry=readdir(dir))) {
        snprintf(name,sizeof name,"%s",entry->d_name);
#else
    fd=open(directory,O_RDONLY);
    if (fd<0) fatal1("Cannot open directory %s",directory);
    while ((amount=read(fd,&entry,sizeof entry))==sizeof entry) {
        if (!entry.d_ino) continue;
        memcpy(name,entry.d_name,DIRSIZ); name[DIRSIZ]=0;
#endif
        if (!amatch(name,filepat)) continue;
        if (prefix+strlen(name)>=sizeof fullname) fatal("path too long");
        memcpy(fullname,pat,prefix); strcpy(fullname+prefix,name);
        q=srchname(fullname); if (!q) q=makename(fullname);
        if (mkchain) {
            dep=ALLOC(depblock); dep->depname=q; dep->nxtdepblock=nextdbl;
            nextdbl=result=dep;
        }
    }
#ifdef NX_MAKE_HOST
    closedir(dir);
#else
    close(fd);
    if (amount) fatal("directory read error");
#endif
    return result;
}

/* stolen from glob through find */

static int amatch(s, p)
char *s, *p;
{
	register int cc, scc, k;
	int c, lc;

	scc = *s;
	lc = 077777;
	switch (c = *p) {

	case '[':
		if (!scc) return 0;
		k = 0;
		while (cc = *++p) {
			switch (cc) {

			case ']':
				if (k)
					return(amatch(++s, ++p));
				else
					return(0);

			case '-':
				k |= (lc <= scc)  & (scc <= (cc=p[1]) ) ;
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

static int umatch(s, p)
char *s, *p;
{
	if(*p==0) return(1);
	while(*s)
		if (amatch(s++,p)) return(1);
	return(0);
}

#ifdef METERFILE
#include <pwd.h>
int meteron	= 0;	/* default: metering off */

meter(file)
char *file;
{
TIMETYPE tvec;
char *p, *ctime();
FILE * mout;
struct passwd *pwd, *getpwuid();

if(file==0 || meteron==0) return;

pwd = getpwuid(getuid());

time(&tvec);

if( (mout=fopen(file,"a")) != NULL )
	{
	p = ctime(&tvec);
	p[16] = '\0';
	fprintf(mout,"User %s, %s\n",pwd->pw_name,p+4);
	fclose(mout);
	}
}
#endif


/* Native archive and ELF64 lookup is in archive.c. */
