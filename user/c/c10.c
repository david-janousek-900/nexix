/*

	    	C compiler, part 2

	Native AMD64 retargeting of the supplied V7 c1: the
	table-driven Sethi-Ullman scheme is retained; register
	numbers name native scratch registers and the code table
	(table.s, converted by cvopt) holds AMD64 templates.

*/

#include "c1.h"
#include <stdarg.h>
#include <unistd.h>

#define	dbprint(op)	/* */

char	maprel[] = {	EQUAL, NEQUAL, GREATEQ, GREAT, LESSEQ,
			LESS, GREATQP, GREATP, LESSEQP, LESSP
};

char	notrel[] = {	NEQUAL, EQUAL, GREAT, GREATEQ, LESS,
			LESSEQ, GREATP, GREATQP, LESSP, LESSEQP
};

struct tnode czero = { CON, INT, 0 };
struct tnode cone  = { CON, INT, 0 };

struct tnode sfuncr = { NAME, STRUCT };

struct	table	*cregtab;
struct	table	lsptab[1];

int	nreg	= NSCRATCH;
int	isn	= 10000;

/*
 * Register names by width: 64, 32, 16 and 8 bits.
 */
char	*regname[4][15] = {
	{"%rax","%rdx","%rcx","%rsi","%rdi","%r8","%r9","%r10","%rbx","%r12","%r13","%r14","%r15","%rbp","%r11"},
	{"%eax","%edx","%ecx","%esi","%edi","%r8d","%r9d","%r10d","%ebx","%r12d","%r13d","%r14d","%r15d","%ebp","%r11d"},
	{"%ax","%dx","%cx","%si","%di","%r8w","%r9w","%r10w","%bx","%r12w","%r13w","%r14w","%r15w","%bp","%r11w"},
	{"%al","%dl","%cl","%sil","%dil","%r8b","%r9b","%r10b","%bl","%r12b","%r13b","%r14b","%r15b","%bpl","%r11b"},
};

static void emitfcon(struct tnode *);
#ifdef NX_SBRK_ARENA
static char ibuf[BUFSIZ], obuf[BUFSIZ];
#endif

int
main(int argc, char *argv[])
{
	cone.value = 1;
	if (argc<4) {
		error("Arg count");
		exit(1);
	}
	if (freopen(argv[1], "r", stdin)==NULL) {
		error("Missing temp file");
		exit(1);
	}
	if ((freopen(argv[3], "w", stdout)) == NULL) {
		error("Can't create %s", argv[3]);
		exit(1);
	}
#ifdef NX_SBRK_ARENA
	/* The node arena grows with sbrk, as in V7; stdio uses static buffers. */
	setbuf(stdin, ibuf);
	setbuf(stdout, obuf);
	setbuf(stderr, NULL);
	coremax = funcbase = sbrk(0);
	if (coremax == (char *)-1) {
		error("Out of space-- c1");
		exit(1);
	}
#else
	if ((funcbase = malloc(1024*1024)) == NULL) {
		error("Out of space-- c1");
		exit(1);
	}
	coremax = funcbase + 1024*1024;
#endif
	curbase = funcbase;
	cregtab = regtab;
	lsptab[0] = sptab[0];
	printf("\t.text\n");
	getree();
	/*
	 * tack on the string file.
	 */
	printf("\t.data\n");
	if (*argv[2] != '-') {
		if (freopen(argv[2], "r", stdin)==NULL) {
			error("Missing temp file");
			exit(1);
		}
#ifdef NX_SBRK_ARENA
		setbuf(stdin, ibuf);
#endif
		getree();
	}
	printf("\t.section\t.note.GNU-stack,\"\",@progbits\n");
	if (fflush(stdout) || ferror(stdout)) {
		error("Write error on %s", argv[3]);
		exit(1);
	}
	exit(nerror!=0);
}

/*
 * Given a tree, a code table, and a
 * count of available registers, find the code table
 * for the appropriate operator such that the operands
 * are of the right type and the number of registers
 * required is not too large.
 * Return a ptr to the table entry or 0 if none found.
 * (The PDP-11 conversion-eliding retry is not used: native operand
 * widths must match the instruction.)
 */
struct optab *
match(struct tnode *atree, struct table *table, int nrleft, int nocvt)
{
	int op, d1, d2, dope;
	struct tnode *p2;
	register struct tnode *p1, *tree;
	register struct optab *opt;

	(void)nocvt;
	if ((tree=atree)==0)
		return(0);
	if (table==lsptab)
		table = sptab;
	if ((op = tree->op)==0)
		return(0);
	if (op >= 256)
		return(0);
	dope = opdope[op];
	if ((dope&LEAF) == 0)
		p1 = tree->tr1;
	else
		p1 = tree;
	d1 = dcalc(p1, nrleft);
	p2 = 0;
	d2 = 0;
	if ((dope&BINARY)!=0) {
		p2 = tree->tr2;
		d2 = dcalc(p2, nrleft);
	}
	for (; table->tabop!=op; table++)
		if (table->tabop==0)
			return(0);
	for (opt = table->tabp; opt->tabdeg1!=0; opt++) {
		if (d1 > (opt->tabdeg1&077)
		 || (opt->tabdeg1 >= 0100 && (p1->op != STAR)))
			continue;
		if (notcompat(p1, opt->tabtyp1, op)) {
			continue;
		}
		if ((opdope[op]&BINARY)!=0 && p2!=0) {
			if (d2 > (opt->tabdeg2&077)
			 || (opt->tabdeg2 >= 0100) && (p2->op != STAR) )
				continue;
			if (notcompat(p2,opt->tabtyp2, 0))
				continue;
		}
		return(opt);
	}
	return(0);
}

/*
 * Given a tree, a code table, and a register,
 * produce code to evaluate the tree with the appropriate table.
 * Registers reg and upcan be used.
 * If there is a value, it is desired that it appear in reg.
 * The routine returns the register in which the value actually appears.
 * This routine must work or there is an error.
 * If the table called for is cctab, sptab, or efftab,
 * and tree can't be done using the called-for table,
 * another try is made.
 * If the tree can't be compiled using cctab, regtab is
 * used and a "test" instruction is produced.
 * If the tree can't be compiled using sptab,
 * regtab is used and the register is pushed on the stack.
 * If the tree can't be compiled using efftab,
 * just use regtab.
 * Regtab must succeed or an "op not found" error results.
 *
 * A number of special cases are recognized, and
 * there is an interaction with the optimizer routines.
 */
