/*
 * C compiler
 *
 *
 */

#include "c0.h"
#include <stdarg.h>
#include <unistd.h>

/*
 * Called from tree, this routine takes the top 1, 2, or 3
 * operands on the expression stack, makes a new node with
 * the operator op, and puts it on the stack.
 * Essentially all the work is in inserting
 * appropriate conversions.
 */
void
build(int op)
{
	register int t1;
	int t2, t;
	register struct tnode *p1, *p2;
	struct tnode *p3;
	int dope, leftc, cvn, pcvn;

	/*
	 * a[i] => *(a+i)
	 */
	if (op==LBRACK) {
		build(PLUS);
		op = STAR;
	}
	dope = opdope[op];
	t2 = 0;
	p2 = NULL;
	/* a malformed expression must not pop below the operand stack */
	if (cp - cmstack < ((dope&BINARY) ? 2 : 1)) {
		error("Expression syntax");
		cp = cmstack;
		*cp++ = cblock(0);
		return;
	}
	if ((dope&BINARY)!=0) {
		/* an empty call argument list is a null operand */
		if ((p2 = *--cp) != NULL) {
			p2 = chkfun(disarray(p2));
			t2 = p2->type;
		}
	}
	p1 = *--cp;
	/*
	 * sizeof gets turned into a number here.
	 */
	if (op==SIZEOF) {
		p3 = cblock(length(p1));
		p3->type = UNSIGN;
		*cp++ = p3;
		return;
	}
	if (op!=AMPER) {
		p1 = disarray(p1);
		if (op!=CALL)
			p1 = chkfun(p1);
	}
	t1 = p1->type;
	pcvn = 0;
	t = INT;
	switch (op) {

	case CAST:
		if ((t1&XTYPE)==FUNC || (t1&XTYPE)==ARRAY)
			error("Disallowed conversion");
		if (t1==VOID) {
			/* (void)expr only discards the value */
			*cp++ = block(SEQNC, VOID, NULL, NULL, p2, cblock(0));
			return;
		}
		if (t1==UNSIGN && t2==CHAR) {
			t2 = INT;
			p2 = block(AND,INT,NULL,NULL,p2,cblock(0377));
		}
		break;

	/* end of expression */
	case 0:
		*cp++ = p1;
		return;

	/* no-conversion operators */
	case QUEST:
		if (p2->op!=COLON)
			error("Illegal conditional");
		else
			if (fold(QUEST, p1, p2))
				return;

	case SEQNC:
		t = t2;

	case COMMA:
	case LOGAND:
	case LOGOR:
		if ((op==LOGAND || op==LOGOR) && p1->op==CON && p2->op==CON) {
			p1->value = op==LOGAND ? p1->value && p2->value : p1->value || p2->value;
			p1->type = INT;
			*cp++ = p1;
			return;
		}
		*cp++ = block(op, t, NULL, NULL, p1, p2);
		return;

	case EXCLA:
		t1 = INT;
		break;

	case CALL:
		if ((t1&XTYPE) != FUNC)
			error("Call of non-function");
		if (p1->op==NAME)
			p2 = protocall((struct hshtab *)p1->tr1, p2);
		p3 = block(CALL,decref(t1),p1->subsp,p1->strp,p1,p2);
		if (p3->type==STRUCT) {
			p3->value = 1;	/* structure result */
			p3->rstrp = p3->strp;
		}
		*cp++ = p3;
		return;

	case STAR:
		if ((t1&XTYPE) == FUNC)
			error("Illegal indirection");
		*cp++ = block(STAR, decref(t1), p1->subsp, p1->strp, p1, NULL);
		return;

	case AMPER:
		if (p1->op==NAME || p1->op==STAR) {
			*cp++ = block(op,incref(t1),p1->subsp,p1->strp,p1,NULL);
			return;
		}
		error("Illegal lvalue");
		break;

	/*
	 * a.b goes to (&a)->b
	 */
	case DOT:
		if (p1->op==CALL && t1==STRUCT) {
			t1 = incref(t1);
			setype(p1, t1, p1);
		} else {
			*cp++ = p1;
			build(AMPER);
			p1 = *--cp;
		}

	/*
	 * In a->b, a is given the type ptr-to-structure element;
	 * then the offset is added in without conversion;
	 * then * is tacked on to access the member.
	 */
	case ARROW:
		if (p2->op!=NAME || ((struct hshtab *)p2->tr1)->hclass!=MOS) {
			error("Illegal structure ref");
			*cp++ = p1;
			return;
		}
		if (t2==INT && ((struct hshtab *)p2->tr1)->hflag&FFIELD)
			t2 = UNSIGN;
		t = incref(t2);
		chkw(p1, -1);
		setype(p1, t, p2);
		*cp++ = block(PLUS,t,p2->subsp,p2->strp,p1,cblock(((struct hshtab *)p2->tr1)->hoffset));
		cp[-1]->tr2->type = LONG;
		build(STAR);
		if (((struct hshtab *)p2->tr1)->hflag&FFIELD) {
			p3 = *--cp;
			*cp++ = block(FSEL,UNSIGN,NULL,NULL,p3,(struct tnode *)((struct hshtab *)p2->tr1)->hstrp);
		}
		return;
	}
	if ((dope&LVALUE)!=0)
		chklval(p1);
	if ((dope&LWORD)!=0)
		chkw(p1, LONG);
	if ((dope&RWORD)!=0)
		chkw(p2, LONG);
	if ((dope&BINARY)==0) {
		if (op==ITOF)
			t1 = DOUBLE;
		else if (op==FTOI)
			t1 = INT;
		else if (op==NEG || op==COMPL) {
			/* the integer promotions */
			if (isinttype(t1) && t1!=UNSIGN)
				t1 = INT;
		}
		if (!fold(op, p1, 0))
			*cp++ = block(op,t1,p1->subsp,p1->strp,p1,NULL);
		return;
	}
	cvn = 0;
	if (t1==STRUCT || t2==STRUCT) {
		if (t1!=t2 || p1->strp != p2->strp)
			error("Incompatible structures");
		cvn = 0;
	} else
		cvn = cvtab[lintyp(t1)][lintyp(t2)];
	leftc = (cvn>>4)&017;
	cvn &= 017;
	t = leftc? t2:t1;
	if (isinttype(t) && t!=UNSIGN && (t1==UNSIGN||t2==UNSIGN))
		t = UNSIGN;
	if (t==LONG && (t1==ULONG||t2==ULONG))
		t = ULONG;
	if (dope&ASSGOP || op==CAST) {
		t = t1;
		if (op==ASSIGN || op==CAST) {
			/*
			 * Native widths: pointers are 64 bits, so an integer
			 * stored to a pointer is extended and a pointer stored
			 * to an integer truncated.  Long and pointer agree.
			 */
			if (cvn==PTI)
				cvn = leftc = 0;
			else if (cvn==ITP) {
				cvn = t1>=PTR ? ITL : LTI;
				leftc = 0;
			} else if (cvn==LTP)
				cvn = leftc = 0;
		}
		if (leftc)
			cvn = leftc;
		leftc = 0;
	} else if (op==COLON || op==MAX || op==MIN) {
		if (t1>=PTR && t2>=PTR)
			cvn = 0;
		else if (cvn==ITP)
			cvn = ITL;
		if (op!=COLON && (t1>=PTR || t2>=PTR))
			op += MAXP-MAX;
	} else if (dope&RELAT) {
		if (op>=LESSEQ && (t1>=PTR||t2>=PTR||t1==ULONG||t2==ULONG||(t1==UNSIGN||t2==UNSIGN)
		 && isinttype(t)))
			op += LESSEQP-LESSEQ;
		if (cvn==PTI)
			cvn = 0;
		else if (cvn==ITP)
			cvn = ITL;
		else if (cvn==LTP)
			cvn = leftc = 0;
	}
	if (cvn==PTI) {
		cvn = 0;
		if (op==MINUS) {
			t = LONG;
			pcvn++;
		} else {
			if (t1!=t2 || t1!=(PTR+CHAR))
				cvn = XX;
		}
	}
	if (cvn) {
		t1 = plength(p1);
		t2 = plength(p2);
		if (cvn==XX || (cvn==PTI&&t1!=t2))
			error("Illegal conversion");
		else if (leftc)
			p1 = convert(p1, t, cvn, t2);
		else
			p2 = convert(p2, t, cvn, t1);
	}
	if (dope&RELAT)
		t = INT;
	if (t==FLOAT)
		t = DOUBLE;
	if (isnarrow(t))
		t = INT;
	if (op==CAST) {
		if (t!=DOUBLE && (t!=INT || !isnarrow(p2->type))) {
			p2->type = t;
			p2->subsp = p1->subsp;
			p2->strp = p1->strp;
		}
		if (isnarrow(p1->type)) {
			/* narrowing casts keep the converted value in an int */
			p2 = block(ITOC, INT, NULL, NULL, p2, NULL);
			p2->value = p1->type;
		}
		*cp++ = p2;
		return;
	}
	if (fold(op, p1, p2)==0) {
		p3 = leftc?p2:p1;
		*cp++ = block(op, t, p3->subsp, p3->strp, p1, p2);
	}
	if (pcvn && t1!=(PTR+CHAR)) {
		p1 = *--cp;
		*cp++ = convert(p1, LONG, PTI, plength(p1->tr1));
	}
}

