/*
 *		C compiler part 2 -- expression optimizer
 *
 *	The PDP-11 specials (&~ via bic, right shifts as negative left
 *	shifts, int*size pointer conversion, big-endian long halves,
 *	floating-point bit tricks, autoincrement addressing and the long
 *	multiply/divide helpers) are replaced by their native forms.
 */

#include "c1.h"
#include <unistd.h>

/*
 * Fold integer constants with native 32-bit or 64-bit semantics.
 */
static LTYPE
foldv(int op, LTYPE a, LTYPE b, int t, int *ok)
{
	uint64_t ua, ub;
	int w, uns;

	*ok = 1;
	w = regwidth(t);
	uns = isunsigned(t);
	if (w==4) {
		ua = uns ? (uint32_t)a : (uint64_t)(int64_t)(int32_t)a;
		ub = uns ? (uint32_t)b : (uint64_t)(int64_t)(int32_t)b;
	} else {
		ua = a;
		ub = b;
	}
	switch (op) {
	case PLUS: ua += ub; break;
	case MINUS: ua -= ub; break;
	case TIMES: ua *= ub; break;
	case AND: ua &= ub; break;
	case OR: ua |= ub; break;
	case EXOR: ua ^= ub; break;
	case MAX: case MIN:
		ua = (op==MAX) == (w==4 ? (int64_t)(int32_t)ua > (int64_t)(int32_t)ub : (int64_t)ua > (int64_t)ub) ? ua : ub;
		break;
	case MAXP: case MINP:
		ua = (op==MAXP) == (ua > ub) ? ua : ub;
		break;
	case LSHIFT:
		if (ub >= (uint64_t)(w*8)) { *ok = 0; return(0); }
		ua <<= ub;
		break;
	case RSHIFT:
	case ULSH:
		if (ub >= (uint64_t)(w*8)) { *ok = 0; return(0); }
		if (uns || op==ULSH)
			ua = (w==4 ? (uint32_t)ua : ua) >> ub;
		else
			ua = (uint64_t)((int64_t)ua < 0 ? ~(~(int64_t)ua >> ub) : (int64_t)ua >> ub);
		break;
	case DIVIDE:
	case MOD:
	case PTOI:
		if (ub==0) {
			error("Divide check");
			*ok = 0;
			return(0);
		}
		if (uns) {
			if (w==4) { ua = (uint32_t)ua; ub = (uint32_t)ub; }
			ua = op==MOD ? ua % ub : ua / ub;
		} else if ((int64_t)ub == -1)
			ua = op==MOD ? 0 : -ua;
		else
			ua = (uint64_t)(op==MOD ? (int64_t)ua % (int64_t)ub : (int64_t)ua / (int64_t)ub);
		break;
	default:
		*ok = 0;
		return(0);
	}
	if (w==4)
		return(uns ? (LTYPE)(uint32_t)ua : (LTYPE)(int32_t)ua);
	return((LTYPE)ua);
}

