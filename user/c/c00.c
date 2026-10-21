/* C compiler
 *
 *
 *
 * Called from cc:
 *   c0 source temp1 temp2 [ profileflag ]
 * temp1 gets most of the intermediate code;
 * strings are put on temp2, which c1 reads after temp1.
 */

#include "c0.h"
#include <stdarg.h>
#include <unistd.h>

int	isn	= 1;
int	peeksym	= -1;
int	line	= 1;
struct	tnode	funcblk = { NAME, 0, NULL, NULL, NULL, NULL, 0, NULL };
int	contype;		/* type of the last integer constant */
static struct tnode *cmstack[CMSIZ];
static int treedepth;

struct kwtab {
	char	*kwname;
	int	kwval;
} kwtab[] = {
	{"int",		INT},
	{"char",	CHAR},
	{"float",	FLOAT},
	{"double",	DOUBLE},
	{"struct",	STRUCT},
	{"long",	LONG},
	{"unsigned",	UNSIGN},
	{"union",	UNION},
	{"short",	SHORTKW},
	{"auto",	AUTO},
	{"extern",	EXTERN},
	{"static",	STATIC},
	{"register",	REG},
	{"goto",	GOTO},
	{"return",	RETURN},
	{"if",		IF},
	{"while",	WHILE},
	{"else",	ELSE},
	{"switch",	SWITCH},
	{"case",	CASE},
	{"break",	BREAK},
	{"continue",	CONTIN},
	{"do",		DO},
	{"default",	DEFAULT},
	{"for",		FOR},
	{"sizeof",	SIZEOF},
	{"typedef",	TYPEDEF},
	{"enum",	ENUM},
	/* Native header profile: these appear in the installed libc headers. */
	{"void",	VOIDKW},
	{"signed",	SIGNED},
	{"const",	QUALIF},
	{"volatile",	QUALIF},
	{"restrict",	QUALIF},
	{"__restrict",	QUALIF},
	{"__restrict__",QUALIF},
	{"inline",	QUALIF},
	{"__inline__",	QUALIF},
	{"_Noreturn",	QUALIF},
	{"__attribute__",ATTRIB},
	{"__builtin_va_list",VALIST},
	{"__builtin_offsetof",OFFSETOF},
	{"_Static_assert",SASSERT},
	{"__builtin_va_start",VASTART},
	{"__builtin_va_arg",VAARG},
	{"__builtin_va_end",VAEND},
	{"__builtin_va_copy",VACOPY},
	{0,		0},
};

/* Bounded tree/declaration arena replacing sbrk growth. */
#define	ARENA	(1024*1024)

int
main(int argc, char *argv[])
{
	register char *sp;
	register int i;
	register struct kwtab *ip;

	if(argc<4) {
		error("Arg count");
		exit(1);
	}
	if (freopen(argv[1], "r", stdin)==NULL) {
		error("Can't find %s", argv[1]);
		exit(1);
	}
	if (freopen(argv[2], "w", stdout)==NULL || (sbufp=fopen(argv[3],"w"))==NULL) {
		error("Can't create temp");
		exit(1);
	}
	if (argc>4)
		proflg++;
	/*
	 * The hash table locations of the keywords
	 * are marked; if an identifier hashes to one of
	 * these locations, it is looked up in in the keyword
	 * table first.
	 */
	for (ip=kwtab; (sp = ip->kwname); ip++) {
		i = 0;
		while (*sp)
			i += *sp++;
		hshtab[i%HSHSIZ].hflag = FKEYW;
	}
	for (i=128; i<256; i++)
		ctab[i] = UNKN;
	for (i=0; i<HSHSIZ; i++)
		hshtab[i].name = nullname;
	structhole.name = nullname;
#ifdef NX_SBRK_ARENA
	/*
	 * As in V7, the tree arena grows with sbrk.  Static stdio
	 * buffers keep the break contiguous with the arena.
	 */
	{
		static char ibuf[BUFSIZ], obuf[BUFSIZ], sbuf[BUFSIZ];

		setbuf(stdin, ibuf);
		setbuf(stdout, obuf);
		setbuf(sbufp, sbuf);
		setbuf(stderr, NULL);
		coremax = funcbase = sbrk(0);
		if (coremax == (char *)-1) {
			error("Out of space");
			exit(1);
		}
	}
#else
	if ((funcbase = malloc(ARENA)) == NULL) {
		error("Out of space");
		exit(1);
	}
	coremax = funcbase + ARENA;
#endif
	curbase = funcbase;
	while(!eof)
		extdef();
	outcode("B", EOFC);
	strflg++;
	outcode("B", EOFC);
	if (fflush(stdout) || fflush(sbufp) || ferror(stdout) || ferror(sbufp)) {
		error("Write error on temp");
		exit(1);
	}
	exit(nerror!=0);
}