/*
 * Generate the appropriate conversion operator.
 */
struct tnode *
convert(struct tnode *p, int t, int cvn, int len)
{
	register int op;
	struct tnode *q;

	op = cvntab[cvn];
	if (opdope[op]&BINARY) {
		if (len==0)
			error("Illegal conversion");
		q = cblock(len);
		q->type = LONG;
		return(block(op, t, NULL, NULL, p, q));
	}
	return(block(op, t, NULL, NULL, p, NULL));
}

/*
 * Traverse an expression tree, adjust things
 * so the types of things in it are consistent
 * with the view that its top node has
 * type at.
 * Used with structure references.
 */
void
setype(struct tnode *ap, int at, struct tnode *anewp)
{
	register struct tnode *p, *newp;
	register int t;

	p = ap;
	t = at;
	newp = anewp;
	for (;; p = p->tr1) {
		p->subsp = newp->subsp;
		p->strp = newp->strp;
		p->type = t;
		if (p->op==AMPER)
			t = decref(t);
		else if (p->op==STAR)
			t = incref(t);
		else if (p->op!=PLUS)
			break;
	}
}

/*
 * A mention of a function name is turned into
 * a pointer to that function.
 */
struct tnode *
chkfun(struct tnode *ap)
{
	register struct tnode *p;
	register int t;

	p = ap;
	if (((t = p->type)&XTYPE)==FUNC && p->op!=ETYPE)
		return(block(AMPER,incref(t),p->subsp,p->strp,p,NULL));
	return(p);
}

