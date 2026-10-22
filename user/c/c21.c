/*
 * C object code improver-- second part
 *
 * The PDP-11 register-content tracker (rmove) is retargeted as a
 * conservative AMD64 one: a register known to hold the value of a
 * memory operand makes a later identical load redundant.  Anything not
 * understood forgets everything.  The PDP-11 sob, (pc) literal, cfcc
 * and constant-compare rewrites have no AMD64 counterpart.
 */

#include "c2.h"

char	*lastsegcode;

/* register names, normalized to the 64-bit register */
static char *gpr[] = {
	"rax","eax","ax","al", "rbx","ebx","bx","bl", "rcx","ecx","cx","cl",
	"rdx","edx","dx","dl", "rsi","esi","si","sil", "rdi","edi","di","dil",
	"rbp","ebp","bp","bpl", "rsp","esp","sp","spl",
	"r8","r8d","r8w","r8b", "r9","r9d","r9w","r9b", "r10","r10d","r10w","r10b",
	"r11","r11d","r11w","r11b", "r12","r12d","r12w","r12b", "r13","r13d","r13w","r13b",
	"r14","r14d","r14w","r14b", "r15","r15d","r15w","r15b", 0
};
#define	NREGS	(16+16)

/* known contents: register n equals memory text with width w */
static struct {
	char	mem[128];
	int	width;
} regs[NREGS];

static int
regnum(const char *s, int *width)
{
	register int i;
	static int widths[] = { 8, 4, 2, 1 };

	if (*s++ != '%')
		return(-1);
	if (strncmp(s, "xmm", 3)==0) {
		i = atoi(s+3);
		if (width)
			*width = 8;
		return(i>=0 && i<16 ? 16+i : -1);
	}
	for (i = 0; gpr[i]; i++)
		if (strcmp(s, gpr[i])==0) {
			if (width)
				*width = widths[i%4];
			return(i/4);
		}
	return(-1);
}

void
clearreg(void)
{
	register int i;

	for (i=0; i<NREGS; i++)
		regs[i].mem[0] = '\0';
}

/* split "op a,b" operands at the top-level comma */
static int
operands(char *code, char *a, char *b, char *mn)
{
	register char *p;
	int depth, n;

	p = code;
	while (*p==' ' || *p=='\t')
		p++;
	n = 0;
	while (*p && *p!=' ' && *p!='\t' && n < 31)
		mn[n++] = *p++;
	mn[n] = 0;
	while (*p==' ' || *p=='\t')
		p++;
	a[0] = b[0] = 0;
	depth = 0;
	for (n = 0; *p && (depth || *p!=','); p++) {
		if (*p=='(')
			depth++;
		else if (*p==')')
			depth--;
		if (n < 127)
			a[n++] = *p;
	}
	a[n] = 0;
	if (*p!=',')
		return(a[0] ? 1 : 0);
	p++;
	for (n = 0; *p && n < 127; p++)
		b[n++] = *p;
	b[n] = 0;
	if (strchr(b, ','))
		return(3);
	return(2);
}

/* does the operand text name general register r (any width)? */
static int
mentions(const char *s, int r)
{
	register int k;
	size_t n;
	const char *p;

	if (r < 0 || r >= 16)
		return(0);
	for (p = s; *p; p++) {
		if (*p!='%')
			continue;
		for (k = 0; k < 4; k++) {
			n = strlen(gpr[4*r+k]);
			if (strncmp(p+1, gpr[4*r+k], n)==0
			 && !(p[1+n]>='a' && p[1+n]<='z' || p[1+n]>='0' && p[1+n]<='9'))
				return(1);
		}
	}
	return(0);
}

/* a register was written: forget it and memory addressed through it */
static void
wrote(int r)
{
	register int i;

	if (r < 0)
		return;
	regs[r].mem[0] = 0;
	if (r >= 16)
		return;
	for (i = 0; i < NREGS; i++)
		if (regs[i].mem[0] && mentions(regs[i].mem, r))
			regs[i].mem[0] = 0;
}

/* memory was written somewhere: forget every memory association */
static void
storeany(void)
{
	register int i;

	for (i = 0; i < NREGS; i++)
		regs[i].mem[0] = 0;
}

static int
ismem(const char *s)
{
	return(*s && *s!='%' && *s!='$');
}

/* two-operand instructions that write only their last operand */
static char *pure[] = {
	"add", "sub", "and", "or", "xor", "imul", "shl", "shr", "sar", "lea",
	"movs", "movz", "movabsq", "cvtsi2sd", "cvttsd2si", "cvtsd2ss", "cvtss2sd",
	"movapd", "movsd", "movss", "xorpd", "addsd", "subsd", "mulsd", "divsd",
	"maxsd", "minsd", "neg", "not", "inc", "dec", "cmov", "set", 0
};