/*
 * Look up the identifier in symbuf in the symbol table.
 * If it hashes to the same spot as a keyword, try the keyword table
 * first.
 * Return is a ptr to the symbol table entry.
 */
int
lookup(void)
{
	int ihash;
	register struct hshtab *rp;
	register char *sp;

	ihash = 0;
	sp = symbuf;
	while (*sp)
		ihash += *sp++&0377;
	rp = &hshtab[ihash%HSHSIZ];
	if (rp->hflag&FKEYW)
		if (findkw())
			return(KEYW);
	while (*rp->name) {
		if (strcmp(rp->name, symbuf)==0 && mossym == (rp->hflag&FMOS)) {
			csym = rp;
			return(NAME);
		}
		if (++rp >= &hshtab[HSHSIZ])
			rp = hshtab;
	}
	if (noinsert)
		return(0);
	if(++hshused >= HSHSIZ) {
		error("Symbol table overflow");
		exit(1);
	}
	rp->hclass = 0;
	rp->htype = 0;
	rp->hoffset = 0;
	rp->hsubsp = NULL;
	rp->hstrp = NULL;
	rp->hpdown = NULL;
	rp->hnext = NULL;
	rp->hblklev = blklev;
	rp->hflag |= mossym;
	rp->name = savename(symbuf);
	csym = rp;
	return(NAME);
}

/*
 * Symbol names live in a bounded pool; deleted names are not reused.
 */
#define	NAMEPOOL	(192*1024)
char	nullname[] = "";

char *
savename(const char *s)
{
	static char pool[NAMEPOOL];
	static int used;
	register int n;

	n = strlen(s) + 1;
	if (n > NAMEPOOL - used) {
		error("Too many names");
		exit(1);
	}
	memcpy(pool+used, s, n);
	used += n;
	return(pool + used - n);
}

/*
 * Search the keyword table.
 */
int
findkw(void)
{
	register struct kwtab *kp;

	for (kp=kwtab; kp->kwname; kp++) {
		if (strcmp(symbuf, kp->kwname)==0) {
			cval = kp->kwval;
			return(1);
		}
	}
	return(0);
}


/*
 * Return the next symbol from the input.
 * peeksym is a pushed-back symbol, peekc is a pushed-back
 * character (after peeksym).
 * mosflg means that the next symbol, if an identifier,
 * is a member of structure or a structure tag, and it
 * is kept in a separate name space from other identifiers.
 */