int
rcexpr(struct tnode *atree, struct table *atable, int reg)
{
	register int r;
	int modf, recurf;
	register struct tnode *tree;
	register struct table *table;

	table = atable;
	recurf = 0;
	if (reg<0) {
		recurf++;
		reg = ~reg;
		if (reg>=020) {
			reg -= 020;
			recurf++;
		}
	}
again:
	if((tree=atree)==0)
		return(0);
	if (opdope[tree->op&0377]&RELAT && tree->tr2->op==CON && tree->tr2->value==0
	 && table==cctab && !isfloat(tree->tr1))
		tree = atree = tree->tr1;
	/*
	 * fieldselect(...) : in efftab mode,
	 * ignore the select, otherwise
	 * do the shift and mask.
	 */
	if (tree->op == FSELT) {
		if (table==efftab)
			atree = tree = tree->tr1;
		else {
			tree->op = FSEL;
			atree = tree = optim(tree);
		}
	}
	switch (tree->op)  {

	/*
	 * Structure assignments
	 */
	case STRASG:
		strasg(tree);
		return(0);

	/*
	 * An initializing expression
	 */
	case INIT:
		tree = optim(tree);
		doinit(tree->type, tree->tr1);
		return(0);

	/*
	 * Put the value of an expression in the return register,
	 * for a switch or a return
	 */
	case RFORCE:
		modf = tree->type;
		tree = tree->tr1;
		if((r=rcexpr(tree, regtab, reg)) != 0)
			movreg(r, 0, tree);
		/* a float result is returned in single precision */
		if (modf==FLOAT)
			printf("\tcvtsd2ss\t%%xmm0,%%xmm0\n");
		return(0);

	/*
	 * sequential execution
	 */
	case SEQNC:
		r = nstack;
		rcexpr(tree->tr1, efftab, reg);
		nstack = r;
		atree = tree = tree->tr2;
		goto again;

	/*
	 * Handle a subroutine call. It has to be done
	 * here because if cexpr got called twice, the
	 * arguments might be compiled twice.
	 */
	case CALL:
		callexpr(tree, reg);
		if (table==efftab)
			return(reg);
		r = reg;
		if (table==regtab)
			return(r);
		goto fixup;

	case CP_SINGLE:
		r = rcexpr(tree->tr1, regtab, reg);
		printf("\tcvtsd2ss\t%%xmm%d,%%xmm%d\n", r, r);
		return(r);

	case CP_VASTART:
	case CP_VAARG:
	case CP_VAEND:
	case CP_VACOPY:
		native('v', tree, tree->tr1, tree->tr2, reg, reg+1);
		if (table==efftab || table==regtab)
			return(reg);
		r = reg;
		goto fixup;

	/*
	 * Try to change * to shift.
	 */
	case TIMES:
	case ASTIMES:
		tree = pow2(tree);
	}
	/*
	 * Try to find postfix ++ and -- operators that can be
	 * pulled out and done after the rest of the expression
	 */
	if (table!=cctab && table!=cregtab && recurf<2
	 && (opdope[tree->op&0377]&LEAF)==0) {
		if ((r=delay(&atree, table, reg))) {
			tree = atree;
			table = efftab;
			reg = r-1;
		}
	}
	/*
	 * Basically, try to reorder the computation
	 * so  reg = x+y  is done as  reg = x; reg += y
	 */
	if (recurf==0 && reorder(&atree, table, reg)) {
		if (table==cctab && atree->op==NAME)
			return(reg);
	}
	tree = atree;
	if (table==efftab && tree->op==NAME)
		return(reg);
	if ((r=cexpr(tree, table, reg))>=0)
		return(r);
	if (table!=regtab && (table!=cctab||(opdope[tree->op&0377]&RELAT)==0)) {
		if((r=cexpr(tree, regtab, reg))>=0) {
	fixup:
			modf = isfvalue(tree);
			dbprint(tree->op);
			if (table==sptab || table==lsptab) {
				if (modf)
					printf("\tsubq\t$8,%%rsp\n\tmovsd\t%%xmm%d,(%%rsp)\n", r);
				else
					printf("\tpushq\t%s\n", regname[0][r]);
				nstack++;
			}
			if (table==cctab) {
				if (modf)
					printf("\txorpd\t%%xmm15,%%xmm15\n\tucomisd\t%%xmm15,%%xmm%d\n", r);
				else {
					int w = regwidth(tree->type)==8 ? 0 : 1;
					printf("\ttest%c\t%s,%s\n", w ? 'l' : 'q', regname[w][r], regname[w][r]);
				}
			}
			return(r);
		}
	}
	if (tree->type == STRUCT)
		error("Illegal operation on structure");
	else if (tree->op>0 && tree->op<RFORCE && opntab[tree->op])
		error("No code table for op: %s", opntab[tree->op]);
	else
		error("No code table for op %d", tree->op);
	return(reg);
}

/*
 * Try to compile the tree with the code table using
 * registers areg and up.  If successful,
 * return the register where the value actually ended up.
 * If unsuccessful, return -1.
 *
 * Most of the work is the macro-expansion of the
 * code table.
 */
