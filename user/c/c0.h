/*
 * C compiler-- first pass header
 *
 * Native LP64 retargeting of the supplied V7 c0.  Types keep the original
 * packed representation with a wider basic-type field; identifiers are exact.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cpass.h"

/*
 * parameters
 */

#define	LTYPE	int64_t	/* long constants */
#define	MAXINT	0x7fffffffL	/* Largest positive int */
#define	MAXUINT	0xffffffffUL	/* largest unsigned integer */
#define	NCPS	255	/* # significant chars per symbol */
#define	HSHSIZ	2003	/* # entries in hash table for names */
#define	CMSIZ	80	/* size of expression stack */
#define	SSIZE	60	/* size of other expression stack */
#define	SWSIZ	1024	/* size of switch table */
#define	NMEMS	512	/* Number of members in a structure */
#define	NBPW	32	/* bits per word, object machine */
#define	NBPC	8	/* bits per character, object machine */
#define	NCPW	4	/* chars per word, object machine */
#define	LNCPW	4	/* chars per int constant */
#define	STAUTO	(-40)	/* offset of first auto variable (below saved registers) */
#define	STARG	16	/* offset of first stack argument */

/*
 * # bytes in primitive types
 */
#define	SZCHAR	1
#define	SZSHORT	2
#define	SZINT	4
#define	SZPTR	8
#define	SZFLOAT	4
#define	SZLONG	8
#define	SZDOUB	8

/*
 * format of a structure description
 */
struct str {
	int	ssize;			/* structure size */
	int	salign;			/* native alignment */
	int	sid;			/* member name space */
	struct hshtab 	**memlist;	/* member list */
};

/*
 * For fields, strp points here instead.
 */
struct field {
	int	flen;		/* field width in bits */
	int	bitoffs;	/* shift count */
};

/*
 * One tree node shape replaces the PDP-11 overlaid tnode/cnode/lnode/fnode
 * structures; the leaf payload members are explicit.
 */
struct tnode {
	int	op;		/* operator */
	int	type;		/* data type */
	int	*subsp;		/* subscript list (for arrays) */
	struct	str *strp;	/* structure description for structs */
	struct	tnode *tr1;	/* left operand, or symbol for names */
	struct	tnode *tr2;	/* right operand */
	LTYPE	value;		/* constant value */
	char	*cstr;		/* floating constant text */
	struct	str *rstrp;	/* structure returned by a call */
};
#define	cnode	tnode
#define	lnode	tnode
#define	fnode	tnode
#define	lvalue	value

/*
 * Pushed-down entry for block structure and top-level namelist.
 */
struct hshtab {
	int	hclass;		/* storage class */
	int	hflag;		/* various flags */
	int	htype;		/* type */
	int	*hsubsp;	/* subscript list */
	struct	str *hstrp;	/* structure description */
	int	hoffset;	/* post-allocation location */
	struct	hshtab *hpdown;	/* Pushed-down name in outer block */
	struct	hshtab *hnext;	/* argument list link */
	int	*hproto;	/* prototype: count, variadic, parameter types */
	int	hblklev;	/* Block level of definition */
	char	*name;		/* ASCII name (pool); members carry @structure */
};
#define	phshtab	hshtab

/*
 * Place used to keep dimensions
 * during declarations
 */
struct	tdim {
	int	rank;
	int	dimens[16];
};

/*
 * Table for recording switches.
 */
struct swtab {
	int	swlab;
	LTYPE	swval;
};