int
symbol(void)
{
	register int c;
	register char *sp;
	register int tline;

	if (peeksym>=0) {
		c = peeksym;
		peeksym = -1;
		if (c==NAME)
			mosflg = 0;
		return(c);
	}
	if (peekc) {
		c = peekc;
		peekc = 0;
	} else
		if (eof)
			return(EOFC);
		else
			c = getchar();
loop:
	if (c==EOF) {
		eof = 1;
		return(EOFC);
	}
	switch(ctab[c&0377]) {

	case SHARP:
		if ((c=symbol())!=CON) {
			error("Illegal #");
			return(c);
		}
		tline = cval;
		while (peekc!=EOF && ctab[peekc&0377]==SPACE)
			peekc = getchar();
		if (peekc=='"') {
			sp = filename;
			while ((c = mapch('"')) >= 0)
				if (sp < filename+sizeof(filename)-1)
					*sp++ = c;
			*sp++ = 0;
			peekc = getchar();
			strflg++;
			outcode("BS", CP_FILE, filename);
			strflg--;
			outcode("BS", CP_FILE, filename);
		}
		while (peekc!=EOF && ctab[peekc&0377]==SPACE)
			peekc = getchar();
		if (peekc != '\n') {
			error("Illegal #");
			while ((c = getchar())!='\n' && c!=EOF)
				;
		}
		peekc = 0;
		line = tline;
		return(symbol());

	case INSERT:		/* ignore newlines */
		inhdr = 1;
		c = getchar();
		goto loop;

	case NEWLN:
		if (!inhdr)
			line++;
		inhdr = 0;

	case SPACE:
		c = getchar();
		goto loop;

	case PLUS:
		return(subseq(c,PLUS,INCBEF));

	case MINUS:
		return(subseq(c,subseq('>',MINUS,ARROW),DECBEF));

	case ASSIGN:
		c = spnextchar();
		peekc = 0;
		if (c=='=')
			return(EQUAL);
		if (c==' ')
			return(ASSIGN);
		if (c=='<' || c=='>') {
			if (spnextchar() != c) {
				peeksym = ctab[c];
				return(ASSIGN);
			}
			peekc = 0;
			return(c=='<'? ASLSH: ASRSH);
		}
		if (ctab[c]>=PLUS && ctab[c]<=EXOR) {
			if (spnextchar() != ' '
			 && (c=='-' || c=='&' || c=='*')) {
				error("Warning: %c= operator assumed", c);
				nerror--;
			}
			c = ctab[c];
			return(c+ASPLUS-PLUS);
		}
		peekc = c;
		return(ASSIGN);

	case LESS:
		if (subseq(c,0,1)) return(LSHIFT);
		return(subseq('=',LESS,LESSEQ));

	case GREAT:
		if (subseq(c,0,1)) return(RSHIFT);
		return(subseq('=',GREAT,GREATEQ));

	case EXCLA:
		return(subseq('=',EXCLA,NEQUAL));

	case BSLASH:
		if (subseq('/', 0, 1))
			return(MAX);
		goto unkn;

	case DIVIDE:
		if (subseq('\\', 0, 1))
			return(MIN);
		if (subseq('*',1,0))
			return(DIVIDE);
		while ((c = spnextchar()) != EOF) {
			peekc = 0;
			if (c=='*') {
				if (spnextchar() == '/') {
					peekc = 0;
					c = getchar();
					goto loop;
				}
			}
		}
		eof = 1;
		error("Nonterminated comment");
		return(0);

	case PERIOD:
		/* The native header profile's "..." prototype marker */
		if ((peekc = getchar()) == '.') {
			if ((peekc = getchar()) != '.') {
				error("Unknown character");
				return(DOT);
			}
			peekc = 0;
			return(ELLIPS);
		}
		ungetc(peekc, stdin);
		peekc = c;
		return(getnum());

	case DIGIT:
		peekc = c;
		return(getnum());

	case DQUOTE:
		cval = isn++;
		return(STRING);

	case SQUOTE:
		return(getcc());

	case LETTER:
		sp = symbuf;
		while(ctab[c&0377]==LETTER || ctab[c&0377]==DIGIT) {
			if (sp<symbuf+NCPS)
				*sp++ = c;
			else if (sp==symbuf+NCPS) {
				error("Identifier too long");
				sp++;
			}
			c = getchar();
		}
		if (sp>symbuf+NCPS)
			sp = symbuf+NCPS;
		*sp = '\0';
		mossym = 0;
		if (mosflg) {
			mossym = FMOS;
			mosflg = 0;
		}
		peekc = c;
		if (mossym && memqual && findkw()) {
			memqual = NULL;
			return(cval==SIZEOF ? SIZEOF : KEYW);
		}
		if (mossym && memqual) {
			/*
			 * Native: members belong to their structure, as the
			 * installed headers require.  A reference whose
			 * structure lacks the name falls back to the V7 rule
			 * of one name space for all members.
			 */
			char plain[NCPS+2];

			strcpy(plain, symbuf);
			sprintf(symbuf+strlen(symbuf), "@%d", memqual->sid);
			if (!memdecl)
				noinsert++;
			c = lookup();
			noinsert = 0;
			memqual = NULL;
			if (c==NAME)
				return(c);
			strcpy(symbuf, plain);
		}
		memqual = NULL;
		if ((c=lookup())==KEYW && cval==SIZEOF)
			c = SIZEOF;
		return(c);

	case AND:
		return(subseq('&', AND, LOGAND));

	case OR:
		return(subseq('|', OR, LOGOR));

	case UNKN:
	unkn:
		error("Unknown character");
		c = getchar();
		goto loop;

	}
	return(ctab[c&0377]);
}

/*
 * Read a number.  Return kind.
 */
