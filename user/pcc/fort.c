/*
 * fort: the portable compiler's second pass for the Fortran front end.
 *
 * Native retargeting of the supplied mip/fort.c.  It reads the binary
 * postfix intermediate file of f77's SCJ interface and generates code
 * with the native PCC backend.  Native format (version 1), all 64-bit
 * little-endian words:
 *
 *	header	op | val<<8 | rest<<16	(rest: type, line, label or ftnno)
 *	FTEXT	val words of NUL-padded assembler text follow
 *	FEXPR	rest = line; val words of file name follow; compiles the stack
 *	FLBRAC	one word: automatic area size in bytes; rest = function number
 *	FRBRAC	end of a function: emit its frame size
 *	FEOF	end of input
 *	LABEL	val = 0: label number rest
 *	GOTO	val = 1: jump to label rest; otherwise computed (unary)
 *	ICON	value word, then (val != 0) a counted name
 *	NAME	(val&1) offset word, then a counted name
 *	OREG	register val (0 automatic, 1 argument area), offset word, counted name
 *	REG	register val (as for OREG): the frame address
 *	other	operators of the shared manifest with type rest
 *
 * A counted name is a byte-length word and that many bytes padded to
 * whole words.  FSWITCH is not implemented, as in the supplied pass.
 */
#include "backend.h"
#undef printf
#undef puts
#undef putchar

#define	FTEXT	200
#define	FEXPR	201
#define	FSWITCH	202
#define	FLBRAC	203
#define	FRBRAC	204
#define	FEOF	205
#define	FARIF	206
#define	FLABEL	207
#define	NSTACKSZ 250
#define	NNODES	256	/* one Fortran expression */

static FILE *lrd;
static int nnodes;	/* ftnno is the front end's function counter */
static uint64_t baseoff, maxoff;
static struct mip_node nodes[NNODES];
static uint32_t fstack[NSTACKSZ], *fsp;
static char filename[256];

static uint64_t
lread(void)
{
	unsigned char b[8];
	uint64_t x;
	int i;

	if (fread(b, 1, 8, lrd) != 8)
		cerror("intermediate file read error");
	for (x = 0, i = 7; i >= 0; i--)
		x = x<<8 | b[i];
	return(x);
}

/* read n words of NUL-padded bytes into s (size bytes) */
static void
lcread(char *s, size_t size, unsigned n)
{
	unsigned i;
	size_t k;
	uint64_t w;
	int j;

	for (k = i = 0; i < n; i++) {
		w = lread();
		for (j = 0; j < 8; j++, w >>= 8)
			if (k < size-1)
				s[k++] = (char)(w&0377);
	}
	s[k < size ? k : size-1] = 0;
}

static void
lname(char *s, size_t size)
{
	uint64_t n = lread();

	if (n >= size)
		cerror("intermediate name too long");
	lcread(s, size, (unsigned)((n+7)/8));
	s[n] = 0;
}

static void
lccopy(unsigned n)
{
	char buf[8*256+1];

	if (n > 256)
		cerror("lccopy asked to copy too much");
	lcread(buf, sizeof(buf), n);
	pcc_target_printf("%s\n", buf);
}

static uint32_t
push(struct mip_node *n)
{
	uint32_t id;
	struct mip_arena a;

	if (nnodes >= NNODES)
		cerror("expression too large");
	a.nodes = nodes;
	a.count = (uint32_t)nnodes;
	a.capacity = NNODES;
	if (mip_node_add(&a, n, &id))
		cerror("invalid intermediate node %u", n->op);
	nnodes = (int)a.count;
	if (fsp >= &fstack[NSTACKSZ])
		uerror("expression depth exceeded");
	else
		*fsp++ = id;
	return(id);
}

static uint32_t
pop(void)
{
	if (fsp == fstack)
		cerror("expression poorly formed");
	return(*--fsp);
}

/* the frame and argument registers address the native frame */
static void
frame(struct mip_node *n, unsigned reg)
{
	if (reg > 1)
		cerror("unsupported register %u", reg);
	n->op = NAME;
	n->reg = reg ? PARAM : AUTO;
}

