/*
 *  C compiler
 */

#include "c1.h"
#include <stdarg.h>

int
max(int a, int b)
{
	if (a>b)
		return(a);
	return(b);
}

/* memory size of a type */
int
tsize(int t)
{
	if (t>=PTR)
		return(8);
	switch (t) {
	case CHAR: case UCHAR:
		return(1);
	case SHORT: case USHORT:
		return(2);
	case INT: case UNSIGN: case FLOAT:
		return(4);
	}
	return(8);
}

/* width of a value of the type held in a scratch register */
int
regwidth(int t)
{
	if (t>=PTR || t==LONG || t==ULONG)
		return(8);
	return(4);
}

int
isunsigned(int t)
{
	return(t==UNSIGN || t==UCHAR || t==USHORT || t==ULONG || t>=PTR);
}

int
isnarrow(int t)
{
	return(t==CHAR || t==UCHAR || t==SHORT || t==USHORT);
}

int
degree(struct tnode *at)
{
	register struct tnode *t, *t1;

	if ((t=at)==0 || t->op==0)
		return(0);
	if (t->op == CON)
		return(-3);
	if (t->op == AMPER)
		return(-2);
	if (t->op==ITOL) {
		if ((t1 = isconstant(t)) && (t1->value>=0 || t1->type==UNSIGN))
			return(-2);
		if ((t1=t->tr1)->type==UNSIGN && opdope[t1->op&0377]&LEAF)
			return(-1);
	}
	if ((opdope[t->op&0377] & LEAF) != 0) {
		if (isnarrow(t->type) || t->type==FLOAT)
			return(1);
		return(0);
	}
	return(t->degree);
}

/*
 * Print an operand in AT&T syntax.  Externals and statics use
 * absolute addresses (the native link places user text below 2 GiB).
 */
void
pname(struct tnode *ap, int flag)
{
	register struct tnode *p;
	LTYPE i;

	(void)flag;
	p = ap;
loop:
	switch(p->op) {

	case LCON:
		printf("$%ld", (long)p->value);
		return;

	case SFCON:
	case CON:
		putchar('$');
		if (tsize(p->type)<=4 && p->type<PTR)
			psoct((int32_t)p->value);
		else
			psoct(p->value);
		return;

	case FCON:
		printf(".L%d", (int)(p->value>0? p->value: -p->value));
		return;

	case NAME:
		i = p->offset;
		switch(p->class) {

		case OFFS:
			if (i)
				psoct(i);
			printf("(%s)", regname[0][p->regno]);
			return;

		case EXTERN:
		case STATIC:
			pbase(p);
			if (i > 0)
				printf("+%ld", (long)i);
			else if (i < 0)
				printf("-%ld", (long)-i);
			return;

		case REG:
			if (i)
				regerr();
			printf("%s", regname[tsize(p->type)==8?0:tsize(p->type)==4?1:tsize(p->type)==2?2:3][p->nloc]);
			return;

		}
		error("Compiler error: pname");
		return;

	case AMPER:
		putchar('$');
		p = p->tr1;
		if (p->op==NAME && (p->class==REG || p->class==OFFS))
			regerr();
		goto loop;

	}
	error("pname called illegally");
}

void
regerr(void)
{
	error("Illegal use of register");
}

void
pbase(struct tnode *ap)
{
	register struct tnode *p;

	p = ap;
	if (p->class==SOFFS || p->class==STATIC)
		printf(".L%d", p->nloc);
	else
		printf("%s", p->name);
}

int
xdcalc(struct tnode *ap, int nrleft)
{
	register struct tnode *p;
	register int d;

	p = ap;
	d = dcalc(p, nrleft);
	if (d<20 && isnarrow(p->type)) {
		if (nrleft>=1)
			d = 20;
		else
			d = 24;
	}
	return(d);
}

int
dcalc(struct tnode *ap, int nrleft)
{
	register struct tnode *p;

	if ((p=ap)==0)
		return(0);
	switch (p->op) {

	case NAME:
		if (p->class==REG)
			return(9);

	case AMPER:
	case FCON:
		return(12);

	case LCON:
	case CON:
	case SFCON:
		if (p->value==0)
			return(4);
		if (p->value==1)
			return(5);
		/* only sign-extended 32-bit constants are immediates */
		if (p->value >= INT32_MIN && p->value <= INT32_MAX || regwidth(p->type)==4 && p->op==CON)
			return(8);
		return(20);
	}
	return(p->degree <= nrleft? 20: 24);
}