int
cexpr(struct tnode *atree, struct table *table, int areg)
{
	int c, r;
	register struct tnode *p, *p1, *tree;
	struct table *ctable;
	struct tnode *p2;
	char *string;
	int reg, reg1, rreg, flag, opd, atstart;
	struct optab *opt;

	tree = atree;
	reg = areg;
	p1 = tree->tr2;
	c = tree->op;
	opd = opdope[c&0377];
	/*
	 * When the value of a relational or a logical expression is
	 * desired, more work must be done.
	 */
	if ((opd&RELAT||c==LOGAND||c==LOGOR||c==EXCLA) && table!=cctab) {
		cbranch(tree, c=isn++, 1, reg);
		rcexpr(&czero, table, reg);
		branch(isn, 0, 0);
		label(c);
		rcexpr(&cone, table, reg);
		label(isn++);
		return(reg);
	}
	if(c==QUEST) {
		if (table==cctab)
			return(-1);
		cbranch(tree->tr1, c=isn++, 0, reg);
		flag = nstack;
		rreg = rcexpr(p1->tr1, table, reg);
		nstack = flag;
		branch(r=isn++, 0, 0);
		label(c);
		reg = rcexpr(p1->tr2, table, rreg);
		if (rreg!=reg)
			movreg(reg, rreg, tree->tr2);
		label(r);
		return(rreg);
	}
	reg = oddreg(tree, reg);
	reg1 = reg+1;
	/*
	 * Leaves of the expression tree
	 */
	if ((r = chkleaf(tree, table, reg)) >= 0)
		return(r);
	/*
	 * x + (-1) is better done as x-1.
	 */

	if ((tree->op==PLUS||tree->op==ASPLUS) &&
	    (p1=tree->tr2)->op == CON && p1->value == -1) {
		p1->value = 1;
		tree->op += (MINUS-PLUS);
	}
	if (table==cregtab)
		table = regtab;
	/*
	 * Account for registers already in use below.
	 * (The PDP-11 tried efftab first for condition codes because
	 * its moves set them; native moves do not.)
	 */
	r = nreg - reg + areg - reg1 + 1;
	if ((opt=match(tree, table, r, 0))==0)
		return(-1);
	string = opt->tabstring;
	p1 = tree->tr1;
	if (p1 && p1->op==FCON && p1->value>0)
		emitfcon(p1);
	p2 = 0;
	if (opdope[tree->op&0377]&BINARY) {
		p2 = tree->tr2;
		if (p2->op==FCON && p2->value>0)
			emitfcon(p2);
	}
	atstart = 1;
loop:
	/*
	 * The 0200 bit asks for a tab.  Native instruction lines are
	 * indented; subtree and Y escapes write whole lines themselves.
	 */
	if ((c = *string++ & 0377) & 0200) {
		c &= 0177;
		putchar('\t');
	} else if (atstart && c && c!='\n' && c!='G' && c!='K' && c!='H' && (c!='Y' || *string=='m'))
		putchar('\t');
	atstart = c=='G' || c=='K' || c=='H' || c=='Y' && *string!='m';
	switch (c) {

	case '\n':
		dbprint(tree->op);
		atstart = 1;
		break;

	case '\0':
		return(reg);

	/* A1 */
	case 'A':
		p = p1;
		goto adr;

	/* A2 */
	case 'B':
		p = p2;
		goto adr;

	adr:
		c = 0;
		while (*string=='\'') {
			c++;
			string++;
		}
		if (*string=='+') {
			c = 100;
			string++;
		}
		pname(p, c);
		goto loop;

	/* I */
	case 'M':
		if ((c = *string)=='\'')
			string++;
		else
			c = 0;
		prins(tree->op, c, instab, tree);
		goto loop;

	/* B1 */
	case 'C':
		if ((opd&LEAF) != 0)
			p = tree;
		else
			p = p1;
		goto pbyte;

	/* BF, BE */
	case 'P':
	case 'L':
		p = tree;
		if (opdope[tree->op&0377]&RELAT)
			p = tree->tr1;
		if (isfloat(p))
			printf("sd");
		else
			putchar(regwidth(p->type)==8 ? 'q' : 'l');
		goto loop;

	/* B2 */
	case 'D':
		p = p2;
	pbyte:
		switch (p->type) {
		case CHAR: case UCHAR:
			putchar('b');
			break;
		case SHORT: case USHORT:
			putchar('w');
			break;
		case FLOAT:
			printf("ss");
			break;
		case DOUBLE:
			printf("sd");
			break;
		default:
			putchar(tsize(p->type)==8 ? 'q' : 'l');
		}
		goto loop;

	/* F */
	case 'G':
		p = p1;
		flag = 01;
		goto subtre;

	/* S */
	case 'K':
		p = p2;
		flag = 02;
		goto subtre;

	/* H */
	case 'H':
		p = tree;
		flag = 04;

	subtre:
		ctable = regtab;
		if (flag&04)
			ctable = cregtab;
		c = *string++ - 'A';
		if (*string=='!') {
			string++;
			c |= 020;	/* force right register */
		}
		if ((c&02)!=0)
			ctable = sptab;
		if ((c&04)!=0)
			ctable = cctab;
		if ((c&01)!=0) {
			p = p->tr1;
			if(collcon(p) && ctable!=sptab) {
				if (p->op==STAR)
					p = p->tr1;
				p = p->tr1;
			}
		}
		if (table==lsptab && ctable==sptab)
			ctable = lsptab;
		if (c&010)
			r = reg1;
		else
			if (opdope[p->op&0377]&LEAF || p->degree < 2)
				r = reg;
			else
				r = areg;
		rreg = rcexpr(p, ctable, r);
		if (ctable!=regtab && ctable!=cregtab)
			goto loop;
		if (c&010) {
			if (c&020 && rreg!=reg1)
				movreg(rreg, reg1, p);
			else
				reg1 = rreg;
		} else if (rreg!=reg)
			if ((c&020)==0 && oddreg(tree, 0)==0
			&& (flag&04
			  || flag&01&&xdcalc(p2,nreg-rreg-1)<=(opt->tabdeg2&077)
			  || flag&02&&xdcalc(p1,nreg-rreg-1)<=(opt->tabdeg1&077))) {
				reg = rreg;
				reg1 = rreg+1;
			} else
				movreg(rreg, reg, p);
		goto loop;

	/* R */
	case 'I':
		r = reg;
		goto preg;

	/* R1 */
	case 'J':
		r = reg1;
	preg:
		if (*string=='-')
			string++;
		if (*string=='+') {
			string++;
			r++;
		}
		if (r>=nreg)
			error("Register overflow: simplify expression");
		p = tree;
		if (opdope[tree->op&0377]&RELAT)
			p = tree->tr1;
		c = 0;
		if (*string=='.') {
			string++;
			switch (*string++) {
			case 'q': c = 8; break;
			case 'l': c = 4; break;
			case 'w': c = 2; break;
			case 'b': c = 1; break;
			case '1': c = tsize(p1->type); break;
			case '2': c = tsize(p2->type); break;
			}
		}
		if (c==0 && isfloat(p))
			printf("%%xmm%d", r);
		else {
			if (c==0)
				c = regwidth(p->type);
			printf("%s", regname[c==8?0:c==4?1:c==2?2:3][r]);
		}
		goto loop;

	/* #1 */
	case '#':
		p = p1->tr1;
		goto nmbr;

	/* #2 */
	case '"':
		p = p2->tr1;

	nmbr:
		if(collcon(p)) {
			if ((p = p->tr2)->op == CON) {
				if (p->value)
					psoct(p->value);
			} else if (p->op==AMPER)
				pname(p->tr1, 0);
		}
		goto loop;

	/*
	 * Mask used in field assignments: the complement of the
	 * field within the word, for "and".
	 */
	case 'Z':
		printf("$%ld", (long)(tsize(p1->type)==1 ? (int8_t)~tree->mask :
		    tsize(p1->type)==2 ? (int16_t)~tree->mask : (int32_t)~tree->mask));
		goto loop;

	/*
	 * Native machine sequences.
	 */
	case 'Y':
		native(*string++, tree, p1, p2, reg, reg1);
		goto loop;
	}
	putchar(c);
	goto loop;
}