struct tnode *
optim(struct tnode *atree)
{
	register int op, dope;
	int d1, d2, ok;
	struct tnode *t;
	register struct tnode *tree;

	if ((tree=atree)==0)
		return(0);
	if ((op = tree->op)==0)
		return(tree);
	if (op==NAME && tree->class==AUTO) {
		tree->class = OFFS;
		tree->regno = RFRAME;
		tree->offset = tree->nloc;
	}
	if (op >= 200)
		return(tree);
	dope = opdope[op];
	if ((dope&LEAF) != 0)
		return(tree);
	if ((dope&BINARY) == 0)
		return(unoptim(tree));
	/* is known to be binary */
	if (isnarrow(tree->type))
		tree->type = INT;
	switch(op) {

	/*
	 * Native: integer to pointer conversion scales the
	 * widened integer; longs are already pointer width.
	 */
	case ITOP:
		tree->tr1 = unoptim(tnode(ITOL, LONG, tree->tr1, NULL));
	case LTOP:
		tree->op = TIMES;
		tree->tr2->type = LONG;
		break;

	case MINUS:
		if ((t = isconstant(tree->tr2)) && !isfloat(tree)) {
			tree->op = PLUS;
			t->value = foldv(MINUS, 0, t->value, t->type, &ok);
		}
		break;
	}
	op = tree->op;
	dope = opdope[op];
	if (dope&LVALUE && tree->tr1->op==FSEL)
		return(lvfield(tree));
	if ((dope&COMMUTE)!=0) {
		d1 = tree->type;
		tree = acommute(tree);
		if (tree->op == op)
			tree->type = d1;
		return(tree);
	}
	tree->tr1 = optim(tree->tr1);
	tree->tr2 = optim(tree->tr2);
	if (tree->type==LONG || tree->type==ULONG || tree->type>=PTR) {
		t = lconst(tree->op, tree->tr1, tree->tr2);
		if (t)
			return(t);
	}
	if ((dope&RELAT) != 0) {
		if ((d1=degree(tree->tr1)) < (d2=degree(tree->tr2))
		 || d1==d2 && tree->tr1->op==NAME && tree->tr2->op!=NAME) {
			t = tree->tr1;
			tree->tr1 = tree->tr2;
			tree->tr2 = t;
			tree->op = maprel[op-EQUAL];
		}
	}
	d1 = max(degree(tree->tr1), islong(tree->type));
	d2 = max(degree(tree->tr2), 0);
	switch (op) {

	/*
	 * In assignment to fields, treat all-zero and all-1 specially.
	 */
	case FSELA:
		if (tree->tr2->op==CON && tree->tr2->value==0) {
			tree->op = ASAND;
			tree->tr2->value = ~tree->mask;
			return(optim(tree));
		}
		if (tree->tr2->op==CON && tree->mask==tree->tr2->value) {
			tree->op = ASOR;
			return(optim(tree));
		}
		tree->degree = 10;
		break;

	case CALL:
		tree->degree = 10;
		break;

	case QUEST:
	case COLON:
		tree->degree = max(d1, d2);
		break;

	case DIVIDE:
	case ASDIV:
	case ASTIMES:
	case PTOI:
		if (tree->tr2->op==CON && tree->tr2->value==1)
			return(tree->tr1);
	case MOD:
	case ASMOD:
		if (isunsigned(tree->tr1->type) && ispow2(tree))
			return(pow2(tree));
		if ((op==MOD||op==ASMOD) && isfloat(tree)) {
			error("Floating %% not defined");
			tree->type = INT;
		}
	case ULSH:
	case ASULSH:
		d1 += 2;
		d2 += 2;
		goto constant;

	case LSHIFT:
	case RSHIFT:
	case ASRSH:
	case ASLSH:
		if (tree->tr2->op==CON && tree->tr2->value==0) {
			return(tree->tr1);
		}
		goto constant;

	constant:
		if (tree->tr1->op==CON && tree->tr2->op==CON) {
			fconst(op, tree->tr1, tree->tr2->value, tree->type);
			return(tree->tr1);
		}


	default:
		if (dope&RELAT) {
			if (opdope[tree->tr1->op&0377]&RELAT && tree->tr2->op==CON
			 && tree->tr2->value==0) {
				tree = tree->tr1;
				switch(op) {
				case GREATEQ:
					return(&cone);
				case LESS:
					return(&czero);
				case LESSEQ:
				case EQUAL:
					tree->op = notrel[tree->op-EQUAL];
				}
				return(tree);
			}
		}
		tree->degree = d1==d2? d1+islong(tree->type): max(d1, d2);
		break;
	}
	return(tree);
}