/*
 * A mention of an array is turned into
 * a pointer to the base of the array.
 */
struct tnode *
disarray(struct tnode *ap)
{
	register int t;
	register struct tnode *p;

	p = ap;
	/* check array & not MOS and not typer */
	if (((t = p->type)&XTYPE)!=ARRAY || p->op==NAME&&((struct hshtab *)p->tr1)->hclass==MOS
	 || p->op==ETYPE)
		return(p);
	p->subsp++;
	*cp++ = p;
	setype(p, decref(t), p);
	build(AMPER);
	return(*--cp);
}

/*
 * make sure that p is a ptr to a node
 * with an integer or pointer type or 'okt.'
 * okt might be nonexistent or 'long'
 * (e.g. for <<).
 */
void
chkw(struct tnode *p, int okt)
{
	register int t;

	if (p==NULL)
		return;
	if (!isinttype(t=p->type) && t<PTR && t!=okt && !(okt==LONG && t==ULONG))
		error("Illegal type of operand");
}

/*
 *'linearize' a type for looking up in the
 * conversion table
 */
int
lintyp(int t)
{
	if (isinttype(t))
		return(0);
	switch(t) {

	case FLOAT:
	case DOUBLE:
		return(1);

	case LONG:
	case ULONG:
		return(2);

	default:
		if (t>=PTR)
			return(3);
		return(4);
	}
}

