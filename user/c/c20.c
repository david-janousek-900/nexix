/*
 *	 C object code improver
 *
 *	Native AMD64 retargeting: the jump graph optimizations of the
 *	supplied c2 run on AT&T assembly; instructions are kept as text
 *	and jumps, labels and segments are parsed.
 */

#include "c2.h"

struct optab optab[] = {
	{"jmp",	JBR},
	{"je",	CBR | JEQ<<8},
	{"jz",	CBR | JEQ<<8},
	{"jne",	CBR | JNE<<8},
	{"jnz",	CBR | JNE<<8},
	{"jle",	CBR | JLE<<8},
	{"jge",	CBR | JGE<<8},
	{"jl",	CBR | JLT<<8},
	{"jg",	CBR | JGT<<8},
	{"jb",	CBR | JLO<<8},
	{"ja",	CBR | JHI<<8},
	{"jbe",	CBR | JLOS<<8},
	{"jae",	CBR | JHIS<<8},
	{"jp",	CBR | JP<<8},
	{"jnp",	CBR | JNP<<8},
	{"js",	CBR | JS<<8},
	{"jns",	CBR | JNS<<8},
	{".globl",EROU},
	{"movb",	MOV | 1<<8},
	{"movw",	MOV | 2<<8},
	{"movl",	MOV | 4<<8},
	{"movq",	MOV | 8<<8},
	{"call",	CALL},
	{"ret",	RET},
	{".text",TEXT},
	{".data",DATA},
	{".bss",	BSS},
	{".section",DATA},
	{0,	0}};

/* jump mnemonics by condition, for output */
static char *cbrname[] = { "je", "jne", "jle", "jge", "jl", "jg", "jb", "ja", "jbe", "jae", "jp", "jnp", "js", "jns" };
char	revbr[] = { JNE, JEQ, JGT, JLT, JGE, JLE, JHIS, JLOS, JHI, JLO, JNP, JP, JNS, JS };
int	isn	= C2LABELS;
int	lastseg	= -1;

char	line[LSIZE];
struct	node	first;
char	*curlp;
int	nbrbr;
int	redunm;
int	iaftbr;
int	njp1;
int	nrlab;
int	nxjump;
int	ncmot;
int	nrevbr;
int	loopiv;
int	nredunj;
int	nskip;
int	ncomj;
int	nchange;
int	debug;
static char *linetext;	/* the whole input line of the current node */
extern char *lastsegcode;

int
main(int argc, char **argv)
{
	register int niter, maxiter, isend;
	int nflag;

	if (argc>1 && argv[1][0]=='+') {
		argc--;
		argv++;
		debug++;
	}
	nflag = 0;
	if (argc>1 && argv[1][0]=='-') {
		argc--;
		argv++;
		nflag++;
	}
	if (argc>1) {
		if (freopen(argv[1], "r", stdin) == NULL) {
			fprintf(stderr, "C2: can't find %s\n", argv[1]);
			exit(1);
		}
	}
	if (argc>2) {
		if (freopen(argv[2], "w", stdout) == NULL) {
			fprintf(stderr, "C2: can't create %s\n", argv[2]);
			exit(1);
		}
	}
	maxiter = 0;
	opsetup();
	do {
		isend = input();
		movedat();
		niter = 0;
		do {
			refcount();
			do {
				iterate();
				clearreg();
				niter++;
			} while (nchange);
			comjump();
			rmove();
		} while (nchange || jumpsw());
		output();
		if (niter > maxiter)
			maxiter = niter;
	} while (isend);
	if (nflag) {
		fprintf(stderr, "%d iterations\n", maxiter);
		fprintf(stderr, "%d jumps to jumps\n", nbrbr);
		fprintf(stderr, "%d inst. after jumps\n", iaftbr);
		fprintf(stderr, "%d jumps to .+1\n", njp1);
		fprintf(stderr, "%d redundant labels\n", nrlab);
		fprintf(stderr, "%d cross-jumps\n", nxjump);
		fprintf(stderr, "%d code motions\n", ncmot);
		fprintf(stderr, "%d branches reversed\n", nrevbr);
		fprintf(stderr, "%d redundant moves\n", redunm);
		fprintf(stderr, "%d loops inverted\n", loopiv);
		fprintf(stderr, "%d common seqs before jmp's\n", ncomj);
		fprintf(stderr, "%d skips over jumps\n", nskip);
	}
	if (fflush(stdout) || ferror(stdout)) {
		fprintf(stderr, "C2: write error\n");
		exit(1);
	}
	exit(0);
}