struct tnode *
unoptim(struct tnode *atree)
{
	register struct tnode *subtre, *tree, *p;
	int t;

	if ((tree=atree)==0)
		return(0);
	if (tree->op==AMPER && tree->tr1->op==STAR) {
		subtre = tree->tr1->tr1;
		subtre->type = tree->type;
		return(optim(subtre));
	}
	subtre = tree->tr1 = optim(tree->tr1);
	switch (tree->op) {

	case ITOL:
		if (subtre->op==CON) {
			subtre->op = LCON;
			if (subtre->type==UNSIGN)
				subtre->value = (uint32_t)subtre->value;
			else
				subtre->value = (int32_t)subtre->value;
			subtre->type = tree->type;
			return(subtre);
		}
		break;

	case FTOI:
		if (tree->type==UNSIGN) {
			tree->op = FTOL;
			tree->type = LONG;
			tree = tnode(LTOI, UNSIGN, tree, NULL);
		}
		break;

	case LTOF:
		if (subtre->op==LCON) {
			tree = getblk(sizeof(*tree));
			memset(tree, 0, sizeof(*tree));
			tree->op = FCON;
			tree->type = DOUBLE;
			tree->value = isn++;
			tree->fvalue = subtre->type==ULONG ? (double)(uint64_t)subtre->value : (double)subtre->value;
			return(optim(tree));
		}
		break;

	case ITOF:
		if (tree->tr1->type==UNSIGN) {
			tree->tr1 = tnode(ITOL, LONG, tree->tr1, NULL);
			tree->op = LTOF;
			tree = optim(tree);
			break;
		}
		if (subtre->op==CON) {
			tree = getblk(sizeof(*tree));
			memset(tree, 0, sizeof(*tree));
			tree->op = FCON;
			tree->type = DOUBLE;
			tree->value = isn++;
			tree->fvalue = (int32_t)subtre->value;
			return(tree);
		}
		break;

	case ITOC:
		p = tree->tr1;
		/*
		 * Narrow to the cast's type, extending to an int.
		 */
		t = (int)tree->value;
		if (p->op==CON) {
			p->value = t==UCHAR ? (uint8_t)p->value : t==SHORT ? (int16_t)p->value :
			    t==USHORT ? (uint16_t)p->value : (int8_t)p->value;
			p->type = INT;
			return(p);
		} else if (p->op==NAME) {
			p->type = t;
			return(p);
		}
		break;

	case LTOI:
		p = tree->tr1;
		switch (p->op) {

		case LCON:
			p->op = CON;
			p->type = tree->type;
			p->value = tree->type==UNSIGN ? (LTYPE)(uint32_t)p->value : (LTYPE)(int32_t)p->value;
			return(p);

		case NAME:
			/* the low half is at the same little-endian address */
			if (p->class!=REG)
				p->type = tree->type;
			else
				break;
			return(p);

		case STAR:
			p->type = tree->type;
			p->tr1->type = tree->type+PTR;
			return(optim(p));

		case ITOL:
			if (regwidth(p->tr1->type)==4)
				return(p->tr1);
			break;

		case PLUS:
		case MINUS:
		case AND:
		case OR:
		case EXOR:
			p->tr2 = tnode(LTOI, tree->type, p->tr2, NULL);
		case NEG:
		case COMPL:
			p->tr1 = tnode(LTOI, tree->type, p->tr1, NULL);
			p->type = tree->type;
			return(optim(p));
		}
		break;

	case FSEL:
		tree->op = AND;
		tree->tr1 = tree->tr2->tr1;
		tree->tr2->tr1 = subtre;
		tree->tr2->op = RSHIFT;
		tree->tr2->type = UNSIGN;
		tree->tr1->value = (1L << tree->tr1->value) - 1;
		tree->tr1->type = UNSIGN;
		return(optim(tree));

	case FSELR:
		tree->op = LSHIFT;
		tree->type = UNSIGN;
		tree->tr1 = tree->tr2;
		tree->tr1->op = AND;
		tree->tr1->type = UNSIGN;
		tree->tr2 = tree->tr2->tr2;
		tree->tr1->tr2 = subtre;
		tree->tr1->tr1->value = (1L << tree->tr1->tr1->value) -1;
		tree->tr1->tr1->type = UNSIGN;
		return(optim(tree));

	case AMPER:
		if (subtre->op==STAR)
			return(subtre->tr1);
		if (subtre->op==NAME && subtre->class == OFFS) {
			p = tnode(PLUS, tree->type, subtre, tree);
			subtre->type = tree->type;
			tree->op = CON;
			tree->type = LONG;
			tree->degree = 0;
			tree->value = subtre->offset;
			subtre->class = REG;
			subtre->nloc = subtre->regno;
			subtre->offset = 0;
			return(optim(p));
		}
		break;

	case STAR:
		if (subtre->op==AMPER) {
			subtre->tr1->type = tree->type;
			return(subtre->tr1);
		}
		if (tree->type==STRUCT)
			break;
		if (subtre->op==NAME && subtre->class==REG) {
			subtre->type = tree->type;
			subtre->class = OFFS;
			subtre->regno = subtre->nloc;
			return(subtre);
		}
		p = subtre->tr1;
		if (subtre->op==PLUS && p->op==NAME && p->class==REG) {
			if (subtre->tr2->op==CON && subtre->tr2->value>=INT32_MIN && subtre->tr2->value<=INT32_MAX) {
				p->offset += subtre->tr2->value;
				p->class = OFFS;
				p->type = tree->type;
				p->regno = p->nloc;
				return(p);
			}
		}
		break;
	case EXCLA:
		if ((opdope[subtre->op&0377]&RELAT)==0)
			break;
		tree = subtre;
		tree->op = notrel[tree->op-EQUAL];
		break;

	case COMPL:
		if (isnarrow(tree->type))
			tree->type = INT;
		if (tree->op == subtre->op)
			return(subtre->tr1);
		if (subtre->op==CON || subtre->op==LCON) {
			subtre->value = regwidth(subtre->type)==4 ? (LTYPE)(isunsigned(subtre->type) ?
			    (uint32_t)~subtre->value : (int32_t)~subtre->value) : ~subtre->value;
			return(subtre);
		}
		break;

	case NEG:
		if (isnarrow(tree->type))
			tree->type = INT;
		if (tree->op==subtre->op)
			return(subtre->tr1);
		if (subtre->op==CON || subtre->op==LCON) {
			subtre->value = foldv(MINUS, 0, subtre->value, subtre->type, &t);
			return(subtre);
		}
		if (subtre->op==FCON) {
			subtre->fvalue = -subtre->fvalue;
			return(subtre);
		}
	}
	if ((opdope[tree->op&0377]&LEAF)==0)
		tree->degree = max(islong(tree->type), degree(subtre));
	return(tree);
}