/*
 * Native shape types: 0 integer or pointer, w (1) not narrow,
 * i (2) int or unsigned, b (3) narrow, f (4) floating, d (5) double,
 * s (6) structure, l (8) 64-bit integer or pointer, u (9) unsigned;
 * p (+16) excludes immediate operands.
 */
int
notcompat(struct tnode *ap, int ast, int op)
{
	register int at, st, intl;

	(void)op;
	at = ap->type;
	st = ast & 017;
	/* p: the operand is written or compared in place, not an immediate */
	if (ast & 020 && (ap->op==CON || ap->op==LCON || ap->op==AMPER || ap->op==FCON))
		return(1);
	if (ap->op==AMPER)
		at = PTR;
	intl = at>=PTR || at==INT || at==UNSIGN || at==LONG || at==ULONG || isnarrow(at);
	switch (st) {
	case 0: return(!intl);
	case 1: return(!intl || isnarrow(at));
	case 2: return(at!=INT && at!=UNSIGN);
	case 3: return(!isnarrow(at));
	case 4: return(at!=FLOAT && at!=DOUBLE);
	case 5: return(at!=DOUBLE);
	case 6: return(at!=STRUCT);
	case 8: return(!(at>=PTR || at==LONG || at==ULONG));
	case 9: return(!intl || !isunsigned(at));
	}
	return(1);
}

/*
 * Print the mnemonic for op; floating operations use the second
 * form.  Right shifts are arithmetic only for signed operands.
 */
void
prins(int op, int c, struct instab *itable, struct tnode *tree)
{
	register struct instab *insp;
	register char *ip;

	if (tree && (op==RSHIFT || op==ASRSH)) {
		printf(isunsigned(tree->tr1->type) ? "shr" : "sar");
		return;
	}
	if (tree && itable==instab && isfloat(tree))
		c = 1;
	for (insp=itable; insp->iop != 0; insp++) {
		if (insp->iop == op) {
			ip = c? insp->str2: insp->str1;
			if (ip==0)
				break;
			printf("%s", ip);
			return;
		}
	}
	error("No match' for op %d", op);
}

/*
 * Is the address an indexable register plus constant?  (The PDP-11
 * deferred forms are not machine addresses on AMD64.)
 */
int
collcon(struct tnode *ap)
{
	register int op;
	register struct tnode *p;

	p = ap;
	if (p->op==STAR)
		return(0);
	if (p->op==PLUS) {
		op = p->tr2->op;
		if (op==CON && p->tr2->value>=INT32_MIN && p->tr2->value<=INT32_MAX)
			return(1);
	}
	return(0);
}

int
isfloat(struct tnode *at)
{
	register struct tnode *t;

	t = at;
	if (t==0)
		return(0);
	if ((opdope[t->op&0377]&RELAT)!=0)
		t = t->tr1;
	if (t->type==FLOAT || t->type==DOUBLE) {
		nfloat = 1;
		return('f');
	}
	return(0);
}

/*
 * The PDP-11 needed odd/even register pairs; AMD64 does not.
 */
int
oddreg(struct tnode *t, int areg)
{
	(void)t;
	return(areg);
}

int
arlength(int t)
{
	(void)t;
	return(8);
}

/*
 * Code for switches: the value is in %eax.
 */