int
input(void)
{
	register struct node *p, *lastp;
	register int oper;

	lastp = &first;
	first.forw = 0;
	for (;;) {
		oper = getline2();
		switch (oper&0377) {

		case LABEL:
			p = alloc(sizeof first);
			if (line[0]=='.' && line[1]=='L' && (p->labno = getnum(line+2))) {
				p->op = LABEL;
				p->subop = 0;
				p->code = 0;
			} else {
				p->op = DLABEL;
				p->subop = 0;
				p->labno = 0;
				p->code = copy(1, line, NULL);
			}
			break;

		case JBR:
		case CBR:
		case JSW:
			p = alloc(sizeof first);
			p->op = oper&0377;
			p->subop = oper>>8;
			if (curlp[0]=='.' && curlp[1]=='L' && (p->labno = getnum(curlp+2)))
				p->code = 0;
			else {
				p->labno = 0;
				p->op = oper==JSW ? 0 : JMP;
				p->code = copy(1, linetext, NULL);
			}
			break;

		default:
			p = alloc(sizeof first);
			p->op = oper&0377;
			p->subop = oper>>8;
			p->labno = 0;
			p->code = copy(1, linetext, NULL);
			break;

		}
		p->forw = 0;
		p->back = lastp;
		lastp->forw = p;
		lastp = p;
		p->ref = 0;
		if (oper==EROU)
			return(1);
		if (oper==END)
			return(0);
	}
}

/*
 * Read one statement: a label (ending with ':') or a line.
 * Leading white space is dropped from labels only.
 */
int
getline2(void)
{
	register char *lp, *s;
	register int c;
	static int pending;	/* a statement followed a label on its line */

	lp = line;
	c = getchar();
	if (pending)
		while (c==' ' || c=='\t')
			c = getchar();
	pending = 0;
	if (c==EOF) {
		line[0] = 0;
		linetext = line;
		curlp = line;
		return(END);
	}
	do {
		if (c==':') {
			/* a label: one token at the start of the line */
			for (s = line; *s==' ' || *s=='\t'; s++)
				;
			if (*s && strchr(s, ' ')==NULL && strchr(s, '\t')==NULL && strchr(s, '"')==NULL) {
				memmove(line, s, strlen(s)+1);
				if ((c = getchar()) != '\n' && c != EOF) {
					ungetc(c, stdin);
					pending = 1;
				}
				return(LABEL);
			}
		}
		if (c=='\n') {
			*lp++ = 0;
			return(oplook());
		}
		if (lp >= &line[LSIZE-2]) {
			fprintf(stderr, "C2: Sorry, input line too long\n");
			exit(1);
		}
		*lp++ = c;
		*lp = 0;
	} while ((c = getchar()) != EOF);
	*lp++ = 0;
	return(oplook()==EROU ? EROU : END);
}

int
getnum(char *ap)
{
	register char *p;
	register int n, c;

	p = ap;
	n = 0;
	while ((c = *p++) >= '0' && c <= '9') {
		if (n > 214748363)
			return(0);
		n = n*10 + c - '0';
	}
	if (*--p != 0)
		return(0);
	return(n);
}

void
output(void)
{
	register struct node *t;

	t = &first;
	while ((t = t->forw)) switch (t->op) {

	case END:
		if (t->code)
			printf("%s\n", t->code);
		return;

	case LABEL:
		printf(".L%d:\n", t->labno);
		continue;

	case DLABEL:
		printf("%s:\n", t->code);
		continue;

	case JBR:
		printf("\tjmp\t.L%d\n", t->labno);
		continue;

	case CBR:
		printf("\t%s\t.L%d\n", cbrname[t->subop], t->labno);
		continue;

	case JSW:
		printf("\t.quad\t.L%d\n", t->labno);
		continue;

	case TEXT:
	case DATA:
	case BSS:
		lastseg = t->op;
		lastsegcode = t->code;

	default:
		if (t->code)
			printf("%s\n", t->code);
		continue;
	}
}

char *
copy(int na, char *ap, char *bp)
{
	register char *np;
	size_t n;

	if (*ap==0 && (na<2 || *bp==0))
		return(0);
	n = strlen(ap) + 1 + (na>1 ? strlen(bp) : 0);
	np = alloc(n);
	strcpy(np, ap);
	if (na>1)
		strcat(np, bp);
	return(np);
}

void
opsetup(void)
{
}

/*
 * Classify a line by its mnemonic.  Instructions not named in optab
 * are INSN; other directives are 0 and are never deleted.
 */