/*
 * Deal with assignments to partial-word fields.
 * The game is that select(x) += y turns into
 * select(x += select(y)) where the shifts and masks
 * are chosen properly.  The outer select
 * is discarded where the value doesn't matter.
 * Sadly, overflow is undetected on += and the like.
 * Pure assignment is handled specially.
 */

struct tnode *
lvfield(struct tnode *at)
{
	register struct tnode *t, *t1;
	register struct tnode *t2;

	t = at;
	switch (t->op) {

	case ASSIGN:
		t2 = getblk(sizeof(*t2));
		memset(t2, 0, sizeof(*t2));
		t2->op = FSELA;
		t2->type = UNSIGN;
		t1 = t->tr1->tr2;
		t2->mask = ((1L<<t1->tr1->value)-1)<<t1->tr2->value;
		t2->tr1 = t->tr1;
		t2->tr2 = t->tr2;
		t = t2;

	case ASPLUS:
	case ASMINUS:
	case ASOR:
	case ASXOR:
	case INCBEF:
	case INCAFT:
	case DECBEF:
	case DECAFT:
		t1 = t->tr1;
		t1->op = FSELR;
		t->tr1 = t1->tr1;
		t1->tr1 = t->tr2;
		t->tr2 = t1;
		t1 = t1->tr2;
		t1 = tnode(COMMA, INT, tconst(t1->tr1->value, INT),
			tconst(t1->tr2->value, INT));
		return(optim(tnode(FSELT, UNSIGN, t, t1)));

	}
	error("Unimplemented field operator");
	return(t);
}