int
main(int argc, char **argv)
{
	uint64_t x;
	unsigned op, val;
	uint64_t rest;
	struct mip_node n, m;

	lrd = stdin;
	if (argc > 1 && strcmp(argv[1], "-") && (lrd = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "fort: cannot open intermediate file %s\n", argv[1]);
		return(1);
	}
	if (argc > 2 && !freopen(argv[2], "w", stdout)) {
		fprintf(stderr, "fort: can't create %s\n", argv[2]);
		return(1);
	}
	fsp = fstack;
	pcc_target_puts(".text");
	for (;;) {
		x = lread();
		op = (unsigned)(x & 0377);
		val = (unsigned)(x>>8 & 0377);
		rest = x>>16;
		memset(&n, 0, sizeof(n));
		n.left = n.right = MIP_NO_NODE;
		switch (op) {

		case 0:
			fprintf(stderr, "null opcode ignored\n");
			continue;

		case FTEXT:
			lccopy(val);
			continue;

		case FLBRAC:
			baseoff = lread();
			if (ftnno != (int)rest) {
				/* beginning of function */
				maxoff = baseoff;
				ftnno = (int)rest;
			} else if (baseoff > maxoff)
				maxoff = baseoff;
			pcc_note_frame((unsigned)maxoff);
			continue;

		case FRBRAC:
			/* frame size of the function: a 16-byte aligned area */
			pcc_target_printf(".set .F%d,%lu\n", ftnno, (unsigned long)((maxoff+15) & ~UINT64_C(15)));
			continue;

		case FEOF:
			pcc_target_puts(".section .note.GNU-stack,\"\",@progbits");
			return(nerrors != 0 || fflush(stdout) != 0);

		case FSWITCH:
		case FARIF:
			uerror(op==FSWITCH ? "switch not yet done" : "arithmetic if not done in pass 2");
			for (; val > 0; --val)
				lread();
			continue;

		case ICON:
			n.op = ICON;
			n.type = rest;
			n.value = lread();
			if (val)
				lname(n.name, sizeof(n.name));
			push(&n);
			continue;

		case NAME:
			n.op = NAME;
			n.type = rest;
			n.reg = EXTERN;
			if (val&1)
				n.value = lread();
			lname(n.name, sizeof(n.name));
			push(&n);
			continue;

		case OREG:
			n.type = rest;
			frame(&n, val);
			n.offset = (int64_t)lread();
			lname(n.name, sizeof(n.name));
			n.name[0] = 0;
			push(&n);
			continue;

		case REG:
			/* the frame address: &(automatic at offset 0) */
			memset(&m, 0, sizeof(m));
			m.left = m.right = MIP_NO_NODE;
			m.type = DECREF(rest);
			frame(&m, val);
			n.op = UNARY AND;
			n.type = rest;
			n.left = push(&m);
			fsp--;
			push(&n);
			continue;

		case FEXPR:
			if (val) {
				if (val > 31)
					cerror("intermediate file name too long");
				lcread(filename, sizeof(filename), val);
			}
			if (fsp == fstack)
				continue;	/* filename only */
			if (--fsp != fstack)
				uerror("expression poorly formed");
			if (!nerrors) {
				struct mip_arena a = { nodes, (uint32_t)nnodes, NNODES };
				pcc_emit_expression(&a, fstack[0]);
			}
			nnodes = 0;
			fsp = fstack;
			continue;

		case FLABEL:
			if (val)
				cerror("named labels are not used by the native interface");
			pcc_target_printf(".L%lu:\n", (unsigned long)rest);
			continue;

		case GOTO:
			if (val) {
				pcc_target_printf(" jmp .L%lu\n", (unsigned long)rest);	/* unconditional branch */
				continue;
			}
			/* otherwise, treat as unary */
		default:
			n.op = op;
			n.type = rest;
			switch (mip_op_flags(op) & TYFLG) {

			case BITYPE:
				n.right = pop();
				n.left = pop();
				break;

			case UTYPE:
				n.left = pop();
				break;

			default:
				uerror("illegal leaf node: %u", op);
				return(1);
			}
			push(&n);
			continue;
		}
	}
}