#define	INSN	13
int
oplook(void)
{
	register struct optab *optp;
	register char *lp, *np;
	static char tmpop[64];

	linetext = line;
	lp = line;
	while (*lp==' ' || *lp=='\t')
		lp++;
	if (*lp=='\0') {
		curlp = lp;
		return(0);
	}
	np = tmpop;
	while (*lp && *lp!=' ' && *lp!='\t' && np < tmpop+sizeof(tmpop)-1)
		*np++ = *lp++;
	*np = 0;
	while (*lp=='\t' || *lp==' ')
		lp++;
	curlp = lp;
	for (optp = optab; optp->opstring; optp++)
		if (strcmp(tmpop, optp->opstring)==0) {
			if ((optp->opcode&0377)==JBR && *curlp=='*')
				return(JMP);
			return(optp->opcode);
		}
	if (strcmp(tmpop, ".quad")==0 && curlp[0]=='.' && curlp[1]=='L'
	 && getnum(curlp+2) && strchr(curlp, ',')==NULL)
		return(JSW);
	if (tmpop[0]=='.')
		return(0);
	return(INSN);
}

void
refcount(void)
{
	register struct node *p, *lp;
	static struct node *labhash[LABHS];
	register struct node **hp, *tp;

	for (hp = labhash; hp < &labhash[LABHS];)
		*hp++ = 0;
	for (p = first.forw; p!=0; p = p->forw)
		if (p->op==LABEL) {
			labhash[p->labno % LABHS] = p;
			p->refc = 0;
		}
	for (p = first.forw; p!=0; p = p->forw) {
		if (p->op==JBR || p->op==CBR || p->op==JSW) {
			p->ref = 0;
			lp = labhash[p->labno % LABHS];
			if (lp==0 || p->labno!=lp->labno)
			for (lp = first.forw; lp!=0; lp = lp->forw) {
				if (lp->op==LABEL && p->labno==lp->labno)
					break;
			}
			if (lp) {
				tp = nonlab(lp)->back;
				if (tp!=lp) {
					p->labno = tp->labno;
					lp = tp;
				}
				p->ref = lp;
				lp->refc++;
			}
		}
	}
	/*
	 * Labels not referenced within the block are removed; c1 and
	 * ccom labels are local to one routine, except those referenced
	 * from data, which refcount saw above.
	 */
	for (p = first.forw; p!=0; p = p->forw)
		if (p->op==LABEL && p->refc==0
		 && (lp = nonlab(p)) && lp->op && lp->op!=JSW)
			decref(p);
}

void
iterate(void)
{
	register struct node *p, *rp, *p1;

	nchange = 0;
	for (p = first.forw; p!=0; p = p->forw) {
		if ((p->op==JBR||p->op==CBR||p->op==JSW) && p->ref) {
			rp = nonlab(p->ref);
			if (rp && rp->op==JBR && rp->labno && p->labno!=rp->labno && rp->ref) {
				nbrbr++;
				p->labno = rp->labno;
				decref(p->ref);
				rp->ref->refc++;
				p->ref = rp->ref;
				nchange++;
			}
		}
		if (p->op==CBR && (p1 = p->forw) && p1->op==JBR && p->ref) {
			rp = p->ref;
			do
				rp = rp->back;
			while (rp->op==LABEL);
			if (rp==p1) {
				decref(p->ref);
				p->ref = p1->ref;
				p->labno = p1->labno;
				p1->forw->back = p;
				p->forw = p1->forw;
				p->subop = revbr[p->subop];
				nchange++;
				nskip++;
			}
		}
		if (p->op==JBR || p->op==JMP || p->op==RET) {
			while (p->forw && p->forw->op!=LABEL
				&& p->forw->op!=DLABEL
				&& p->forw->op!=EROU && p->forw->op!=END
				&& p->forw->op!=0 && p->forw->op!=DATA
				&& p->forw->op!=TEXT && p->forw->op!=BSS
				&& p->forw->op!=JSW) {
				nchange++;
				iaftbr++;
				if (p->forw->ref)
					decref(p->forw->ref);
				p->forw = p->forw->forw;
				if (p->forw)
					p->forw->back = p;
			}
			rp = p->forw;
			while (p->op==JBR && rp && rp->op==LABEL) {
				if (p->ref == rp) {
					p->back->forw = p->forw;
					p->forw->back = p->back;
					p = p->back;
					decref(rp);
					nchange++;
					njp1++;
					break;
				}
				rp = rp->forw;
			}
		}
		if (p->op==JBR || p->op==JMP) {
			xjump(p);
			p = codemove(p);
		}
	}
}

