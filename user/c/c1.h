/*
 * C code generator header
 *
 * Native AMD64 retargeting of the supplied V7 c1.  The overlaid PDP-11
 * node shapes (tnode, tname, xtname, tconst, lconst, ftconst, fasgn)
 * become one explicit node so every member has its own storage.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cpass.h"

#define	LTYPE	int64_t

struct	tnode {
	int	op;
	int	type;
	int	degree;
	struct	tnode *tr1, *tr2;
	int	class;		/* names */
	int	regno;		/* base register of OFFS/XOFFS names */
	LTYPE	offset;
	int	nloc;		/* label, frame offset or register */
	char	*name;		/* external name */
	LTYPE	value;		/* constants; also narrowing target of ITOC */
	double	fvalue;
	LTYPE	mask;		/* field assignments; structure size */
	int	strclass;	/* System V class word of structure values */
};
#define	lvalue	value

struct	optab {
	char	tabdeg1;
	char	tabtyp1;
	char	tabdeg2;
	char	tabtyp2;
	char	*tabstring;
};

struct	table {
	int	tabop;
	struct	optab *tabp;
};

struct	instab {
	int	iop;
	char	*str1;
	char	*str2;
};

struct	swtab {
	int	swlab;
	LTYPE	swval;
};

extern char	maprel[];
extern char	notrel[];
extern int	nreg;
extern int	isn;
extern int	line;
extern int	nerror;
extern struct	table	cctab[];
extern struct	table	efftab[];
extern struct	table	regtab[];
extern struct	table	sptab[];
extern struct	table	lsptab[1];
extern struct	instab	instab[];
extern struct	instab	branchtab[];
extern int	opdope[];
extern char	*opntab[];
extern int	nstack;
extern int	nfloat;
extern struct	tnode	sfuncr;
extern char	*funcbase;
extern char	*curbase;
extern char	*coremax;
extern struct tnode czero, cone;
extern long	totspace;
extern char	filename[1024];
extern int	sfsize, sfclass, sfhidden, sftemp;

/*
	operators
*/
#define	EOFC	0
#define	SEMI	1
#define	LBRACE	2
#define	RBRACE	3
#define	LBRACK	4
#define	RBRACK	5
#define	LPARN	6
#define	RPARN	7
#define	COLON	8
#define	COMMA	9
#define	FSEL	10
#define	FSELR	11
#define	FSELT	12
#define	FSELA	16
#define	ULSH	17
#define	ASULSH	18

#define	KEYW	19
#define	NAME	20
#define	CON	21
#define	STRING	22
#define	FCON	23
#define	SFCON	24
#define	LCON	25
#define	SLCON	26

#define	AUTOI	27
#define	AUTOD	28
#define	INCBEF	30
#define	DECBEF	31
#define	INCAFT	32
#define	DECAFT	33
#define	EXCLA	34
#define	AMPER	35
#define	STAR	36
#define	NEG	37
#define	COMPL	38

#define	DOT	39
#define	PLUS	40
#define	MINUS	41
#define	TIMES	42
#define	DIVIDE	43
#define	MOD	44
#define	RSHIFT	45
#define	LSHIFT	46
#define	AND	47
#define	ANDN	55
#define	OR	48
#define	EXOR	49
#define	ARROW	50
#define	ITOF	51
#define	FTOI	52
#define	LOGAND	53
#define	LOGOR	54
#define	FTOL	56
#define	LTOF	57
#define	ITOL	58
#define	LTOI	59
#define	ITOP	13
#define	PTOI	14
#define	LTOP	15

#define	EQUAL	60
#define	NEQUAL	61
#define	LESSEQ	62
#define	LESS	63
#define	GREATEQ	64
#define	GREAT	65
#define	LESSEQP	66
#define	LESSP	67
#define	GREATQP	68
#define	GREATP	69

#define	ASPLUS	70
#define	ASMINUS	71
#define	ASTIMES	72
#define	ASDIV	73
#define	ASMOD	74
#define	ASRSH	75
#define	ASLSH	76
#define	ASAND	77
#define	ASOR	78
#define	ASXOR	79
#define	ASSIGN	80
#define	TAND	81
#define	LTIMES	82
#define	LDIV	83
#define	LMOD	84
#define	ASANDN	85
#define	LASTIMES 86
#define	LASDIV	87
#define	LASMOD	88

#define	QUEST	90
#define	MAX	93
#define	MAXP	94
#define	MIN	95
#define	MINP	96
#define	LLSHIFT	91
#define	ASLSHL	92
#define	SEQNC	97
#define	CALL1	98
#define	CALL2	99
#define	CALL	100
#define	MCALL	101
#define	JUMP	102
#define	CBRANCH	103
#define	INIT	104
#define	SETREG	105
#define	LOAD	106
#define	ITOC	109
#define	RFORCE	110

/*
 * Intermediate code operators
 */