/*
 * Report an error.
 */
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
	fprintf(stderr, "\n");
}

/*
 * Generate a node in an expression tree,
 * setting the operator, type, dimen/struct table ptrs,
 * and the operands.
 */
struct tnode *
block(int op, int t, int *subs, struct str *str, struct tnode *p1, struct tnode *p2)
{
	register struct tnode *p;

	p = gblock(sizeof(*p));
	p->op = op;
	p->type = t;
	p->subsp = subs;
	p->strp = str;
	p->tr1 = p1;
	if (op<=RFORCE && opdope[op]&BINARY || op==CP_VACOPY)
		p->tr2 = p2;
	else
		p->tr2 = NULL;
	p->value = 0;
	p->cstr = NULL;
	p->rstrp = NULL;
	return(p);
}

struct tnode *
nblock(struct hshtab *ads)
{
	register struct hshtab *ds;

	ds = ads;
	return(block(NAME, ds->htype, ds->hsubsp, ds->hstrp, (struct tnode *)ds, NULL));
}

/*
 * Generate a block for a constant
 */
struct tnode *
cblock(LTYPE v)
{
	register struct tnode *p;

	p = gblock(sizeof(*p));
	p->op = CON;
	p->type = INT;
	p->subsp = NULL;
	p->strp = NULL;
	p->tr1 = p->tr2 = NULL;
	p->value = v;
	p->cstr = NULL;
	return(p);
}

/*
 * A block for a float or long constant
 */
struct tnode *
fblock(int t, char *string)
{
	register struct tnode *p;

	p = gblock(sizeof(*p));
	p->op = FCON;
	p->type = t;
	p->subsp = NULL;
	p->strp = NULL;
	p->tr1 = p->tr2 = NULL;
	p->value = 0;
	p->cstr = string;
	return(p);
}

/*
 * Assign a block for use in the
 * expression tree.
 */
void *
gblock(int n)
{
	register char *p;

	n = (n+7) & ~7;
	p = curbase;
	if (n > coremax-curbase) {
#ifdef NX_SBRK_ARENA
		int grow = (n - (coremax-curbase) + 0xffff) & ~0xffff;

		if (grow > 0 && sbrk(grow) == coremax)
			coremax += grow;
		else
#endif
		{
			error("Out of space");
			exit(1);
		}
	}
	curbase += n;
	return(p);
}

/*
 * Check that a tree can be used as an lvalue.
 */
void
chklval(struct tnode *ap)
{
	register struct tnode *p;

	p = ap;
	if (p->op==FSEL)
		p = p->tr1;
	if (p->op!=NAME && p->op!=STAR)
		error("Lvalue required");
}

/*
 * reduce some forms of `constant op constant'
 * to a constant.  More of this is done in the next pass
 * but this is used to allow constant expressions
 * to be used in switches and array bounds.
 * Native int arithmetic is 32-bit two's complement.
 */