/*
 * Floating constants are emitted, once, as read-only doubles.
 */
static void
emitfcon(struct tnode *p)
{
	uint64_t bits;
	double d;

	d = p->fvalue;
	memcpy(&bits, &d, sizeof(bits));
	printf("\t.section\t.rodata\n\t.balign\t8\n.L%d:\n\t.quad\t%lu\n\t.text\n", (int)p->value, (unsigned long)bits);
	p->value = -p->value;
}

/*
 * This routine just calls sreorder (below)
 * on the subtrees and then on the tree itself.
 * It returns non-zero if anything changed.
 */
/*
 * Is the value of the tree (not of its operands) floating?
 */
int
isfvalue(struct tnode *t)
{
	return(t && (t->type==FLOAT || t->type==DOUBLE) && (opdope[t->op&0377]&RELAT)==0);
}

int
reorder(struct tnode **treep, struct table *table, int reg)
{
	register int r, o;
	register struct tnode *p;

	p = *treep;
	o = p->op;
	if (opdope[o&0377]&LEAF || o==LOGOR || o==LOGAND || o>=200)
		return(0);
	while(sreorder(&p->tr1, regtab, reg, 1))
		;
	if (opdope[o&0377]&BINARY)
		while(sreorder(&p->tr2, regtab, reg, 1))
			;
	r = 0;
	if (table!=cctab)
	while (sreorder(treep, table, reg, 0))
		r++;
	*treep = optim(*treep);
	return(r);
}

/*
 * Basically this routine carries out two kinds of optimization.
 * First, it observes that "x + (reg = y)" where actually
 * the = is any assignment op is better done as "reg=y; x+reg".
 * In this case rcexpr is called to do the first part and the
 * tree is modified so the name of the register
 * replaces the assignment.
 * Moreover, expressions like "reg = x+y" are best done as
 * "reg = x; reg += y" (so long as "reg" and "y" are not the same!).
 */
int
sreorder(struct tnode **treep, struct table *table, int reg, int recurf)
{
	register struct tnode *p, *p1;

	p = *treep;
	if (p==0 || p->op>=200 || opdope[p->op&0377]&LEAF)
		return(0);
	if (p->op==PLUS && recurf)
		if (reorder(&p->tr2, table, reg))
			*treep = p = optim(p);
	p1 = p->tr1;
	if (p->op==STAR || p->op==PLUS) {
		if (recurf && reorder(&p->tr1, table, reg))
			*treep = p = optim(p);
		p1 = p->tr1;
	}
	if (p1 && p1->op==NAME) switch(p->op) {
		case ASLSH:
		case ASRSH:
		case ASSIGN:
			if (p1->class != REG||isnarrow(p1->type)||isfloat(p->tr2)
			 || regwidth(p1->type)!=regwidth(p->tr2->type))
				return(0);
			if (p->op==ASSIGN) switch (p->tr2->op) {
			case TIMES:
				if (!ispow2(p->tr2))
					break;
				p->tr2 = pow2(p->tr2);
			case PLUS:
			case MINUS:
			case AND:
			case OR:
			case EXOR:
			case LSHIFT:
			case RSHIFT:
				p1 = p->tr2->tr2;
				if (xdcalc(p1, 16) > 12
				 || p1->op==NAME
				 &&(p1->nloc==p->tr1->nloc
				  || p1->regno==p->tr1->nloc))
					return(0);
				if (p->tr2->tr1->type != p->tr1->type)
					return(0);
				p1 = p->tr2;
				p->tr2 = p1->tr1;
				if (p1->tr1->op!=NAME
				 || p1->tr1->class!=REG
				 || p1->tr1->nloc!=p->tr1->nloc)
					rcexpr(p, efftab, reg);
				p->tr2 = p1->tr2;
				p->op = p1->op + ASPLUS - PLUS;
				*treep = p;
				return(1);
			}
			goto OK;

		case ASTIMES:
			if (!ispow2(p))
				return(0);
		case ASPLUS:
		case ASMINUS:
		case ASAND:
		case ASOR:
		case ASXOR:
		case INCBEF:
		case DECBEF:
			if (p1->class != REG || isfloat(p))
				return(0);
		OK:
			if (table==cctab||table==cregtab)
				reg += 020;
			rcexpr(optim(p), efftab, ~reg);
			*treep = p1;
			return(1);
	}
	return(0);
}

/*
 * Delay handles postfix ++ and --
 * It observes that "x + y++" is better
 * treated as "x + y; y++".
 * If the operator is ++ or -- itself,
 * it calls rcexpr to load the operand, letting
 * the calling instance of rcexpr to do the
 * ++ using efftab.
 * Otherwise it uses sdelay to search for inc/dec
 * among the operands.
 */
int
delay(struct tnode **treep, struct table *table, int reg)
{
	register struct tnode *p, *p1;
	register int r;

	p = *treep;
	if ((p->op==INCAFT||p->op==DECAFT)
	 && p->tr1->op==NAME) {
		return(1+rcexpr(p->tr1, table, reg));
	}
	p1 = 0;
	if (opdope[p->op&0377]&BINARY) {
		if (p->op==LOGAND || p->op==LOGOR)
			return(0);
		p1 = sdelay(&p->tr2);
	}
	if (p1==0 && p->tr1)
		p1 = sdelay(&p->tr1);
	if (p1) {
		r = rcexpr(optim(p), table, reg);
		*treep = p1;
		return(r+1);
	}
	return(0);
}