#define	BRANCH	111
#define	LABEL	112
#define	NLABEL	113
#define	RLABEL	114
#define	STRASG	115
#define	STRSET	116
#define	BDATA	200
#define	PROG	202
#define	DATA	203
#define	BSS	204
#define	CSPACE	205
#define	SSPACE	206
#define	SYMDEF	207
#define	SAVE	208
#define	RETRN	209
#define	EVEN	210
#define	PROFIL	212
#define	SWIT	213
#define	EXPR	214
#define	SNAME	215
#define	RNAME	216
#define	ANAME	217
#define	NULLOP	218
#define	SETSTK	219
#define	SINIT	220
#define	GLOBAL	221
#define	C3BRANCH	222

/*
 *	types (shared with c0)
 */
#define	INT	0
#define	CHAR	1
#define	FLOAT	2
#define	DOUBLE	3
#define	STRUCT	4
#define	RSTRUCT	5
#define	LONG	6
#define	UNSIGN	7
#define	UCHAR	9
#define	SHORT	10
#define	USHORT	11
#define	ULONG	12
#define	VOID	13

#define	TYLEN	2
#define	TYPE	017
#define	XTYPE	(03<<4)
#define	PTR	020
#define	FUNC	040
#define	ARRAY	060

/*
	storage	classes
*/
#define	KEYWC	1
#define	MOS	10
#define	AUTO	11
#define	EXTERN	12
#define	STATIC	13
#define	REG	14
#define	STRTAG	15
#define	ARG	16
#define	OFFS	20
#define	XOFFS	21
#define	SOFFS	22

/*
	Flag	bits
*/

#define	BINARY	01
#define	LVALUE	02
#define	RELAT	04
#define	ASSGOP	010
#define	LWORD	020
#define	RWORD	040
#define	COMMUTE	0100
#define	RASSOC	0200
#define	LEAF	0400
#define	CNVRT	01000

/*
 * Native machine registers.  Scratch values use 0..NSCRATCH-1; c0's
 * register variables 0..4 live in the callee-saved registers.
 */
#define	NSCRATCH	8
#define	RREGVAR		8	/* rbx, r12-r15 */
#define	RFRAME		13	/* rbp */
#define	RTEMP		14	/* r11: template temporary */
extern char	*regname[4][15];

/*
 * functions
 */
struct	tnode *tnode(int, int, struct tnode *, struct tnode *);
struct	tnode *tconst(LTYPE, int);
struct	tnode *optim(struct tnode *);
struct	tnode *unoptim(struct tnode *);
struct	tnode *lvfield(struct tnode *);
struct	tnode *acommute(struct tnode *);
struct	tnode *lconst(int, struct tnode *, struct tnode *);
struct	tnode *isconstant(struct tnode *);
struct	tnode *hardlongs(struct tnode *);
struct	tnode *pow2(struct tnode *);
struct	tnode *strfunc(struct tnode *);
struct	tnode *ncopy(struct tnode *);
void	*getblk(int);
struct	optab *match(struct tnode *, struct table *, int, int);
int	rcexpr(struct tnode *, struct table *, int);
int	cexpr(struct tnode *, struct table *, int);
int	reorder(struct tnode **, struct table *, int);
int	sreorder(struct tnode **, struct table *, int, int);
int	delay(struct tnode **, struct table *, int);
struct	tnode *sdelay(struct tnode **);
int	chkleaf(struct tnode *, struct table *, int);
int	comarg(struct tnode *, int *);
void	doinit(int, struct tnode *);
void	movreg(int, int, struct tnode *);
int	max(int, int);
int	degree(struct tnode *);
void	pname(struct tnode *, int);
void	regerr(void);
void	pbase(struct tnode *);
int	xdcalc(struct tnode *, int);
int	dcalc(struct tnode *, int);
int	notcompat(struct tnode *, int, int);
void	prins(int, int, struct instab *, struct tnode *);
int	collcon(struct tnode *);
int	isfloat(struct tnode *);
int	isfvalue(struct tnode *);
int	oddreg(struct tnode *, int);
int	arlength(int);
void	pswitch(struct swtab *, struct swtab *, int);
void	breq(LTYPE, int);
int	sort(struct swtab *, struct swtab *);
int	ispow2(struct tnode *);
void	cbranch(struct tnode *, int, int, int);
void	branch(int, int, int);
void	label(int);
void	popstk(int);
void	error(const char *, ...);
void	psoct(LTYPE);
void	getree(void);
LTYPE	geti(void);
char	*outname(void);
void	strasg(struct tnode *);
void	setype(struct tnode *, int);
int	decref(int);
int	incref(int);
void	distrib(void *);
void	squash(struct tnode **, struct tnode **);
void	fconst(int, struct tnode *, LTYPE, int);
void	insert(int, struct tnode *, void *);
int	islong(int);
int	isunsigned(int);
int	isnarrow(int);
int	tsize(int);
int	regwidth(int);
void	callexpr(struct tnode *, int);
void	native(int, struct tnode *, struct tnode *, struct tnode *, int, int);