#define	LSTSIZ	20
struct acl {
	int nextl;
	int nextn;
	struct tnode *nlist[LSTSIZ];
	struct tnode *llist[LSTSIZ+1];
};

struct tnode *
acommute(struct tnode *atree)
{
	struct acl acl;
	int d, i, op, flt, d1;
	register struct tnode *t1, **t2, *tree;
	struct tnode *t;

	acl.nextl = 0;
	acl.nextn = 0;
	tree = atree;
	op = tree->op;
	flt = isfloat(tree);
	insert(op, tree, &acl);
	acl.nextl--;
	t2 = &acl.llist[acl.nextl];
	if (!flt) {
		/* put constants together */
		for (i=acl.nextl; i>0; i--) {
			if ((t2[0]->op==CON || t2[0]->op==LCON) && (t2[-1]->op==CON || t2[-1]->op==LCON)) {
				acl.nextl--;
				t2--;
				fconst(op, t2[0], t2[1]->value, tree->type);
				if (regwidth(tree->type)==8 && t2[0]->op==CON)
					t2[0]->op = LCON;
				t2[0]->type = tree->type;
			} else if ((t = lconst(op, t2[-1], t2[0]))) {
				acl.nextl--;
				t2--;
				t2[0] = t;
			}
		}
	}
	if (op==PLUS || op==OR) {
		/* toss out "+0" */
		if (acl.nextl>0 && ((t1 = isconstant(*t2)) && t1->value==0
		 || (*t2)->op==LCON && (*t2)->lvalue==0)) {
			acl.nextl--;
			t2--;
		}
		if (acl.nextl <= 0) {
			if (isnarrow((*t2)->type) && !isnarrow(tree->type))
				*t2 = tnode(LOAD, tree->type, *t2, NULL);
			(*t2)->type = tree->type;
			return(*t2);
		}
		/* subsume constant in "&x+c" */
		if (op==PLUS && t2[0]->op==CON && t2[-1]->op==AMPER
		 && t2[-1]->tr1->op==NAME && (t2[-1]->tr1->class==EXTERN || t2[-1]->tr1->class==STATIC)) {
			t2--;
			t2[0]->tr1->offset += t2[1]->value;
			acl.nextl--;
		}
	} else if (op==TIMES || op==AND) {
		t1 = acl.llist[acl.nextl];
		if (t1->op==CON) {
			if (t1->value==0)
				return(t1);
			if (op==TIMES && t1->value==1 && acl.nextl>0)
				if (--acl.nextl <= 0) {
					t1 = acl.llist[0];
					if (isunsigned(tree->type))
						t1->type = tree->type;
					return(t1);
				}
		}
	}
	if (op==PLUS && !flt)
		distrib(&acl);
	tree = *(t2 = &acl.llist[0]);
	d = max(degree(tree), islong(tree->type));
	if (op==TIMES && !flt)
		d++;
	for (i=0; i<acl.nextl; i++) {
		t1 = acl.nlist[i];
		t1->tr2 = t = *++t2;
		d1 = degree(t);
		t1->degree = d = d==d1? d+islong(t1->type): max(d, d1);
		t1->tr1 = tree;
		tree = t1;
	}
	if (tree->op==TIMES && ispow2(tree))
		tree->degree = max(degree(tree->tr1), islong(tree->type));
	return(tree);
}