void
pswitch(struct swtab *afp, struct swtab *alp, int deflab)
{
	int ncase, i, j, tabs, worst, best;
	LTYPE range, k;
	register struct swtab *swp, *fp, *lp;
	int *poctab;

	fp = afp;
	lp = alp;
	if (fp==lp) {
		printf("\tjmp\t.L%d\n", deflab);
		return;
	}
	isn++;
	if (sort(fp, lp))
		return;
	ncase = lp-fp;
	lp--;
	range = lp->swval - fp->swval;
	/* direct switch */
	if (range>0 && range <= 3*ncase) {
		if (fp->swval)
			printf("\tsubl\t$%d,%%eax\n", (int)fp->swval);
		printf("\tcmpl\t$%d,%%eax\n\tja\t.L%d\n\tmovl\t%%eax,%%eax\n", (int)range, deflab);
		printf("\tmovq\t$.L%d,%%r11\n\tjmp\t*(%%r11,%%rax,8)\n\t.section\t.rodata\n\t.balign\t8\n.L%d:\n", isn, isn);
		isn++;
		for (k=fp->swval; ; k++) {
			if (k==fp->swval) {
				printf("\t.quad\t.L%d\n", fp->swlab);
				if (fp==lp)
					break;
				fp++;
			} else
				printf("\t.quad\t.L%d\n", deflab);
		}
		printf("\t.text\n");
		return;
	}
	/* simple switch */
	if (ncase<10) {
		for (fp = afp; fp<=lp; fp++)
			breq(fp->swval, fp->swlab);
		printf("\tjmp\t.L%d\n", deflab);
		return;
	}
	/* hash switch */
	best = INT32_MAX;
	tabs = 1;
	poctab = getblk(((ncase+2)/2) * sizeof(*poctab));
	for (i=ncase/4; i<=ncase/2; i++) {
		if (i==0)
			continue;
		for (j=0; j<i; j++)
			poctab[j] = 0;
		for (swp=fp; swp<=lp; swp++)
			poctab[(uint32_t)swp->swval%i]++;
		worst = 0;
		for (j=0; j<i; j++)
			if (poctab[j]>worst)
				worst = poctab[j];
		if (i*worst < best) {
			tabs = i;
			best = i*worst;
		}
	}
	i = isn++;
	printf("\tmovl\t$%d,%%ecx\n\txorl\t%%edx,%%edx\n\tdivl\t%%ecx\n", tabs);
	printf("\tmovq\t$.L%d,%%r11\n\tjmp\t*(%%r11,%%rdx,8)\n\t.section\t.rodata\n\t.balign\t8\n.L%d:\n", i, i);
	for (i=0; i<tabs; i++)
		printf("\t.quad\t.L%d\n", isn+i);
	printf("\t.text\n");
	for (i=0; i<tabs; i++) {
		printf(".L%d:\n", isn++);
		for (swp=fp; swp<=lp; swp++) {
			if ((uint32_t)swp->swval%tabs == (uint32_t)i)
				breq((uint32_t)swp->swval/tabs, swp->swlab);
		}
		printf("\tjmp\t.L%d\n", deflab);
	}
}

void
breq(LTYPE v, int l)
{
	if (v==0)
		printf("\ttestl\t%%eax,%%eax\n");
	else
		printf("\tcmpl\t$%d,%%eax\n", (int)v);
	printf("\tje\t.L%d\n", l);
}

int
sort(struct swtab *afp, struct swtab *alp)
{
	register struct swtab *cp, *fp, *lp;
	int intch;
	LTYPE t;

	fp = afp;
	lp = alp;
	while (fp < --lp) {
		intch = 0;
		for (cp=fp; cp<lp; cp++) {
			if (cp->swval == cp[1].swval) {
				error("Duplicate case (%ld)", (long)cp->swval);
				return(1);
			}
			if (cp->swval > cp[1].swval) {
				intch++;
				t = cp->swval;
				cp->swval = cp[1].swval;
				cp[1].swval = t;
				t = cp->swlab;
				cp->swlab = cp[1].swlab;
				cp[1].swlab = t;
			}
		}
		if (intch==0)
			break;
	}
	return(0);
}

int
ispow2(struct tnode *atree)
{
	register LTYPE d;
	register struct tnode *tree;

	tree = atree;
	if (!isfloat(tree) && tree->tr2->op==CON) {
		d = tree->tr2->value;
		if (d>1 && d<=(1L<<30) && (d&(d-1))==0)
			return(d);
	}
	return(0);
}

struct tnode *
pow2(struct tnode *atree)
{
	register int d, i;
	register struct tnode *tree;

	tree = atree;
	if ((d = ispow2(tree))) {
		for (i=0; (d>>=1)!=0; i++);
		tree->tr2->value = i;
		tree->tr2->type = INT;
		switch (tree->op) {

		case TIMES:
			tree->op = LSHIFT;
			break;

		case ASTIMES:
			tree->op = ASLSH;
			break;

		case DIVIDE:
			tree->op = ULSH;
			break;

		case ASDIV:
			tree->op = ASULSH;
			break;

		case MOD:
			tree->op = AND;
			tree->tr2->value = (1<<i)-1;
			break;

		case ASMOD:
			tree->op = ASAND;
			tree->tr2->value = (1<<i)-1;
			break;

		default:
			error("pow2 botch");
		}
		tree = optim(tree);
	}
	return(tree);
}