int
getnum(void)
{
	register char *np;
	register int c, base;
	int expseen, sym, ndigit, overflow, unsignf, longf;
	char *nsyn;
	int maxdigit;
	uint64_t uval;

	nsyn = "Number syntax";
	uval = 0;
	base = 10;
	maxdigit = 0;
	np = numbuf;
	ndigit = 0;
	sym = CON;
	expseen = 0;
	overflow = 0;
	unsignf = longf = 0;
	contype = INT;
	if ((c=spnextchar()) == '0')
		base = 8;
	for (;; c = getchar()) {
		if (np >= numbuf+sizeof(numbuf)-2) {
			error(nsyn);
			np = numbuf;
		}
		*np++ = c;
		if (ctab[c&0377]==DIGIT || (base==16) && ('a'<=c&&c<='f'||'A'<=c&&c<='F')) {
			if (ctab[c&0377]==DIGIT)
				c -= '0';
			else if (c>='a')
				c -= 'a'-10;
			else
				c -= 'A'-10;
			if (sym==CON) {
				if (uval > (UINT64_MAX - c) / base)
					overflow++;
				uval = uval * base + c;
			}
			ndigit++;
			if (c>maxdigit)
				maxdigit = c;
			continue;
		}
		if (c=='.') {
			if (base==16 || sym==FCON)
				error(nsyn);
			sym = FCON;
			base = 10;
			continue;
		}
		if (ndigit==0) {
			sym = DOT;
			break;
		}
		if ((c=='e'||c=='E') && expseen==0 && base!=16) {
			expseen++;
			sym = FCON;
			if (maxdigit>=10)
				error(nsyn);
			base = 10;
			*np++ = c = getchar();
			if (c!='+' && c!='-' && ctab[c&0377]!=DIGIT)
				break;
		} else if (c=='x' || c=='X') {
			if (base!=8 || uval!=0 || sym!=CON || ndigit!=1)
				error(nsyn);
			base = 16;
			ndigit = 0;
		} else if ((c=='l' || c=='L') && sym==CON) {
			longf++;
		} else if ((c=='u' || c=='U') && sym==CON) {
			unsignf++;
		} else
			break;
	}
	peekc = c;
	if (maxdigit >= base && sym != FCON)
		error(nsyn);
	if (overflow || longf>2 || unsignf>1)
		error(nsyn);
	if (sym==FCON) {
		np[-1] = 0;
		cval = np-numbuf;
		return(FCON);
	}
	if (sym==DOT)
		return(DOT);
	/*
	 * Native widths: int is 32 bits.  As in V7, decimal constants that
	 * do not fit a signed int and other constants that do not fit an
	 * unsigned int become long; the U suffix selects the unsigned type.
	 */
	lcval = (LTYPE)uval;
	if (!longf && (base==10 ? uval<=MAXINT || unsignf && uval<=MAXUINT : uval<=MAXUINT)) {
		contype = unsignf || uval>MAXINT ? UNSIGN : INT;
		cval = contype==UNSIGN ? (LTYPE)(uint32_t)uval : (LTYPE)(int32_t)uval;
		return(CON);
	}
	contype = unsignf || uval > INT64_MAX ? ULONG : LONG;
	return(LCON);
}

/*
 * If the next input character is c, return b and advance.
 * Otherwise push back the character and return a.
 */
int
subseq(int c, int a, int b)
{
	if (spnextchar() != c)
		return(a);
	peekc = 0;
	return(b);
}

/*
 * Write out a string, either in-line
 * or in the string temp file labelled by
 * lab.
 */
void
putstr(int lab, int amax)
{
	register int c, max;

	nchstr = 0;
	max = amax;
	if (lab) {
		strflg++;
		outcode("BNB", LABEL, lab, BDATA);
		max = 1<<30;
	} else
		outcode("B", BDATA);
	while ((c = mapch('"')) >= 0) {
		if (nchstr < max) {
			nchstr++;
			if (nchstr%15 == 0)
				outcode("0B", BDATA);
			outcode("1N", c & 0377);
		}
	}
	if (nchstr < max) {
		nchstr++;
		outcode("10");
	}
	outcode("0");
	strflg = 0;
}

/*
 * read a single-quoted character constant.
 * Multi-character constants pack their bytes in little-endian
 * order, as on the PDP-11; a single character is sign-extended.
 */
int
getcc(void)
{
	register int c, cc;
	uint32_t v;

	v = 0;
	cc = 0;
	while((c=mapch('\'')) >= 0)
		if(cc++ < LNCPW)
			v |= (uint32_t)(c&0377) << 8*(cc-1);
	if (cc>LNCPW)
		error("Long character constant");
	if (cc==1)
		cval = (signed char)v;
	else
		cval = (int32_t)v;
	contype = INT;
	return(CON);
}

/*
 * Read a character in a string or character constant,
 * detecting the end of the string.
 * It implements the escape sequences.
 */