void
distrib(void *alist)
{
/*
 * Find a list member of the form c1c2*x such
 * that c1c2 divides no other such constant, is divided by
 * at least one other (say in the form c1*y), and which has
 * fewest divisors. Reduce this pair to c1*(y+c2*x)
 * and iterate until no reductions occur.
 */
	struct acl *list = alist;
	register struct tnode **p1, **p2;
	struct tnode *t;
	int ndmaj, ndmin;
	struct tnode **dividend, **divisor;
	struct tnode **maxnod, **mindiv;

    loop:
	maxnod = &list->llist[list->nextl];
	ndmaj = 1000;
	dividend = 0;
	divisor = mindiv = 0;
	for (p1 = list->llist; p1 <= maxnod; p1++) {
		if ((*p1)->op!=TIMES || (*p1)->tr2->op!=CON || (*p1)->tr2->value==0)
			continue;
		ndmin = 0;
		for (p2 = list->llist; p2 <= maxnod; p2++) {
			if (p1==p2 || (*p2)->op!=TIMES || (*p2)->tr2->op!=CON || (*p2)->tr2->value==0)
				continue;
			if ((*p1)->tr2->value == (*p2)->tr2->value
			 && (*p1)->type==(*p2)->type && (*p1)->tr1->type==(*p2)->tr1->type) {
				(*p2)->tr2 = (*p1)->tr1;
				(*p2)->op = PLUS;
				(*p1)->tr1 = (*p2);
				*p1 = optim(*p1);
				squash(p2, maxnod);
				list->nextl--;
				goto loop;
			}
			if (((*p2)->tr2->value % (*p1)->tr2->value) == 0)
				goto contmaj;
			if (((*p1)->tr2->value % (*p2)->tr2->value) == 0
			 && (*p1)->type==(*p2)->type && (*p1)->tr1->type==(*p2)->tr1->type) {
				ndmin++;
				mindiv = p2;
			}
		}
		if (ndmin > 0 && ndmin < ndmaj) {
			ndmaj = ndmin;
			dividend = p1;
			divisor = mindiv;
		}
    contmaj:;
	}
	if (dividend==0 || list->nextn <= 0)
		return;
	t = list->nlist[--list->nextn];
	p1 = dividend;
	p2 = divisor;
	t->op = PLUS;
	t->type = (*p1)->type;
	t->tr1 = (*p1);
	t->tr2 = (*p2)->tr1;
	(*p1)->tr2->value /= (*p2)->tr2->value;
	(*p2)->tr1 = t;
	t = optim(*p2);
	if (p1 < p2) {
		*p1 = t;
		squash(p2, maxnod);
	} else {
		*p2 = t;
		squash(p1, maxnod);
	}
	list->nextl--;
	goto loop;
}

void
squash(struct tnode **p, struct tnode **maxp)
{
	register struct tnode **np;

	for (np = p; np < maxp; np++)
		*np = *(np+1);
}

/*
 * Fold "constant op v" into the constant node.
 */
void
fconst(int op, struct tnode *vp, LTYPE av, int type)
{
	int ok;
	LTYPE r;

	if (op==ANDN) {
		op = AND;
		av = ~av;
	}
	r = foldv(op, vp->value, av, isunsigned(type) || isunsigned(vp->type) ? (regwidth(type)==8 ? ULONG : UNSIGN) :
	    regwidth(type)==8 ? LONG : INT, &ok);
	if (!ok) {
		if (op!=DIVIDE && op!=MOD && op!=PTOI)
			error("C error: const");
		return;
	}
	vp->value = r;
}

