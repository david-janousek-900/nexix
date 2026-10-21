/*
 * C compiler
 *
 *
 */

#include "c0.h"
#include <stdarg.h>

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
	if (t&BIGTYPE) {
		typov();
		return(t);
	}
	return(((t&~TYPE)<<TYLEN) | (t&TYPE) | PTR);
}

/*
 * Make a tree that causes a branch to lbl
 * if the tree's value is non-zero together with the cond.
 */
void
cbranch(struct tnode *t, int lbl, int cond)
{
	treeout(t, 0);
	outcode("BNNN", CBRANCH, lbl, cond, line);
}

/*
 * Write out a tree.
 */
void
rcexpr(struct tnode *atp)
{
	register struct tnode *tp;

	if ((tp=atp)==0)
		return;
	/*
	 * Special optimization
	 */
	if (tp->op==INIT && tp->tr1->op==CON) {
		if (tp->type==CHAR || tp->type==UCHAR) {
			outcode("B1L0", BDATA, tp->tr1->value);
			return;
		} else if (tp->type==INT || tp->type==UNSIGN) {
			outcode("BL", SINIT, tp->tr1->value);
			return;
		}
	}
	treeout(tp, 0);
	outcode("BN", EXPR, line);
}

/*
 * A va_list argument is the decayed pointer to its single element.
 */
static struct tnode *
vaobject(struct tnode *p)
{
	p = disarray(p);
	if (p->type!=PTR+STRUCT || p->strp!=&vastr)
		error("va_list required");
	return(p);
}

void
treeout(struct tnode *atp, int isstruct)
{
	register struct tnode *tp;
	register struct hshtab *hp;
	register int nextisstruct;

	if ((tp = atp) == 0) {
		outcode("B", NULLOP);
		return;
	}
	nextisstruct = tp->type==STRUCT;
	switch(tp->op) {

	case NAME:
		hp = (struct hshtab *)tp->tr1;
		if (hp->hclass==TYPEDEF)
			error("Illegal use of type name");
		outcode("BNN", NAME, hp->hclass==0?STATIC:hp->hclass, tp->type);
		if (hp->hclass==EXTERN)
			outcode("S", hp->name);
		else
			outcode("N", hp->hoffset);
		break;

	case LCON:
		outcode("BNL", tp->op, tp->type, tp->lvalue);
		break;

	case CON:
		outcode("BNL", tp->op, tp->type, tp->value);
		break;

	case FCON:
		outcode("BNF", tp->op, tp->type, tp->cstr);
		break;

	case FSEL:
		treeout(tp->tr1, nextisstruct);
		outcode("BNNN",tp->op,tp->type,((struct field *)tp->tr2)->bitoffs,((struct field *)tp->tr2)->flen);
		break;

	case ETYPE:
		error("Illegal use of type");
		break;

	case AMPER:
		treeout(tp->tr1, 1);
		outcode("BN", tp->op, tp->type);
		break;


	case CALL:
		treeout(tp->tr1, 1);
		treeout(tp->tr2, 0);
		outcode("BN", CALL, tp->type);
		break;

	case ITOC:
		treeout(tp->tr1, 0);
		outcode("BNN", ITOC, tp->type, (int)tp->value);
		break;

	case CP_VASTART:
		treeout(tp->tr1, 0);
		outcode("BNNNNN", CP_VASTART, tp->type, vasave, vagp, vafp, vaover);
		break;

	case CP_VAARG:
	case CP_VAEND:
		treeout(tp->tr1, 0);
		outcode("BN", tp->op, tp->type);
		break;

	case CP_VACOPY:
		treeout(tp->tr1, 0);
		treeout(tp->tr2, 0);
		outcode("BN", tp->op, tp->type);
		break;

	default:
		treeout(tp->tr1, nextisstruct);
		if (opdope[tp->op]&BINARY)
			treeout(tp->tr2, nextisstruct);
		outcode("BN", tp->op, tp->type);
		break;
	}
	if (nextisstruct && isstruct==0)
		outcode("BNN", STRASG, STRUCT, tp->strp->ssize);
}

/*
 * Generate a branch
 */
void
branch(int lab)
{
	outcode("BN", BRANCH, lab);
}

/*
 * Generate a label
 */
void
label(int l)
{
	outcode("BN", LABEL, l);
}

/*
 * ap is a tree node whose type
 * is some kind of pointer; return the size of the object
 * to which the pointer points.
 */
int
plength(struct tnode *ap)
{
	register int t, l;
	register struct tnode *p;

	p = ap;
	if (p==0 || ((t=p->type)&~TYPE) == 0)		/* not a reference */
		return(1);
	p->type = decref(t);
	l = length(p);
	p->type = t;
	return(l);
}

/*
 * return the number of bytes in the object
 * whose tree node is acs.
 */