struct tnode *
sdelay(struct tnode **ap)
{
	register struct tnode *p, *p1;

	p = *ap;
	if (p==0)
		return(0);
	if ((p->op==INCAFT||p->op==DECAFT) && p->tr1->op==NAME) {
		*ap = ncopy(p->tr1);
		return(p);
	}
	if (p->op==STAR || p->op==PLUS)
		if ((p1=sdelay(&p->tr1)))
			return(p1);
	if (p->op==PLUS)
		return(sdelay(&p->tr2));
	return(0);
}

/*
 * Copy a tree node for a register variable.
 * Used by sdelay because if *reg-- is turned
 * into *reg; reg-- the *reg will in turn
 * be changed to some offset class, accidentally
 * modifying the reg--.
 */
struct tnode *
ncopy(struct tnode *ap)
{
	register struct tnode *p, *q;

	p = ap;
	if (p->class!=REG)
		return(p);
	q = getblk(sizeof(*p));
	*q = *p;
	return(q);
}

/*
 * If the tree can be immediately loaded into a register,
 * produce code to do so and return success.
 */
int
chkleaf(struct tnode *atree, struct table *table, int reg)
{
	struct tnode lbuf;
	register struct tnode *tree;

	tree = atree;
	if (tree->op!=STAR && tree->op!=CON && tree->op!=LCON && dcalc(tree, nreg-reg) > 12)
		return(-1);
	memset(&lbuf, 0, sizeof(lbuf));
	lbuf.op = LOAD;
	lbuf.type = tree->type;
	lbuf.degree = tree->degree;
	lbuf.tr1 = tree;
	return(rcexpr(&lbuf, table, reg));
}

/*
 * Compile a call.  Arguments are evaluated one at a time into their
 * final System V locations in a single aligned block: stack-class
 * arguments at the bottom, register-class values staged above them,
 * and the function address last.  Structures are classified by c0
 * (their eightbytes go to registers only if all fit) and copied; a
 * MEMORY-class structure result is written through a hidden pointer
 * to a frame temporary.  No scratch register is live during the call;
 * live registers below reg are saved around it.
 */
static void
copyarg(struct tnode *a, int off)
{
	struct tnode *e;
	int r;

	e = strfunc(a->tr1);
	r = rcexpr(optim(tnode(AMPER, STRUCT+PTR, e, NULL)), regtab, 0);
	printf("\tmovq\t%s,%%rsi\n\tleaq\t%d(%%rsp),%%rdi\n\tmovq\t$%ld,%%rcx\n\tcld\n\trep movsb\n",
	    regname[0][r], off, (long)a->mask);
}