int
mapch(int ac)
{
	register int a, c, n;
	static int mpeek;

	c = ac;
	if ((a = mpeek))
		mpeek = 0;
	else
		a = getchar();
loop:
	if (a==c)
		return(-1);
	switch(a) {

	case '\n':
	case '\0':
	case EOF:
		error("Nonterminated string");
		peekc = a==EOF ? 0 : a;
		if (a==EOF)
			eof = 1;
		return(-1);

	case '\\':
		switch (a=getchar()) {

		case 't':
			return('\t');

		case 'n':
			return('\n');

		case 'b':
			return('\b');

		case 'f':
			return('\014');

		case 'v':
			return('\013');

		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			n = 0;
			c = 0;
			while (++c<=3 && '0'<=a && a<='7') {
				n <<= 3;
				n += a-'0';
				a = getchar();
			}
			mpeek = a;
			return(n);

		case 'r':
			return('\r');

		case '\n':
			if (!inhdr)
				line++;
			inhdr = 0;
			a = getchar();
			goto loop;
		}
	}
	return(a);
}

/*
 * Read an expression and return a pointer to its tree.
 * It's the classical bottom-up, priority-driven scheme.
 * The initflg prevents the parse from going past
 * "," or ":" because those delimiters are special
 * in initializer (and some other) expressions.
 */