void
xjump(struct node *p1)
{
	register struct node *p2, *p3;

	if ((p2 = p1->ref)==0)
		return;
	for (;;) {
		while ((p1 = p1->back) && p1->op==LABEL);
		while ((p2 = p2->back) && p2->op==LABEL);
		if (p1==0 || p2==0 || !equop(p1, p2) || p1==p2)
			return;
		p3 = insertl(p2);
		p1->op = JBR;
		p1->subop = 0;
		p1->ref = p3;
		p1->labno = p3->labno;
		p1->code = 0;
		nxjump++;
		nchange++;
	}
}

struct node *
insertl(struct node *oldp)
{
	register struct node *lp;

	if (oldp->op == LABEL) {
		oldp->refc++;
		return(oldp);
	}
	if (oldp->back->op == LABEL) {
		oldp = oldp->back;
		oldp->refc++;
		return(oldp);
	}
	lp = alloc(sizeof first);
	lp->op = LABEL;
	lp->subop = 0;
	lp->labno = isn++;
	lp->ref = 0;
	lp->code = 0;
	lp->refc = 1;
	lp->back = oldp->back;
	lp->forw = oldp;
	oldp->back->forw = lp;
	oldp->back = lp;
	return(lp);
}

struct node *
codemove(struct node *p)
{
	register struct node *p1, *p2, *p3;
	struct node *t, *tl;
	int n;

	p1 = p;
	if (p1->op!=JBR || (p2 = p1->ref)==0)
		return(p1);
	while (p2->op == LABEL)
		if ((p2 = p2->back) == 0)
			return(p1);
	if (p2->op!=JBR && p2->op!=JMP && p2->op!=RET)
		goto ivloop;
	p2 = p2->forw;
	p3 = p1->ref;
	while (p3) {
		if (p3->op==JBR || p3->op==JMP || p3->op==RET) {
			if (p1==p3)
				return(p1);
			ncmot++;
			nchange++;
			p1->back->forw = p2;
			p1->forw->back = p3;
			p2->back->forw = p3->forw;
			p3->forw->back = p2->back;
			p2->back = p1->back;
			p3->forw = p1->forw;
			decref(p1->ref);
			return(p2);
		} else if (p3->op==0 || p3->op==DLABEL || p3->op==EROU || p3->op==END
		 || p3->op==DATA || p3->op==TEXT || p3->op==BSS || p3->op==JSW)
			return(p1);	/* do not move directives or data */
		else
			p3 = p3->forw;
	}
	return(p1);
ivloop:
	if (p1->forw==0 || p1->forw->op!=LABEL)
		return(p1);
	p3 = p2 = p2->forw;
	n = 16;
	do {
		if ((p3 = p3->forw) == 0 || p3==p1 || --n==0 || p3->op==0 || p3->op==DLABEL)
			return(p1);
	} while (p3->op!=CBR || p3->labno!=p1->forw->labno);
	do
		if ((p1 = p1->back) == 0)
			return(p);
	while (p1!=p3);
	p1 = p;
	tl = insertl(p1);
	p3->subop = revbr[p3->subop];
	decref(p3->ref);
	p2->back->forw = p1;
	p3->forw->back = p1;
	p1->back->forw = p2;
	p1->forw->back = p3;
	t = p1->back;
	p1->back = p2->back;
	p2->back = t;
	t = p1->forw;
	p1->forw = p3->forw;
	p3->forw = t;
	p2 = insertl(p1->forw);
	p3->labno = p2->labno;
	p3->ref = p2;
	decref(tl);
	if (tl->refc<=0)
		nrlab--;
	loopiv++;
	nchange++;
	return(p3);
}

void
comjump(void)
{
	register struct node *p1, *p2, *p3;

	for (p1 = first.forw; p1!=0; p1 = p1->forw)
		if (p1->op==JBR && (p2 = p1->ref) && p2->refc > 1)
			for (p3 = p1->forw; p3!=0; p3 = p3->forw)
				if (p3->op==JBR && p3->ref == p2)
					backjmp(p1, p3);
}

void
backjmp(struct node *ap1, struct node *ap2)
{
	register struct node *p1, *p2, *p3;

	p1 = ap1;
	p2 = ap2;
	for(;;) {
		while ((p1 = p1->back) && p1->op==LABEL);
		p2 = p2->back;
		if (p1 && p2 && p1!=p2 && equop(p1, p2)) {
			p3 = insertl(p1);
			p2->back->forw = p2->forw;
			p2->forw->back = p2->back;
			p2 = p2->forw;
			decref(p2->ref);
			p2->labno = p3->labno;
			p2->ref = p3;
			nchange++;
			ncomj++;
		} else
			return;
	}
}