extern char	cvtab[5][5];
extern char	filename[1024];
extern int	opdope[];
extern char	ctab[];
extern char	symbuf[NCPS+16];
extern int	hshused;
extern struct	hshtab	hshtab[HSHSIZ];
extern struct	tnode **cp;
extern struct	tnode *cmstack[];
extern int	isn;
extern struct	swtab	swtab[SWSIZ];
extern struct	swtab	*swp;
extern int	contlab;
extern int	brklab;
extern int	retlab;
extern int	deflab;
extern int	autolen;
extern int	maxauto;
extern int	peeksym;
extern int	peekc;
extern int	eof;
extern int	line;
extern char	*funcbase;
extern char	*curbase;
extern char	*coremax;
extern char	*maxdecl;
extern struct	hshtab	*defsym;
extern struct	hshtab	*funcsym;
extern int	proflg;
extern struct	hshtab	*csym;
extern LTYPE	cval;
extern LTYPE	lcval;
extern int	nchstr;
extern int	nerror;
extern struct	hshtab	*paraml;
extern struct	hshtab	*parame;
extern int	strflg;
extern int	mosflg;
extern int	initflg;
extern int	inhdr;
extern FILE	*sbufp;
extern int	regvar;
extern int	bitoffs;
extern struct	tnode	funcblk;
extern char	cvntab[];
extern char	numbuf[512];
extern struct	hshtab **memlist;
extern int	nmems;
extern struct	hshtab	structhole;
extern int	blklev;
extern int	mossym;
extern int	protolev;
extern int	variadic;
extern int	csymflag;
extern int	protorec;
extern int	protodef;
extern int	vasave, vagp, vafp, vaover;
extern int	stralign;
extern struct str vastr;
extern struct str *memqual, *curstr;
extern int memdecl, nsid, noinsert;
extern char nullname[];
char	*savename(const char *);

/*
  operators
*/
#define	EOFC	0
#define	NULLOP	218
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
#define	CAST	11
#define	ETYPE	12
#define	ELLIPS	16

#define	KEYW	19
#define	NAME	20
#define	CON	21
#define	STRING	22
#define	FCON	23
#define	SFCON	24
#define	LCON	25
#define	SLCON	26

#define	SIZEOF	91
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
#define	ASSAND	77
#define	ASOR	78
#define	ASXOR	79
#define	ASSIGN	80

#define	QUEST	90
#define	MAX	93
#define	MAXP	94
#define	MIN	95
#define	MINP	96
#define	SEQNC	97
#define	CALL	100
#define	MCALL	101
#define	JUMP	102
#define	CBRANCH	103
#define	INIT	104
#define	SETREG	105
#define	RFORCE	110
#define	BRANCH	111
#define	LABEL	112
#define	NLABEL	113
#define	RLABEL	114
#define	STRASG	115
#define	ITOC	109
#define	SEOF	200	/* stack EOF marker in expr compilation */

/*
  types
*/
#define	INT	0
#define	CHAR	1
#define	FLOAT	2
#define	DOUBLE	3
#define	STRUCT	4
#define	LONG	6
#define	UNSIGN	7
#define	UNION	8		/* adjusted later to struct */
#define	UCHAR	9
#define	SHORT	10
#define	USHORT	11
#define	ULONG	12
#define	VOID	13

#define	ALIGN	07		/* stack/argument slot alignment - 1 */
#define	TYPE	017
#define	BIGTYPE	(03<<28)
#define	TYLEN	2
#define	XTYPE	(03<<4)
#define	PTR	020
#define	FUNC	040
#define	ARRAY	060

/*
  storage classes
*/
#define	KEYWC	1
#define	DEFXTRN	20
#define	TYPEDEF	9
#define	MOS	10
#define	AUTO	11
#define	EXTERN	12
#define	STATIC	13
#define	REG	14
#define	STRTAG	15
#define ARG	16
#define	ARG1	17
#define	AREG	18
#define	MOU	21
#define	ENUMTAG	22
#define	ENUMCON	24

/*
  keywords
*/
#define	GOTO	20
#define	RETURN	21
#define	IF	22
#define	WHILE	23
#define	ELSE	24
#define	SWITCH	25
#define	CASE	26
#define	BREAK	27
#define	CONTIN	28
#define	DO	29
#define	DEFAULT	30
#define	FOR	31
#define	ENUM	32
/* native header profile keywords */
#define	QUALIF	33	/* const, volatile, restrict, inline: no code effect */
#define	SIGNED	34
#define	ATTRIB	35
#define	VALIST	36
#define	OFFSETOF	37
#define	SASSERT	38
#define	VASTART	39
#define	VAARG	40
#define	VAEND	41
#define	VACOPY	42
#define	SHORTKW	43
#define	VOIDKW	44