struct tnode *
tree(void)
{
	int *op, opst[SSIZE], *pp, prst[SSIZE];
	register int andflg, o;
	register struct hshtab *cs;
	int p, ps, os;
	struct tnode **cmst;
	struct tnode *lcp, *result;

	/*
	 * The operand stack outlives tree(): callers such as cinit and
	 * doret push onto it after the parse, so it is not automatic.
	 * A nested parse (a builtin's operand) continues above the outer one.
	 */
	cmst = treedepth ? cp : cmstack;
	treedepth++;
	curbase = funcbase;
	op = opst;
	pp = prst;
	cp = cmst;
	*op = SEOF;
	*pp = 06;
	andflg = 0;

advanc:
	switch (o=symbol()) {

	case NAME:
		cs = csym;
		if (cs->hclass==TYPEDEF)
			goto atype;
		if (cs->hclass==ENUMCON) {
			*cp++ = cblock(cs->hoffset);
			goto tand;
		}
		if (cs->hclass==0 && cs->htype==0)
			if(nextchar()=='(') {
				/* set function */
				cs->hclass = EXTERN;
				cs->htype = FUNC;
			} else {
				cs->hclass = STATIC;
				error("%s undefined; func. %s", cs->name, funcsym?funcsym->name:"");
				if (initflg) {
					cs->hclass = EXTERN;
					error("(Warning only)");
					nerror -= 2;
				}
			}
		*cp++ = nblock(cs);
		goto tand;

	case FCON:
		*cp++ = fblock(DOUBLE, copnum(cval));
		goto tand;

	case LCON:
		lcp = gblock(sizeof(*lcp));
		lcp->op = LCON;
		lcp->type = contype;
		lcp->lvalue = lcval;
		*cp++ = lcp;
		goto tand;

	case CON:
		*cp++ = cblock(cval);
		cp[-1]->type = contype;
		goto tand;

	/* fake a static char array */
	case STRING:
		putstr(cval, 0);
		cs = gblock(sizeof(*cs));
		memset(cs, 0, sizeof(*cs));
		cs->name = nullname;
		cs->hclass = STATIC;
		cs->hoffset = cval;
		*cp++ = block(NAME, ARRAY+CHAR, &nchstr, NULL, (struct tnode *)cs, NULL);
		cp[-1]->subsp = gblock(sizeof(int));
		*cp[-1]->subsp = nchstr;

	tand:
		if(cp>=cmstack+CMSIZ-4) {
			error("Expression overflow");
			exit(1);
		}
		if (andflg)
			goto syntax;
		andflg = 1;
		goto advanc;

	case KEYW:
		if (cval==OFFSETOF || cval==VASTART || cval==VAARG
		 || cval==VAEND || cval==VACOPY) {
			if (andflg)
				goto syntax;
			if ((*cp++ = builtin(cval)) == NULL) {
				treedepth--;
				return(0);
			}
			goto tand;
		}
	atype:
		if (*op != LPARN || andflg)
			goto syntax;
		peeksym = o;
		*cp++ = xprtype();
		if ((o=symbol()) != RPARN)
			goto syntax;
		o = CAST;
		--op;
		--pp;
		if (*op == SIZEOF) {
			andflg = 1;
			*pp = 100;
			goto advanc;
		}
		goto oponst;

	case INCBEF:
	case DECBEF:
		if (andflg)
			o += 2;
		goto oponst;

	case COMPL:
	case EXCLA:
	case SIZEOF:
		if (andflg)
			goto syntax;
		goto oponst;

	case MINUS:
		if (!andflg)
			o = NEG;
		andflg = 0;
		goto oponst;

	case AND:
	case TIMES:
		if (andflg)
			andflg = 0;
		else if (o==AND)
			o = AMPER;
		else
			o = STAR;
		goto oponst;

	case LPARN:
		if (andflg) {
			o = symbol();
			if (o==RPARN)
				o = MCALL;
			else {
				peeksym = o;
				o = CALL;
				andflg = 0;
			}
		}
		goto oponst;

	case RBRACK:
	case RPARN:
		if (!andflg)
			goto syntax;
		goto oponst;

	case DOT:
	case ARROW:
		mosflg++;
		memqual = NULL;
		memdecl = 0;
		if (cp > cmst && cp[-1]) {
			lcp = cp[-1];
			if (o==DOT && lcp->type==STRUCT || o==ARROW && lcp->type==PTR+STRUCT
			 || o==ARROW && (lcp->type&XTYPE)==ARRAY && (lcp->type&TYPE)==STRUCT && decref(lcp->type)==STRUCT)
				memqual = lcp->strp;
		}
		break;

	case ASSIGN:
		if (andflg==0 && PLUS<=*op && *op<=EXOR) {
			o = *op-- + ASPLUS - PLUS;
			pp--;
			goto oponst;
		}
		break;

	}
	/* binaries */
	if (andflg==0)
		goto syntax;
	andflg = 0;

oponst:
	p = (opdope[o]>>9) & 077;
opon1:
	ps = *pp;
	if (p>ps || p==ps && (opdope[o]&RASSOC)!=0) {
		switch (o) {

		case INCAFT:
		case DECAFT:
			p = 37;
			break;
		case LPARN:
		case LBRACK:
		case CALL:
			p = 04;
		}
		if (initflg) {
			if ((o==COMMA && *op!=LPARN && *op!=CALL)
			 || (o==COLON && *op!=QUEST)) {
				p = 00;
				goto opon1;
			}
		}
		if (op >= &opst[SSIZE-1]) {
			error("expression overflow");
			exit(1);
		}
		*++op = o;
		*++pp = p;
		goto advanc;
	}
	--pp;
	switch (os = *op--) {

	case SEOF:
		peeksym = o;
		build(0);		/* flush conversions */
		result = *--cp;
		treedepth--;
		return(result);

	case COMMA:
		if (*op != CALL)
			os = SEQNC;
		break;

	case CALL:
		if (o!=RPARN)
			goto syntax;
		build(os);
		goto advanc;

	case MCALL:
		*cp++ = NULL;	/* empty arglist */
		os = CALL;
		break;

	case INCBEF:
	case INCAFT:
	case DECBEF:
	case DECAFT:
		*cp++ = cblock(1);
		break;

	case LPARN:
		if (o!=RPARN)
			goto syntax;
		goto advanc;

	case LBRACK:
		if (o!=RBRACK)
			goto syntax;
		build(LBRACK);
		goto advanc;
	}
	build(os);
	goto opon1;

syntax:
	error("Expression syntax");
	errflush(o);
	treedepth--;
	return(0);
}

/*
 * Parse a type name.  The PDP-11 version overlaid the symbol entry on a
 * tree node; the native version returns an explicit ETYPE node.
 */
struct tnode *
xprtype(void)
{
	register struct hshtab *tyb;
	struct hshtab typer;
	int sc;
	register char *md, *fb;
	struct tnode **scp;

	tyb = gblock(sizeof(*tyb));
	memset(tyb, 0, sizeof(*tyb));
	tyb->name = nullname;
	fb = funcbase;
	md = maxdecl;
	scp = cp;
	funcbase = curbase;
	sc = DEFXTRN;		/* will cause error if class mentioned */
	getkeywords(&sc, &typer);
	tyb->hclass = 0;
	tyb->hblklev = 0;
	decl1(sc, &typer, 0, tyb);
	funcbase = fb;
	maxdecl = md;
	cp = scp;
	return(block(ETYPE, tyb->htype, tyb->hsubsp, tyb->hstrp, NULL, NULL));
}

char *
copnum(int len)
{
	register char *s1;

	s1 = gblock((len+LNCPW-1) & ~(LNCPW-1));
	strcpy(s1, numbuf);
	return(s1);
}