/*
 * Floating comparisons set the flags like unsigned ones; an unordered
 * result (NaN) satisfies only !=.
 */
static void
fbranch(int lbl, int op, int cond)
{
	static char *pos[] = { "e", "ne", "be", "b", "ae", "a" };
	static char *neg[] = { "ne", "e", "a", "ae", "b", "be" };
	int skip;

	if (op < EQUAL || op > GREAT)
		op = op - LESSEQP + LESSEQ;
	if (cond) {
		if (op==NEQUAL) {
			printf("\tjp\t.L%d\n\tjne\t.L%d\n", lbl, lbl);
			return;
		}
		skip = isn++;
		printf("\tjp\t.L%d\n\tj%s\t.L%d\n", skip, pos[op-EQUAL], lbl);
		label(skip);
		putchar('\n');
	} else {
		if (op==NEQUAL) {
			skip = isn++;
			printf("\tjp\t.L%d\n\tje\t.L%d\n", skip, lbl);
			label(skip);
			putchar('\n');
			return;
		}
		printf("\tjp\t.L%d\n\tj%s\t.L%d\n", lbl, neg[op-EQUAL], lbl);
	}
}

void
cbranch(struct tnode *atree, int albl, int cond, int areg)
{
	int l1, op;
	register int lbl, reg;
	register struct tnode *tree;

	lbl = albl;
	reg = areg;
again:
	if ((tree=atree)==0)
		return;
	switch(tree->op) {

	case LOGAND:
		if (cond) {
			cbranch(tree->tr1, l1=isn++, 0, reg);
			cbranch(tree->tr2, lbl, 1, reg);
			label(l1);
		} else {
			cbranch(tree->tr1, lbl, 0, reg);
			cbranch(tree->tr2, lbl, 0, reg);
		}
		return;

	case LOGOR:
		if (cond) {
			cbranch(tree->tr1, lbl, 1, reg);
			cbranch(tree->tr2, lbl, 1, reg);
		} else {
			cbranch(tree->tr1, l1=isn++, 1, reg);
			cbranch(tree->tr2, lbl, 0, reg);
			label(l1);
		}
		return;

	case EXCLA:
		cbranch(tree->tr1, lbl, !cond, reg);
		return;

	case SEQNC:
		rcexpr(tree->tr1, efftab, reg);
		atree = tree->tr2;
		goto again;

	case ITOL:
		tree = tree->tr1;
		break;
	}
	op = tree->op;
	/* constant conditions (no immediate-only compare on AMD64) */
	if (tree->op==CON || tree->op==LCON) {
		if ((tree->value!=0) == (cond!=0))
			branch(lbl, 0, 0);
		return;
	}
	if (opdope[op&0377]&RELAT
	 && tree->tr1->op==ITOL && tree->tr2->op==ITOL
	 && isunsigned(tree->tr1->tr1->type)==isunsigned(tree->tr2->tr1->type)
	 && regwidth(tree->tr1->tr1->type)==regwidth(tree->tr2->tr1->type)) {
		tree->tr1 = tree->tr1->tr1;
		tree->tr2 = tree->tr2->tr1;
	}
	if (isfloat(tree) && (opdope[op&0377]&RELAT)==0) {
		struct tnode *z = getblk(sizeof(*z));
		memset(z, 0, sizeof(*z));
		z->op = FCON;
		z->type = DOUBLE;
		z->value = isn++;
		tree = tnode(NEQUAL, INT, tree, z);
		tree = optim(tree);
		op = tree->op;
	}
	rcexpr(tree, cctab, reg);
	op = tree->op;
	if (isfloat(tree)) {
		fbranch(lbl, op, cond);
		return;
	}
	if ((opdope[op&0377]&RELAT)==0)
		op = NEQUAL+200;
	else {
		l1 = tree->tr2->op;
	 	if ((l1==CON || l1==SFCON) && tree->tr2->value==0)
			op += 200;		/* special for tests */
	}
	branch(lbl, op, !cond);
}