int
length(struct tnode *acs)
{
	register int t, elsz;
	int64_t n;
	register struct tnode *cs;
	int *subsp;
	struct str *strp;
	int nd;

	cs = acs;
	t = cs->type;
	subsp = cs->subsp;
	strp = cs->strp;
	n = 1;
	nd = 0;
	while ((t&XTYPE) == ARRAY) {
		t = decref(t);
		if (subsp==NULL) {
			error("Compiler error (dimension)");
			return(0);
		}
		n *= subsp[nd++];
	}
	if ((t&~TYPE)==FUNC)
		return(0);
	if (t>=PTR)
		elsz = SZPTR;
	else switch(t&TYPE) {

	case INT:
	case UNSIGN:
		elsz = SZINT;
		break;

	case CHAR:
	case UCHAR:
	case VOID:
		elsz = 1;
		break;

	case SHORT:
	case USHORT:
		elsz = SZSHORT;
		break;

	case FLOAT:
		elsz = SZFLOAT;
		break;

	case LONG:
	case ULONG:
		elsz = SZLONG;
		break;

	case DOUBLE:
		elsz = SZDOUB;
		break;

	case STRUCT:
		if ((elsz = strp->ssize) == 0)
			error("Undefined structure");
		break;
	default:
		error("Compiler error (length)");
		return(0);
	}
	n *= elsz;
	if (n > INT32_MAX/2) {
		error("Data structure too large");
		return(0);
	}
	return(n);
}

/*
 * The same for a symbol table entry.
 */
int
slength(struct hshtab *hp)
{
	struct tnode t;

	memset(&t, 0, sizeof(t));
	t.op = NAME;
	t.type = hp->htype;
	t.subsp = hp->hsubsp;
	t.strp = hp->hstrp;
	return(length(&t));
}

int
srlength(struct hshtab *hp)
{
	return((slength(hp)+ALIGN) & ~ALIGN);
}

/*
 * The number of bytes in an object, rounded up to a stack slot.
 */
int
rlength(struct tnode *cs)
{
	return((length(cs)+ALIGN) & ~ALIGN);
}

/*
 * Native natural alignment of an object type.
 */
int
talign(int t, struct str *strp)
{
	while ((t&XTYPE) == ARRAY)
		t = decref(t);
	if (t>=PTR)
		return((t&XTYPE)==FUNC ? 1 : SZPTR);
	switch (t) {
	case SHORT:
	case USHORT:
		return(SZSHORT);
	case INT:
	case UNSIGN:
	case FLOAT:
		return(SZINT);
	case LONG:
	case ULONG:
	case DOUBLE:
		return(SZLONG);
	case STRUCT:
		return(strp && strp->salign ? strp->salign : 1);
	}
	return(1);
}

/*
 * After an "if (...) goto", look to see if the transfer
 * is to a simple label.
 */
int
simplegoto(void)
{
	register struct hshtab *csp;

	if ((peeksym=symbol())==NAME && nextchar()==';') {
		csp = csym;
		if (csp->hblklev == 0)
			pushdecl(csp);
		if (csp->hclass==0 && csp->htype==0) {
			csp->htype = ARRAY;
			csp->hflag |= FLABL;
			if (csp->hoffset==0)
				csp->hoffset = isn++;
		}
		if ((csp->hclass==0||csp->hclass==STATIC)
		 &&  csp->htype==ARRAY) {
			peeksym = -1;
			return(csp->hoffset);
		}
	}
	return(0);
}

/*
 * Return the next non-white-space character
 */
int
nextchar(void)
{
	while (spnextchar()==' ')
		peekc = 0;
	return(peekc);
}

/*
 * Return the next character, translating all white space
 * to blank and handling line-ends.
 */
int
spnextchar(void)
{
	register int c;

	if ((c = peekc)==0)
		c = getchar();
	if (c=='\t' || c=='\014' || c=='\r')	/* FF */
		c = ' ';
	else if (c=='\n') {
		c = ' ';
		if (inhdr==0)
			line++;
		inhdr = 0;
	} else if (c=='\001') {	/* SOH, insert marker */
		inhdr++;
		c = ' ';
	} else if (c==EOF)
		eof = 1;
	peekc = c;
	return(c);
}

/*
 * is a break or continue legal?
 */
void
chconbrk(int l)
{
	if (l==0)
		error("Break/continue error");
}

/*
 * The goto statement.
 */
void
dogoto(void)
{
	register struct tnode *np;

	if ((np = tree()) == NULL)
		return;
	*cp++ = np;
	build(STAR);
	chkw(np = *--cp, -1);
	rcexpr(block(JUMP,0,NULL,NULL,np,NULL));
}

/*
 * The return statement, which has to convert
 * the returned object to the function's type.
 */