void
callexpr(struct tnode *tree, int reg)
{
	struct tnode *args[256], *a, *fn;
	int nargs, i, j, ngp, nsse, slot[256], memarg[256], gp[256][2], sse[256][2];
	int nstk, total, fnslot, direct, saved, sclass, need[3], r;
	static char *gpreg[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
	static char *ret[] = { "%rax", "%rdx" };

	nargs = 0;
	a = tree->tr2;
	if (a && a->op) {
		while (a->op==COMMA) {
			if (nargs >= 255)
				break;
			args[nargs++] = a->tr2;
			a = a->tr1;
		}
		args[nargs++] = a;
	}
	/* collected last to first */
	for (i = 0; i < nargs/2; i++) {
		a = args[i];
		args[i] = args[nargs-1-i];
		args[nargs-1-i] = a;
	}
	sclass = tree->mask ? tree->strclass : CP_CNONE;
	ngp = nsse = nstk = 0;
	if (CP_CLASS(sclass, 0)==CP_CMEM)
		ngp = 1;		/* hidden result pointer */
	for (i = 0; i < nargs; i++) {
		a = args[i];
		gp[i][0] = gp[i][1] = sse[i][0] = sse[i][1] = -1;
		memarg[i] = 0;
		if (a->op==STRASG) {
			int c = a->strclass;

			need[CP_CINT] = need[CP_CSSE] = 0;
			for (j = 0; j < 2; j++)
				if (CP_CLASS(c, j)==CP_CINT || CP_CLASS(c, j)==CP_CSSE)
					need[CP_CLASS(c, j)]++;
			if (CP_CLASS(c, 0)!=CP_CMEM && ngp+need[CP_CINT] <= 6 && nsse+need[CP_CSSE] <= 8) {
				for (j = 0; j < 2; j++) {
					if (CP_CLASS(c, j)==CP_CINT)
						gp[i][j] = ngp++;
					else if (CP_CLASS(c, j)==CP_CSSE)
						sse[i][j] = nsse++;
				}
			} else {
				memarg[i] = 1;
				slot[i] = 8*nstk;
				nstk += (a->mask + 7) / 8;
			}
			continue;
		}
		if (isfvalue(a)) {
			if (nsse < 8)
				sse[i][0] = nsse++;
		} else if (ngp < 6)
			gp[i][0] = ngp++;
		if (gp[i][0] < 0 && sse[i][0] < 0) {
			memarg[i] = 1;
			slot[i] = 8*nstk++;
		}
	}
	total = 8*nstk;
	for (i = 0; i < nargs; i++)
		if (!memarg[i]) {
			slot[i] = total;
			total += args[i]->op==STRASG ? 16 : 8;
		}
	fn = tree->tr1;
	direct = fn->op==NAME && (fn->class==EXTERN || fn->class==STATIC);
	fnslot = total;
	if (!direct)
		total += 8;
	/* save live scratch registers below reg */
	saved = 0;
	for (i = 0; i < reg; i++) {
		printf("\tpushq\t%s\n\tsubq\t$8,%%rsp\n\tmovsd\t%%xmm%d,(%%rsp)\n", regname[0][i], i);
		nstack += 2;
		saved++;
	}
	if ((nstack*8 + total) % 16)
		total += 8;
	if (total)
		printf("\tsubq\t$%d,%%rsp\n", total);
	nstack += total/8;
	if (!direct) {
		fn = fn->op==STAR ? fn->tr1 : tnode(AMPER, incref(fn->type), fn, NULL);
		r = rcexpr(optim(fn), regtab, 0);
		printf("\tmovq\t%s,%d(%%rsp)\n", regname[0][r], fnslot);
	}
	for (i = 0; i < nargs; i++) {
		a = args[i];
		if (a->op==STRASG) {
			copyarg(a, slot[i]);
			continue;
		}
		r = rcexpr(a, regtab, 0);
		if (a->op==CP_SINGLE || isfvalue(a))
			printf("\tmovsd\t%%xmm%d,%d(%%rsp)\n", r, slot[i]);
		else {
			if (regwidth(a->type)==4) {
				if (isunsigned(a->type))
					printf("\tmovl\t%s,%s\n", regname[1][r], regname[1][r]);
				else
					printf("\tmovslq\t%s,%s\n", regname[1][r], regname[0][r]);
			}
			printf("\tmovq\t%s,%d(%%rsp)\n", regname[0][r], slot[i]);
		}
	}
	for (i = 0; i < nargs; i++) {
		if (memarg[i])
			continue;
		for (j = 0; j < 2; j++) {
			if (sse[i][j] >= 0)
				printf("\tmovsd\t%d(%%rsp),%%xmm%d\n", slot[i]+8*j, sse[i][j]);
			if (gp[i][j] >= 0)
				printf("\tmovq\t%d(%%rsp),%s\n", slot[i]+8*j, gpreg[gp[i][j]]);
		}
	}
	if (CP_CLASS(sclass, 0)==CP_CMEM)
		printf("\tleaq\t%d(%%rbp),%%rdi\n", tree->nloc);
	/* AL bounds the vector registers a variadic callee must save */
	printf("\tmovl\t$%d,%%eax\n", nsse);
	if (direct) {
		printf("\tcall\t");
		pbase(fn);
		putchar('\n');
	} else
		printf("\tmovq\t%d(%%rsp),%%r11\n\tcall\t*%%r11\n", fnslot);
	if (total)
		printf("\taddq\t$%d,%%rsp\n", total);
	nstack -= total/8;
	if (tree->mask) {
		/* a structure result is addressed in its frame temporary */
		ngp = nsse = 0;
		if (CP_CLASS(sclass, 0)!=CP_CMEM)
			for (j = 0; j < 2; j++) {
				if (CP_CLASS(sclass, j)==CP_CINT)
					printf("\tmovq\t%s,%d(%%rbp)\n", ret[ngp++], tree->nloc+8*j);
				else if (CP_CLASS(sclass, j)==CP_CSSE)
					printf("\tmovsd\t%%xmm%d,%d(%%rbp)\n", nsse++, tree->nloc+8*j);
			}
		printf("\tleaq\t%d(%%rbp),%%rax\n", tree->nloc);
	} else switch (tree->type) {
	case CHAR: printf("\tmovsbl\t%%al,%%eax\n"); break;
	case UCHAR: printf("\tmovzbl\t%%al,%%eax\n"); break;
	case SHORT: printf("\tmovswl\t%%ax,%%eax\n"); break;
	case USHORT: printf("\tmovzwl\t%%ax,%%eax\n"); break;
	case FLOAT: printf("\tcvtss2sd\t%%xmm0,%%xmm0\n"); break;
	}
	if (saved) {
		printf("\tmovq\t%%rax,%%r11\n\tmovapd\t%%xmm0,%%xmm15\n");
		for (i = reg-1; i >= 0; i--) {
			printf("\tmovsd\t(%%rsp),%%xmm%d\n\taddq\t$8,%%rsp\n\tpopq\t%s\n", i, regname[0][i]);
			nstack -= 2;
		}
		if (isfvalue(tree) && !tree->mask)
			printf("\tmovapd\t%%xmm15,%%xmm%d\n", reg);
		else
			printf("\tmovq\t%%r11,%s\n", regname[0][reg]);
	} else if (reg != 0) {
		if (tree->mask)
			printf("\tmovq\t%%rax,%s\n", regname[0][reg]);
		else
			movreg(0, reg, tree);
	}
}

struct tnode *
strfunc(struct tnode *atp)
{
	register struct tnode *tp;

	tp = atp;
	if (tp->op != CALL)
		return(tp);
	setype(tp, STRUCT+PTR);
	return(tnode(STAR, STRUCT, tp, NULL));
}

/*
 * Compile an initializing expression
 */
void
doinit(int atype, struct tnode *atree)
{
	register struct tnode *tree;
	register int type;
	float sfval;
	double fval;
	LTYPE lval;
	uint32_t fbits;
	uint64_t dbits;
	char *dir;

	tree = atree;
	type = atype;
	dir = tsize(type)==1 ? ".byte" : tsize(type)==2 ? ".short" : tsize(type)==4 ? ".long" : ".quad";
	if (type==FLOAT || type==DOUBLE)
		goto flt;
	if (isnarrow(type) && tree->type&XTYPE)
		goto illinit;
	while (tree->op==ITOL || tree->op==LTOI || tree->op==ITOC)
		tree = tree->tr1;
	if (tree->op==FTOI || tree->op==FTOL) {
		if (tree->tr1->op!=FCON && tree->tr1->op!=SFCON)
			goto illinit;
		lval = (LTYPE)tree->tr1->fvalue;
		printf("\t%s\t%ld\n", dir, (long)lval);
		return;
	}
	if (tree->op == CON || tree->op==LCON) {
		lval = tree->value;
		if (tsize(type)==1)
			lval = (uint8_t)lval;
		else if (tsize(type)==2)
			lval = (uint16_t)lval;
		else if (tsize(type)==4)
			lval = (uint32_t)lval;
		printf("\t%s\t%ld\n", dir, (long)lval);
		return;
	}
	if (tree->op==AMPER && tsize(type)==8) {
		printf("\t.quad\t");
		pname(tree->tr1, 0);
		putchar('\n');
		return;
	}
	if (tree->op==PLUS && tree->tr1->op==AMPER && tree->tr2->op==CON && tsize(type)==8) {
		tree->tr1->tr1->offset += tree->tr2->value;
		printf("\t.quad\t");
		pname(tree->tr1->tr1, 0);
		putchar('\n');
		return;
	}
	goto illinit;

flt:
	if (tree->op==ITOF) {
		if (tree->tr1->op==CON)
			fval = tree->tr1->value;
		else
			goto illinit;
	} else if (tree->op==FCON || tree->op==SFCON)
		fval = tree->fvalue;
	else if (tree->op==LTOF) {
		if (tree->tr1->op!=LCON && tree->tr1->op!=CON)
			goto illinit;
		fval = tree->tr1->type==ULONG ? (double)(uint64_t)tree->tr1->value : (double)tree->tr1->value;
	} else
		goto illinit;
	if (type==FLOAT) {
		sfval = fval;
		memcpy(&fbits, &sfval, 4);
		printf("\t.long\t%lu\n", (unsigned long)fbits);
	} else {
		memcpy(&dbits, &fval, 8);
		printf("\t.quad\t%lu\n", (unsigned long)dbits);
	}
	return;
illinit:
	error("Illegal initialization");
}

void
movreg(int r0, int r1, struct tnode *tree)
{
	if (r0==r1)
		return;
	if (isfvalue(tree))
		printf("\tmovapd\t%%xmm%d,%%xmm%d\n", r0, r1);
	else
		printf("\tmovq\t%s,%s\n", regname[0][r0], regname[0][r1]);
}

/*
 * Native machine sequences named by Y escapes in the code table.
 * Values are in R (reg) and R1 (reg1); r11 and xmm15 are temporaries.
 */
static char *
rn(int r, int w)
{
	return(regname[w==8?0:w==4?1:w==2?2:3][r]);
}

static void
vaarg(struct tnode *tree, int reg)
{
	int over, done, w;
	char *r;

	over = isn++;
	done = isn++;
	r = regname[0][reg];
	if (isfloat(tree)) {
		printf("\tmovl\t4(%s),%%r11d\n\tcmpl\t$176,%%r11d\n\tjae\t.L%d\n", r, over);
		printf("\taddq\t16(%s),%%r11\n\taddl\t$16,4(%s)\n\tjmp\t.L%d\n", r, r, done);
	} else {
		printf("\tmovl\t(%s),%%r11d\n\tcmpl\t$48,%%r11d\n\tjae\t.L%d\n", r, over);
		printf("\taddq\t16(%s),%%r11\n\taddl\t$8,(%s)\n\tjmp\t.L%d\n", r, r, done);
	}
	label(over);
	printf("\tmovq\t8(%s),%%r11\n\taddq\t$8,8(%s)\n", r, r);
	label(done);
	if (isfloat(tree)) {
		printf("\tmovsd\t(%%r11),%%xmm%d\n", reg);	/* float arguments are promoted */
		return;
	}
	w = regwidth(tree->type);
	switch (tree->type) {
	case CHAR: printf("\tmovsbl\t(%%r11),%s\n", rn(reg, 4)); break;
	case UCHAR: printf("\tmovzbl\t(%%r11),%s\n", rn(reg, 4)); break;
	case SHORT: printf("\tmovswl\t(%%r11),%s\n", rn(reg, 4)); break;
	case USHORT: printf("\tmovzwl\t(%%r11),%s\n", rn(reg, 4)); break;
	default: printf("\tmov%c\t(%%r11),%s\n", w==8 ? 'q' : 'l', rn(reg, w));
	}
}

/*
 * va_start, va_arg, va_end and va_copy on the System V va_list:
 * gp_offset, fp_offset, overflow_arg_area, reg_save_area.
 */
static void
vaops(struct tnode *tree, int reg)
{
	int r, r1;
	char *rp;

	r = rcexpr(tree->tr1, regtab, reg);
	if (r != reg)
		movreg(r, reg, tree->tr1);
	rp = regname[0][reg];
	switch (tree->op) {
	case CP_VASTART:
		printf("\tmovl\t$%d,(%s)\n\tmovl\t$%d,4(%s)\n", (int)tree->value, rp, (int)tree->offset, rp);
		printf("\tleaq\t%d(%%rbp),%%r11\n\tmovq\t%%r11,8(%s)\n", (int)tree->mask, rp);
		printf("\tleaq\t%d(%%rbp),%%r11\n\tmovq\t%%r11,16(%s)\n", tree->nloc, rp);
		return;
	case CP_VAARG:
		vaarg(tree, reg);
		return;
	case CP_VAEND:
		return;
	case CP_VACOPY:
		if (reg+1 >= nreg)
			error("Register overflow: simplify expression");
		r1 = rcexpr(tree->tr2, regtab, reg+1);
		printf("\tmovq\t(%s),%%r11\n\tmovq\t%%r11,(%s)\n", regname[0][r1], rp);
		printf("\tmovq\t8(%s),%%r11\n\tmovq\t%%r11,8(%s)\n", regname[0][r1], rp);
		printf("\tmovq\t16(%s),%%r11\n\tmovq\t%%r11,16(%s)\n", regname[0][r1], rp);
		return;
	}
}

void
native(int code, struct tnode *tree, struct tnode *p1, struct tnode *p2, int reg, int reg1)
{
	int w, uns, l1, l2;
	char *r, *r1, s;
	LTYPE v;

	w = regwidth(tree->type);
	if (opdope[tree->op&0377]&RELAT)
		w = regwidth(tree->tr1->type);
	if (reg>=nreg || reg1>=nreg && code!='k' && code!='m' && code!='u' && code!='v' && code!='c' && code!='n')
		error("Register overflow: simplify expression");
	r = rn(reg, w);
	r1 = rn(reg1<nreg ? reg1 : reg, w);
	s = w==8 ? 'q' : 'l';
	uns = isunsigned(tree->type);
	switch (code) {

	/* load a constant */
	case 'k':
		v = p1->value;
		if (w==4)
			printf("\tmovl\t$%d,%s\n", (int)(int32_t)v, rn(reg, 4));
		else if (v >= INT32_MIN && v <= INT32_MAX)
			printf("\tmovq\t$%ld,%s\n", (long)v, rn(reg, 8));
		else
			printf("\tmovabsq\t$%ld,%s\n", (long)v, rn(reg, 8));
		return;

	/* load mnemonic for the first operand's type */
	case 'm':
		switch (p1->op==AMPER ? PTR : p1->type) {
		case CHAR: printf("movsbl"); break;
		case UCHAR: printf("movzbl"); break;
		case SHORT: printf("movswl"); break;
		case USHORT: printf("movzwl"); break;
		case FLOAT: printf("cvtss2sd"); break;
		case DOUBLE: printf("movsd"); break;
		default: printf("mov%c", tsize(p1->type)==8 || p1->op==AMPER ? 'q' : 'l');
		}
		return;

	/* integer division and remainder of R by R1 */
	case 'd':
		printf("\tmovq\t%s,%%r11\n\tpushq\t%%rax\n\tpushq\t%%rdx\n\tmovq\t%s,%%rax\n", rn(reg1, 8), rn(reg, 8));
		if (uns)
			printf("\txorl\t%%edx,%%edx\n\tdiv%c\t%s\n", s, w==8 ? "%r11" : "%r11d");
		else
			printf("\t%s\n\tidiv%c\t%s\n", w==8 ? "cqto" : "cltd", s, w==8 ? "%r11" : "%r11d");
		printf("\tmovq\t%s,%%r11\n\tpopq\t%%rdx\n\tpopq\t%%rax\n\tmovq\t%%r11,%s\n",
		    tree->op==MOD || tree->op==ASMOD ? "%rdx" : "%rax", rn(reg, 8));
		return;

	/* shift R by the count in R1 */
	case 'h':
		printf("\tmovq\t%s,%%r11\n\tpushq\t%%rcx\n\tmovq\t%s,%%rcx\n\t", rn(reg, 8), rn(reg1, 8));
		prins(tree->op, 0, instab, tree);
		printf("%c\t%%cl,%s\n\tpopq\t%%rcx\n\tmovq\t%%r11,%s\n", s, w==8 ? "%r11" : "%r11d", rn(reg, 8));
		return;

	/* floating negation */
	case 'n':
		printf("\tmovabsq\t$-9223372036854775808,%%r11\n\tmovq\t%%r11,%%xmm15\n\txorpd\t%%xmm15,%%xmm%d\n", reg);
		return;

	/* pop the stacked operand into R1 (or R1+) */
	case 'P':
	case 'p':
		printf("\tpopq\t%s\n", rn(code=='P' ? reg1 : reg1+1, 8));
		nstack--;
		return;
	case 'Q':
		printf("\tmovsd\t(%%rsp),%%xmm%d\n\taddq\t$8,%%rsp\n", reg1);
		nstack--;
		return;

	/* push an addressable 64-bit operand or constant */
	case 'u':
		printf("\tpushq\t");
		pname(p1, 0);
		putchar('\n');
		nstack++;
		return;

	/* max and min */
	case 'x':
		if (isfloat(tree)) {
			printf("\t%s\t%%xmm%d,%%xmm%d\n", tree->op==MAX||tree->op==MAXP ? "maxsd" : "minsd", reg1, reg);
			return;
		}
		printf("\tcmp%c\t%s,%s\n\tcmov%s%c\t%s,%s\n", s, r1, r, tree->op==MAX ? "l" : tree->op==MAXP ? "b" :
		    tree->op==MIN ? "g" : "a", s, r1, r);
		return;

	/* conversions of R from the operand type to the tree type */
	case 'c':
		switch (tree->op) {
		case ITOL:
			if (isunsigned(p1->type))
				printf("\tmovl\t%s,%s\n", rn(reg, 4), rn(reg, 4));
			else
				printf("\tmovslq\t%s,%s\n", rn(reg, 4), rn(reg, 8));
			return;
		case LTOI:
			return;
		case ITOC:
			switch ((int)tree->value) {
			case CHAR: printf("\tmovsbl\t%s,%s\n", rn(reg, 1), rn(reg, 4)); break;
			case UCHAR: printf("\tmovzbl\t%s,%s\n", rn(reg, 1), rn(reg, 4)); break;
			case SHORT: printf("\tmovswl\t%s,%s\n", rn(reg, 2), rn(reg, 4)); break;
			case USHORT: printf("\tmovzwl\t%s,%s\n", rn(reg, 2), rn(reg, 4)); break;
			default: printf("\tmovsbl\t%s,%s\n", rn(reg, 1), rn(reg, 4));
			}
			return;
		case ITOF:
			printf("\tcvtsi2sdl\t%s,%%xmm%d\n", rn(reg, 4), reg);
			return;
		case FTOI:
			printf("\tcvttsd2si\t%%xmm%d,%s\n", reg, rn(reg, 4));
			return;
		case LTOF:
			if (p1->type==ULONG) {
				l1 = isn++;
				l2 = isn++;
				printf("\tmovq\t%s,%%r11\n\ttestq\t%%r11,%%r11\n\tjns\t.L%d\n", rn(reg, 8), l1);
				printf("\tshrq\t$1,%%r11\n\tandl\t$1,%s\n\torq\t%s,%%r11\n", rn(reg, 4), rn(reg, 8));
				printf("\tcvtsi2sdq\t%%r11,%%xmm%d\n\taddsd\t%%xmm%d,%%xmm%d\n\tjmp\t.L%d\n", reg, reg, reg, l2);
				label(l1);
				printf("\n\tcvtsi2sdq\t%%r11,%%xmm%d\n", reg);
				label(l2);
				printf("\n");
			} else
				printf("\tcvtsi2sdq\t%s,%%xmm%d\n", rn(reg, 8), reg);
			return;
		case FTOL:
			if (tree->type==ULONG) {
				l1 = isn++;
				l2 = isn++;
				printf("\tmovabsq\t$4890909195324358656,%%r11\n\tmovq\t%%r11,%%xmm15\n");
				printf("\tucomisd\t%%xmm15,%%xmm%d\n\tjb\t.L%d\n\tsubsd\t%%xmm15,%%xmm%d\n", reg, l1, reg);
				printf("\tcvttsd2siq\t%%xmm%d,%s\n\tmovabsq\t$-9223372036854775808,%%r11\n\txorq\t%%r11,%s\n\tjmp\t.L%d\n", reg, rn(reg, 8), rn(reg, 8), l2);
				label(l1);
				printf("\n\tcvttsd2siq\t%%xmm%d,%s\n", reg, rn(reg, 8));
				label(l2);
				printf("\n");
			} else
				printf("\tcvttsd2siq\t%%xmm%d,%s\n", reg, rn(reg, 8));
			return;
		}
		error("Unknown conversion %d", tree->op);
		return;

	/* the variadic builtins */
	case 'v':
		vaops(tree, reg);
		return;
	}
	error("Unknown table escape %c", code);
}