void
branch(int lbl, int aop, int c)
{
	register int op;

	if ((op=aop)) {
		struct instab *insp;
		for (insp=branchtab; insp->iop!=0 && insp->iop!=op; insp++)
			;
		if (insp->iop==0) {
			error("No branch for op %d", op);
			return;
		}
		if ((c? insp->str2: insp->str1)==0)
			return;		/* never */
		printf("\t%s", c? insp->str2: insp->str1);
	} else
		printf("\tjmp");
	printf("\t.L%d\n", lbl);
}

void
label(int l)
{
	printf(".L%d:", l);
}

void
popstk(int a)
{
	if (a)
		printf("\taddq\t$%d,%%rsp\n", a);
}

void
error(const char *s, ...)
{
	va_list ap;

	nerror++;
	if (filename[0])
		fprintf(stderr, "%s:", filename);
	fprintf(stderr, "%d: ", line);
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
	putc('\n', stderr);
}

void
psoct(LTYPE an)
{
	printf("%ld", (long)an);
}

/*
 * Read in an intermediate file.
 */
#define	STKS	512
void
getree(void)
{
	struct tnode *expstack[STKS];
	register struct tnode **sp;
	register int t, op;
	struct swtab *swp;
	struct tnode *np, *fp, *lp, *sap;
	int lbl, cond, lbl2, lbl3, n, i;
	char *s;
	static char *gpreg[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

	curbase = funcbase;
	sp = expstack;
	lbl2 = lbl3 = cond = 0;
	for (;;) {
		if (sp >= &expstack[STKS])
			error("Stack overflow botch");
		LTYPE w = geti();
		if ((w&CP_MARKMASK) != CP_MARK) {
			error("Intermediate file error");
			exit(1);
		}
		lbl = 0;
		switch(op = (int)(w & 0377)) {

	case SINIT:
		printf("\t.long\t%ld\n", (long)(uint32_t)geti());
		break;

	case EOFC:
		return;

	case BDATA:
		if (geti() == 1) {
			printf("\t.byte\t");
			for (;;)  {
				printf("%ld", (long)(geti()&0377));
				if (geti() != 1)
					break;
				printf(",");
			}
			printf("\n");
		}
		break;

	case PROG:
		printf("\t.text\n");
		break;

	case DATA:
		printf("\t.data\n");
		break;

	case BSS:
		printf("\t.bss\n");
		break;

	case SYMDEF:
		s = outname();
		if (s[0])
			printf("\t.globl\t%s\n", s);
		sfuncr.nloc = 0;
		break;

	case RETRN:
		printf("\tmovq\t-8(%%rbp),%%rbx\n\tmovq\t-16(%%rbp),%%r12\n\tmovq\t-24(%%rbp),%%r13\n");
		printf("\tmovq\t-32(%%rbp),%%r14\n\tmovq\t-40(%%rbp),%%r15\n\tleave\n\tret\n");
		break;

	case CSPACE:
		s = outname();
		t = geti();
		printf("\t.comm\t%s,%d,%d\n", s, t, (int)geti());
		break;

	case SSPACE:
		t = geti();
		if (t)
			printf("\t.zero\t%d\n", t);
		totspace += (unsigned)t;
		break;

	case EVEN:
		t = geti();
		if (t>1)
			printf("\t.balign\t%d\n", t);
		break;

	case SAVE:
		printf("\tpushq\t%%rbp\n\tmovq\t%%rsp,%%rbp\n\tpushq\t%%rbx\n\tpushq\t%%r12\n");
		printf("\tpushq\t%%r13\n\tpushq\t%%r14\n\tpushq\t%%r15\n");
		break;

	case SETSTK:
		t = geti();
		t = ((t + 15) & ~15) - 40;
		if (t > 0)
			printf("\tsubq\t$%d,%%rsp\n", t);
		break;

	case PROFIL:
		geti();
		printf("\tcall\tmcount\n");
		break;

	case SNAME:
	case ANAME:
	case RNAME:
		outname();
		geti();
		break;

	case CP_FILE:
		s = outname();
		snprintf(filename, sizeof(filename), "%s", s);
		break;

	case CP_PARAM:
		t = geti();
		i = geti();
		n = geti();
		geti();
		if (t==CP_PSSE)
			printf("\tmovsd\t%%xmm%d,%d(%%rbp)\n", i, n);
		else
			printf("\tmovq\t%s,%d(%%rbp)\n", gpreg[i], n);
		break;

	case CP_VASAVE:
		n = geti();
		geti();
		geti();
		geti();
		for (i = 0; i < 6; i++)
			printf("\tmovq\t%s,%d(%%rbp)\n", gpreg[i], n+8*i);
		for (i = 0; i < 8; i++)
			printf("\tmovsd\t%%xmm%d,%d(%%rbp)\n", i, n+48+16*i);
		break;

	case SWIT:
		t = geti();
		line = geti();
		curbase = funcbase;
		while(swp=getblk(sizeof(*swp)), (swp->swlab = geti()))
			swp->swval = geti();
		pswitch((struct swtab *)funcbase, swp, t);
		break;

	case C3BRANCH:		/* for fortran [sic] */
		lbl = geti();
		lbl2 = geti();
		lbl3 = geti();
		goto xpr;

	case CBRANCH:
		lbl = geti();
		cond = geti();

	case EXPR:
	xpr:
		line = geti();
		if (sp != &expstack[1]) {
			error("Expression input botch");
			exit(1);
		}
		nstack = 0;
		--sp;
		*sp = optim(*sp);
		if (op==CBRANCH)
			cbranch(*sp, lbl, cond, 0);
		else if (op==EXPR)
			rcexpr(*sp, efftab, 0);
		else {
			rcexpr(*sp, cctab, 0);
			printf("\tjg\t.L%d\n", lbl3);
			printf("\tjl\t.L%d\n\tjmp\t.L%d\n", lbl, lbl2);
		}
		if (nstack)
			error("Stack botch %d", nstack);
		curbase = funcbase;
		break;

	case NAME:
		t = geti();
		np = getblk(sizeof(*np));
		memset(np, 0, sizeof(*np));
		np->type = geti();
		if (t==EXTERN) {
			np->name = outname();
		} else {
			np->nloc = geti();
			if (t==REG)
				np->nloc += RREGVAR;
		}
		np->op = NAME;
		np->class = t;
		np->regno = 0;
		np->offset = 0;
		*sp++ = np;
		break;

	case CON:
		t = geti();
		*sp++ = tconst(geti(), t);
		break;

	case LCON:
		t = geti();
		lp = tconst(geti(), t);
		lp->op = LCON;
		*sp++ = lp;
		break;

	case FCON:
		t = geti();
		s = outname();
		fp = getblk(sizeof(*fp));
		memset(fp, 0, sizeof(*fp));
		fp->op = FCON;
		fp->type = t;
		fp->value = isn++;
		fp->fvalue = atof(s);
		*sp++ = fp;
		break;

	case FSEL:
		np = sp[-1];
		sp[-1] = tnode(FSEL, geti(), np, NULL);
		t = geti();
		sp[-1]->tr2 = tnode(COMMA, INT, tconst(geti(), INT), tconst(t, INT));
		break;

	case STRASG:
		sap = getblk(sizeof(*sap));
		memset(sap, 0, sizeof(*sap));
		sap->op = STRASG;
		sap->type = geti();
		sap->mask = geti();
		sap->tr1 = *--sp;
		sap->tr2 = NULL;
		*sp++ = sap;
		break;

	case NULLOP:
		*sp++ = tnode(0, 0, NULL, NULL);
		break;

	case LABEL:
		label(geti());
		putchar('\n');
		break;

	case NLABEL:
		s = outname();
		printf("%s:\n", s);
		break;

	case RLABEL:
		s = outname();
		printf("\t.type\t%s,@function\n%s:\n", s, s);
		break;

	case BRANCH:
		branch(geti(), 0, 0);
		break;

	case SETREG:
		geti();
		break;

	case ITOC:
		t = geti();
		sp[-1] = tnode(ITOC, t, sp[-1], NULL);
		sp[-1]->value = geti();
		break;

	case CP_VASTART:
		t = geti();
		sp[-1] = tnode(CP_VASTART, t, sp[-1], NULL);
		sp[-1]->nloc = geti();
		sp[-1]->value = geti();
		sp[-1]->offset = geti();
		sp[-1]->mask = geti();
		break;

	case CP_VAARG:
	case CP_VAEND:
		sp[-1] = tnode(op, geti(), sp[-1], NULL);
		break;

	case CP_VACOPY:
		if (sp < &expstack[2]) {
			error("Binary expression botch");
			exit(1);
		}
		np = *--sp;
		sp[-1] = tnode(op, geti(), sp[-1], np);
		break;

	default:
		if (op > STRSET) {
			error("Intermediate operator %d", op);
			exit(1);
		}
		if (opdope[op]&BINARY) {
			if (sp < &expstack[2]) {
				error("Binary expression botch");
				exit(1);
			}
			np = *--sp;
			sp[-1] = tnode(op, geti(), sp[-1], np);
		} else {
			if (sp < &expstack[1]) {
				error("Unary expression botch");
				exit(1);
			}
			sp[-1] = tnode(op, geti(), sp[-1], NULL);
		}
		break;
	}
	}
}

LTYPE
geti(void)
{
	register int i, c;
	uint64_t w;

	w = 0;
	for (i = 0; i < 8; i++) {
		if ((c = getchar()) == EOF) {
			error("Intermediate file truncated");
			exit(1);
		}
		w |= (uint64_t)(c&0377) << 8*i;
	}
	return((LTYPE)w);
}

char *
outname(void)
{
	register char *p, *s;
	register int c, n;

	s = p = getblk(1024);
	n = 0;
	while ((c = getchar())) {
		if (c==EOF || ++n >= 1024) {
			error("Intermediate file error");
			exit(1);
		}
		*p++ = c;
	}
	*p = 0;
	return(s);
}

/*
 * Structure assignment: copy the bytes with rep movsb, saving the
 * string registers in case they hold live values.
 */
void
strasg(struct tnode *atp)
{
	register struct tnode *tp;
	register int r;
	LTYPE nbytes;

	nbytes = atp->mask;
	tp = atp->tr1;
	if (tp->op != ASSIGN) {
		if (tp->op==CALL) {
			rcexpr(tp, efftab, 0);
			return;
		}
		error("Illegal structure operation");
		return;
	}
	tp->tr2 = strfunc(tp->tr2);
	if (tp->tr1->op!=NAME && tp->tr1->op!=STAR
	 || tp->tr2->op!=NAME && tp->tr2->op!=STAR) {
		error("unimplemented structure assignment");
		return;
	}
	rcexpr(optim(tnode(AMPER, STRUCT+PTR, tp->tr1, NULL)), sptab, 0);
	r = rcexpr(optim(tnode(AMPER, STRUCT+PTR, tp->tr2, NULL)), regtab, 0);
	printf("\tmovq\t%s,%%r11\n\tpushq\t%%rsi\n\tpushq\t%%rdi\n\tpushq\t%%rcx\n", regname[0][r]);
	printf("\tmovq\t%%r11,%%rsi\n\tmovq\t24(%%rsp),%%rdi\n\tmovq\t$%ld,%%rcx\n\tcld\n\trep movsb\n", (long)nbytes);
	printf("\tpopq\t%%rcx\n\tpopq\t%%rdi\n\tpopq\t%%rsi\n\taddq\t$8,%%rsp\n");
	nstack--;
}

void
setype(struct tnode *p, int t)
{
	for (;; p = p->tr1) {
		p->type = t;
		if (p->op==AMPER)
			t = decref(t);
		else if (p->op==STAR)
			t = incref(t);
		else if (p->op==ASSIGN)
			setype(p->tr2, t);
		else if (p->op!=PLUS)
			break;
	}
}

/*
 * Reduce the degree-of-reference by one.
 * e.g. turn "ptr-to-int" into "int".
 */
int
decref(int at)
{
	register int t;

	t = at;
	if ((t & ~TYPE) == 0) {
		error("Illegal indirection");
		return(t);
	}
	return((int)((unsigned)t>>TYLEN) & ~TYPE | t&TYPE);
}

/*
 * Increase the degree of reference by
 * one; e.g. turn "int" to "ptr-to-int".
 */
int
incref(int t)
{
	return(((t&~TYPE)<<TYLEN) | (t&TYPE) | PTR);
}