int
fold(int op, struct tnode *ap1, struct tnode *ap2)
{
	register struct tnode *p1;
	int32_t v1, v2;
	uint32_t u1, u2;
	int unsignf;

	p1 = ap1;
	if (p1->op!=CON)
		return(0);
	unsignf = p1->type==UNSIGN;
	if (op==QUEST) {
		if (ap2->tr1->op==CON && ap2->tr2->op==CON) {
			p1->value = p1->value? ap2->tr1->value: ap2->tr2->value;
			p1->type = ap2->tr1->type==UNSIGN || ap2->tr2->type==UNSIGN ? UNSIGN : INT;
			*cp++ = p1;
			return(1);
		}
		return(0);
	}
	v2 = 0;
	if (ap2) {
		if (ap2->op!=CON)
			return(0);
		v2 = (int32_t)ap2->value;
		unsignf |= ap2->type==UNSIGN;
	}
	v1 = (int32_t)p1->value;
	u1 = (uint32_t)v1;
	u2 = (uint32_t)v2;
	switch (op) {

	case PLUS:
		u1 += u2;
		break;

	case MINUS:
		u1 -= u2;
		break;

	case TIMES:
		u1 *= u2;
		break;

	case DIVIDE:
		if (v2==0)
			goto divchk;
		if (unsignf)
			u1 /= u2;
		else if (v1==INT32_MIN && v2==-1)
			u1 = (uint32_t)INT32_MIN;
		else
			u1 = (uint32_t)(v1/v2);
		break;

	case MOD:
		if (v2==0)
			goto divchk;
		if (unsignf)
			u1 %= u2;
		else if (v2==-1)
			u1 = 0;
		else
			u1 = (uint32_t)(v1%v2);
		break;

	case AND:
		u1 &= u2;
		break;

	case OR:
		u1 |= u2;
		break;

	case EXOR:
		u1 ^= u2;
		break;

	case NEG:
		u1 = -u1;
		break;

	case COMPL:
		u1 = ~u1;
		break;

	case EXCLA:
		u1 = !u1;
		unsignf = 0;
		break;

	case LSHIFT:
		if (u2 >= 32)
			return(0);
		u1 <<= u2;
		break;

	case RSHIFT:
		if (u2 >= 32)
			return(0);
		if (unsignf)
			u1 >>= u2;
		else
			u1 = (uint32_t)(v1 < 0 ? ~(~v1 >> u2) : v1 >> u2);
		break;

	case EQUAL:
		u1 = v1==v2;
		unsignf = 0;
		break;

	case NEQUAL:
		u1 = v1!=v2;
		unsignf = 0;
		break;

	case LESS:
		u1 = unsignf ? u1<u2 : v1<v2;
		unsignf = 0;
		break;

	case GREAT:
		u1 = unsignf ? u1>u2 : v1>v2;
		unsignf = 0;
		break;

	case LESSEQ:
		u1 = unsignf ? u1<=u2 : v1<=v2;
		unsignf = 0;
		break;

	case GREATEQ:
		u1 = unsignf ? u1>=u2 : v1>=v2;
		unsignf = 0;
		break;

	divchk:
		error("Divide check");
	default:
		return(0);
	}
	p1->type = unsignf ? UNSIGN : INT;
	p1->value = unsignf ? (LTYPE)u1 : (LTYPE)(int32_t)u1;
	*cp++ = p1;
	return(1);
}

/*
 * Compile an expression expected to have constant value,
 * for example an array bound or a case value.
 */
LTYPE
conexp(void)
{
	register struct tnode *t;

	initflg++;
	if ((t = tree()))
		if (t->op != CON)
			error("Constant required");
	initflg--;
	curbase = funcbase;
	return(t ? t->value : 0);
}

int
isinttype(int t)
{
	return(t==INT || t==CHAR || t==UNSIGN || t==UCHAR || t==SHORT || t==USHORT);
}

int
isnarrow(int t)
{
	return(t==CHAR || t==UCHAR || t==SHORT || t==USHORT);
}

int
isunsigned(int t)
{
	return(t==UNSIGN || t==UCHAR || t==USHORT || t==ULONG || t>=PTR);
}

int
islongtype(int t)
{
	return(t==LONG || t==ULONG);
}

int
isfloattype(int t)
{
	return(t==FLOAT || t==DOUBLE);
}