void
rmove(void)
{
	register struct node *p;
	register int i, r, rs;
	int n, w, ws;
	char a[128], b[128], mn[32];

	clearreg();
	for (p=first.forw; p!=0; p = p->forw) {
		switch (p->op) {

		case MOV:
			n = operands(p->code, a, b, mn);
			if (n != 2) {
				clearreg();
				continue;
			}
			r = regnum(b, &w);
			rs = regnum(a, &ws);
			if (r >= 0 && r < 16 && ismem(a)) {
				/* a load: redundant if the register already holds it */
				if (regs[r].mem[0] && regs[r].width==p->subop && strcmp(regs[r].mem, a)==0) {
					p->forw->back = p->back;
					p->back->forw = p->forw;
					redunm++;
					nchange++;
					continue;
				}
				wrote(r);
				if (!mentions(a, r) && strlen(a) < 127) {
					strcpy(regs[r].mem, a);
					regs[r].width = p->subop;
				}
				continue;
			}
			if (rs >= 0 && rs < 16 && ismem(b)) {
				/* a store: the register now equals the memory */
				storeany();
				if (!mentions(b, rs) && strlen(b) < 127) {
					strcpy(regs[rs].mem, b);
					regs[rs].width = p->subop;
				}
				continue;
			}
			if (ismem(b))
				storeany();
			else
				wrote(r);
			continue;

		case 13:	/* other instructions */
			n = operands(p->code, a, b, mn);
			for (i = 0; pure[i]; i++)
				if (strncmp(mn, pure[i], strlen(pure[i]))==0)
					break;
			if (strncmp(mn, "cmp", 3)==0 || strncmp(mn, "test", 4)==0 || strncmp(mn, "ucomi", 5)==0)
				continue;
			if (pure[i]==0 || n==3) {
				clearreg();
				continue;
			}
			w = n==2 ? 1 : 0;
			if (ismem(w ? b : a))
				storeany();
			else
				wrote(regnum(w ? b : a, NULL));
			continue;

		default:
			clearreg();
		}
	}
	clearreg();
}

int
jumpsw(void)
{
	register struct node *p, *p1;
	register int t;
	register struct node *tp;
	int nj;

	t = 0;
	nj = 0;
	for (p=first.forw; p!=0; p = p->forw)
		p->refc = ++t;
	for (p=first.forw; p!=0; p = p1) {
		p1 = p->forw;
		if (p->op == CBR && p1 && p1->op==JBR && p->ref && p1->ref
		 && iabs(p->refc - p->ref->refc) > iabs(p1->refc - p1->ref->refc)) {
			if (p->ref==p1->ref)
				continue;
			p->subop = revbr[p->subop];
			tp = p1->ref;
			p1->ref = p->ref;
			p->ref = tp;
			t = p1->labno;
			p1->labno = p->labno;
			p->labno = t;
			nrevbr++;
			nj++;
		}
	}
	/* refcount() recomputes the reference counts used above */
	return(nj);
}

int
iabs(int x)
{
	return(x<0? -x: x);
}

int
equop(struct node *ap1, struct node *p2)
{
	register char *cp1, *cp2;
	register struct node *p1;

	p1 = ap1;
	if (p1->op!=p2->op || p1->subop!=p2->subop)
		return(0);
	if (p1->op>0 && p1->op<MOV || p1->op==0 || p1->op>=TEXT)
		return(0);
	cp1 = p1->code;
	cp2 = p2->code;
	if (cp1==0 && cp2==0)
		return(1);
	if (cp1==0 || cp2==0)
		return(0);
	return(strcmp(cp1, cp2)==0);
}

void
decref(struct node *p)
{
	if (--p->refc <= 0) {
		nrlab++;
		p->back->forw = p->forw;
		if (p->forw)
			p->forw->back = p->back;
	}
}

struct node *
nonlab(struct node *p)
{
	while (p && p->op==LABEL)
		p = p->forw;
	return(p);
}

void *
alloc(int n)
{
	register char *p;

	if ((p = calloc(1, n)) == NULL) {
		fprintf(stderr, "C Optimizer: out of space\n");
		exit(1);
	}
	return(p);
}

/*
 * Move data blocks to the beginning of the text block so the jump
 * optimizer sees straight-line code; the order within a section is
 * not significant.
 */
void
movedat(void)
{
	register struct node *p1, *p2;
	struct node *p3;
	register int seg;
	struct node data;
	struct node *datp;

	if (first.forw == 0)
		return;
	if (lastseg != TEXT && lastseg != -1 && lastsegcode) {
		p1 = alloc(sizeof(first));
		p1->op = lastseg;
		p1->subop = 0;
		p1->code = lastsegcode;
		p1->forw = first.forw;
		p1->back = &first;
		first.forw->back = p1;
		first.forw = p1;
	}
	memset(&data, 0, sizeof(data));
	datp = &data;
	for (p1 = first.forw; p1!=0; p1 = p1->forw) {
		if (p1->op == DATA || p1->op == BSS) {
			p2 = p1->forw;
			while (p2 && p2->op!=TEXT)
				p2 = p2->forw;
			if (p2==0)
				break;
			p3 = p1->back;
			p1->back->forw = p2->forw;
			if (p2->forw)
				p2->forw->back = p3;
			p2->forw = 0;
			datp->forw = p1;
			p1->back = datp;
			p1 = p3;
			datp = p2;
		}
	}
	if (data.forw) {
		datp->forw = first.forw;
		if (first.forw)
			first.forw->back = datp;
		data.forw->back = &first;
		first.forw = data.forw;
	}
	/* drop segment directives that change nothing */
	seg = lastseg;
	for (p1 = first.forw; p1!=0; p1 = p1->forw) {
		if (p1->op==TEXT) {
			if (p1->op == seg && p1->back!=&first) {
				p1->back->forw = p1->forw;
				if (p1->forw)
					p1->forw->back = p1->back;
				p1 = p1->back;
				continue;
			}
			seg = p1->op;
		} else if (p1->op==DATA || p1->op==BSS)
			seg = p1->op;
	}
}