/*
  characters
*/
#define	BSLASH	117
#define	SHARP	118
#define	INSERT	119
#define	PERIOD	120
#define	SQUOTE	121
#define	DQUOTE	122
#define	LETTER	123
#define	DIGIT	124
#define	NEWLN	125
#define	SPACE	126
#define	UNKN	127

/*
 * Special operators in intermediate code
 */
#define	BDATA	200
#define	WDATA	201
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
#define	SETSTK	219
#define	SINIT	220

/*
  Flag bits
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

/*
 * Conversion codes
 */
#define	ITF	1
#define	ITL	2
#define	LTF	3
#define	ITP	4
#define	PTI	5
#define	FTI	6
#define	LTI	7
#define	FTL	8
#define	LTP	9
#define	ITC	10
#define	XX	15

/*
 * symbol table flags
 */

#define	FMOS	01
#define	FKEYW	04
#define	FFIELD	020
#define	FINIT	040
#define	FLABL	0100

/*
 * functions
 */
struct	tnode *tree(void);
char	*copnum(int);
struct	tnode *convert(struct tnode *, int, int, int);
struct	tnode *chkfun(struct tnode *);
struct	tnode *disarray(struct tnode *);
struct	tnode *block(int, int, int *, struct str *, struct tnode *, struct tnode *);
struct	tnode *cblock(LTYPE);
struct	tnode *fblock(int, char *);
void	*gblock(int);
struct	tnode *pexpr(void);
struct	str *strdec(int, int);
struct	tnode *xprtype(void);
struct	tnode *nblock(struct hshtab *);
void	error(const char *, ...);
void	outcode(const char *, ...);
int	symbol(void);

int	lookup(void);
int	findkw(void);
int	getnum(void);
int	subseq(int, int, int);
void	putstr(int, int);
int	getcc(void);
int	mapch(int);
void	build(int);
void	setype(struct tnode *, int, struct tnode *);
void	chkw(struct tnode *, int);
int	lintyp(int);
void	chklval(struct tnode *);
int	fold(int, struct tnode *, struct tnode *);
LTYPE	conexp(void);
void	extdef(void);
void	cfunc(void);
int	cinit(struct hshtab *, int, int);
void	strinit(struct hshtab *, int);
void	setinit(struct hshtab *);
void	statement(void);
int	forstmt(void);
void	pswitch(void);
void	funchead(void);
void	blockhead(void);
void	blkend(void);
void	prste(struct hshtab *);
void	errflush(int);
int	declist(int);
int	getkeywords(int *, struct hshtab *);
int	declare(int, struct hshtab *, int);
int	decl1(int, struct hshtab *, int, struct hshtab *);
void	pushdecl(struct hshtab *);
void	cpysymb(struct hshtab *, struct hshtab *);
int	getype(struct tdim *, struct hshtab *);
void	typov(void);
int	align(int, int, int, struct str *);
void	decsyn(int);
void	redec(void);
int	goodreg(struct hshtab *);
int	decref(int);
int	incref(int);
void	cbranch(struct tnode *, int, int);
void	rcexpr(struct tnode *);
void	treeout(struct tnode *, int);
void	branch(int);
void	label(int);
int	plength(struct tnode *);
int	length(struct tnode *);
int	rlength(struct tnode *);
int	slength(struct hshtab *);
int	srlength(struct hshtab *);
int	talign(int, struct str *);
int	simplegoto(void);
int	nextchar(void);
int	spnextchar(void);
void	chconbrk(int);
void	dogoto(void);
void	doret(void);
int	isunsigned(int);
int	isnarrow(int);
int	islongtype(int);
int	isfloattype(int);
int	isinttype(int);
struct	tnode *builtin(int);
void	skipattr(void);
int	protolist(struct hshtab *);
void	memalias(struct hshtab *);
int	strclass(struct str *);
int	*saveproto(int *, int);
struct	tnode *protocall(struct hshtab *, struct tnode *);