void
doret(void)
{
	register struct tnode *t;

	if (nextchar() != ';') {
		t = tree();
		if (t==NULL) {
			branch(retlab);
			return;
		}
		if (funcblk.type==VOID) {
			error("Value returned from void function");
			return;
		}
		*cp++ = &funcblk;
		*cp++ = t;
		build(ASSIGN);
		cp[-1] = cp[-1]->tr2;
		if (isnarrow(funcblk.type)) {
			cp[-1] = block(ITOC, INT, NULL, NULL, cp[-1], NULL);
			cp[-1]->value = funcblk.type;
		}
		build(RFORCE);
		rcexpr(*--cp);
	}
	branch(retlab);
}

/*
 * Native builtins and checks from the installed header profile.
 */
struct tnode *
builtin(int kind)
{
	register int o;
	struct tnode **scp, *ap, *bp, *ty;
	char *fb;
	LTYPE v;

	if (kind==SASSERT) {
		if ((o=symbol())!=LPARN)
			goto syntax;
		v = conexp();
		if ((o=symbol())!=COMMA || (o=symbol())!=STRING)
			goto syntax;
		while (mapch('"') >= 0)
			;
		if ((o=symbol())!=RPARN || (o=symbol())!=SEMI)
			goto syntax;
		if (v==0)
			error("Static assertion failed");
		return(NULL);
	}
	if ((o=symbol())!=LPARN)
		goto syntax;
	scp = cp;
	fb = funcbase;
	funcbase = curbase;
	ap = bp = ty = NULL;
	if (kind==OFFSETOF) {
		ty = xprtype();
		if ((o=symbol())!=COMMA)
			goto syntax;
		mosflg++;
		memqual = ty->strp;
		memdecl = 0;
		if ((o=symbol())!=NAME || csym->hclass!=MOS) {
			error("Illegal structure ref");
			goto syntax;
		}
		v = csym->hoffset;
		if ((o=symbol())!=RPARN)
			goto syntax;
		funcbase = fb;
		cp = scp;
		ap = cblock(v);
		ap->type = UNSIGN;
		return(ap);
	}
	initflg++;
	ap = tree();
	initflg--;
	if (ap==NULL)
		goto syntax;
	if (kind!=VAEND) {
		if ((o=symbol())!=COMMA)
			goto syntax;
		if (kind==VAARG)
			ty = xprtype();
		else {
			initflg++;
			bp = tree();
			initflg--;
			if (bp==NULL)
				goto syntax;
		}
	}
	if ((o=symbol())!=RPARN)
		goto syntax;
	funcbase = fb;
	cp = scp;
	ap = vaobject(ap);
	switch (kind) {
	case VASTART:
		if (!variadic)
			error("va_start requires variadic function");
		if (bp->op!=NAME || ((struct hshtab *)bp->tr1)->hclass!=AUTO && ((struct hshtab *)bp->tr1)->hclass!=REG)
			error("va_start requires last parameter");
		return(block(CP_VASTART, VOID, NULL, NULL, ap, NULL));
	case VAEND:
		return(block(CP_VAEND, VOID, NULL, NULL, ap, NULL));
	case VACOPY:
		return(block(CP_VACOPY, VOID, NULL, NULL, ap, vaobject(bp)));
	case VAARG:
		if (ty->type==VOID || (ty->type&XTYPE)==FUNC || (ty->type&XTYPE)==ARRAY || ty->type==STRUCT)
			error("Illegal va_arg type");
		return(block(CP_VAARG, ty->type, ty->subsp, ty->strp, ap, NULL));
	}
syntax:
	error("Builtin syntax");
	errflush(o);
	return(NULL);
}

/*
 * Coded output:
 *   B: beginning of line; an operator
 *   N: an int number
 *   L: a 64-bit number
 *   S: a symbol (external)
 *   F: a floating literal
 *   1: number 1
 *   0: number 0
 * Every item is a 64-bit little-endian word, except the
 * NUL-terminated strings.
 */
static void
putword(int64_t w, FILE *bufp)
{
	register int i;

	for (i=0; i<8; i++) {
		putc((int)(w&0377), bufp);
		w >>= 8;
	}
}

void
outcode(const char *s, ...)
{
	va_list ap;
	register FILE *bufp;
	register char *np;

	bufp = stdout;
	if (strflg)
		bufp = sbufp;
	va_start(ap, s);
	for (;;) switch(*s++) {
	case 'B':
		putword(CP_MARK | va_arg(ap, int), bufp);
		continue;

	case 'N':
		putword(va_arg(ap, int), bufp);
		continue;

	case 'L':
		putword(va_arg(ap, LTYPE), bufp);
		continue;

	case 'F':
	case 'S':
		np = va_arg(ap, char *);
		while (*np)
			putc(*np++&0377, bufp);
		putc(0, bufp);
		continue;

	case '1':
		putword(1, bufp);
		continue;

	case '0':
		putword(0, bufp);
		continue;

	case '\0':
		va_end(ap);
		if (ferror(bufp)) {
			error("Write error on temp");
			exit(1);
		}
		return;

	default:
		error("Botch in outcode");
	}
}