struct tnode *
lconst(int op, struct tnode *lp, struct tnode *rp)
{
	LTYPE l, r;
	int ok, t;

	if (lp->op!=LCON || rp->op!=LCON)
		return(0);
	l = lp->lvalue;
	r = rp->lvalue;
	t = lp->type==ULONG || rp->type==ULONG || lp->type>=PTR ? ULONG : LONG;
	switch (op) {
	case PLUS: case MINUS: case TIMES: case DIVIDE: case MOD:
	case AND: case OR: case EXOR: case LSHIFT: case RSHIFT:
		l = foldv(op, l, r, t, &ok);
		if (!ok)
			return(0);
		break;
	default:
		return(0);
	}
	lp->lvalue = l;
	return(lp);
}

void
insert(int op, struct tnode *atree, void *alist)
{
	register int d;
	register struct acl *list;
	register struct tnode *tree;
	int d1, i;
	struct tnode *t;

	tree = atree;
	list = alist;
ins:
	if (tree->op != op)
		tree = optim(tree);
	if (tree->op == op && list->nextn < LSTSIZ-2) {
		list->nlist[list->nextn++] = tree;
		insert(op, tree->tr1, list);
		insert(op, tree->tr2, list);
		return;
	}
	if (!isfloat(tree)) {
		/* c1*(x+c2) -> c1*x+c1*c2 */
		if ((tree->op==TIMES||tree->op==LSHIFT)
		  && tree->tr2->op==CON && tree->tr2->value>0
		  && tree->tr1->op==PLUS && tree->tr1->tr2->op==CON
		  && tree->type==tree->tr1->type) {
			d = tree->tr2->value;
			if (tree->op==TIMES)
				tree->tr2->value *= tree->tr1->tr2->value;
			else
				tree->tr2->value = (LTYPE)((uint64_t)tree->tr1->tr2->value << (d & 63));
			tree->tr1->tr2->value = d;
			tree->tr1->op = tree->op;
			tree->op = PLUS;
			tree = optim(tree);
			if (op==PLUS)
				goto ins;
		}
	}
	d = degree(tree);
	for (i=0; i<list->nextl; i++) {
		if ((d1=degree(list->llist[i]))<d) {
			t = list->llist[i];
			list->llist[i] = tree;
			tree = t;
			d = d1;
		}
	}
	if (list->nextl >= LSTSIZ) {
		error("Expression too complicated");
		return;
	}
	list->llist[list->nextl++] = tree;
}

struct tnode *
tnode(int op, int type, struct tnode *tr1, struct tnode *tr2)
{
	register struct tnode *p;

	p = getblk(sizeof(*p));
	memset(p, 0, sizeof(*p));
	p->op = op;
	p->type = type;
	p->degree = 0;
	p->tr1 = tr1;
	if (op<200 && opdope[op]&BINARY || op==CP_VACOPY)
		p->tr2 = tr2;
	else
		p->tr2 = NULL;
	return(p);
}

struct tnode *
tconst(LTYPE val, int type)
{
	register struct tnode *p;

	p = getblk(sizeof(*p));
	memset(p, 0, sizeof(*p));
	p->op = CON;
	p->type = type;
	p->value = val;
	return(p);
}

void *
getblk(int size)
{
	register char *p;

	size = (size+7) & ~7;
	p = curbase;
	if (size > coremax-curbase) {
#ifdef NX_SBRK_ARENA
		int grow = (size - (coremax-curbase) + 0xffff) & ~0xffff;

		if (grow > 0 && sbrk(grow) == coremax)
			coremax += grow;
		else
#endif
		{
			error("Out of space-- c1");
			exit(1);
		}
	}
	curbase += size;
	return(p);
}

/*
 * Register cost of a value: every native scalar fits one register
 * (PDP-11 longs needed two).
 */
int
islong(int t)
{
	(void)t;
	return(1);
}

struct tnode *
isconstant(struct tnode *at)
{
	register struct tnode *t;

	t = at;
	if (t->op==CON || t->op==SFCON || t->op==LCON)
		return(t);
	if (t->op==ITOL && t->tr1->op==CON)
		return(t->tr1);
	return(0);
}

struct tnode *
hardlongs(struct tnode *at)
{
	return(at);
}
