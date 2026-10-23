#
/*
 * sed -- stream  editor
 *
 *
 */

#define CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	CNL	8
#define	CDOL	10
#define	CEOF	11
#define CKET	12
#define CNULL	13
#define CLNUM	14
#define CEND	16
#define CDONT	17
#define	CBACK	18

#define	STAR	01

#define NLINES	256
#define	DEPTH	20
#define	CCLSIZE	32	/* bytes in a 256-bit class (V7: 16 for 7-bit) */
#define	YSIZE	256	/* y table entries (V7: 128) */
#define PTRSIZE	100
#define RESIZE	5000
#define	ABUFSIZE	100
/* Native sizes: V7 used 4000-byte lines and 20 pending appends. */
#define	LBSIZE	8192
#define	ESIZE	256
#define	LABSIZE	50
#define NBRA	9

FILE	*fin;
union reptr	*abuf[ABUFSIZE];
union reptr **aptr;
char	*lastre;
char	ibuf[512];
char	*cbp;
char	*ebp;
char	genbuf[LBSIZE+1];
char	*loc1;
char	*loc2;
char	*locs;
char	seof;
char	*reend;
char	*lbend;
char	*hend;
char	*lcomend;
union reptr	*ptrend;
int	eflag;
int	dolflag;
int	sflag;
int	jflag;
int	numbra;
int	delflag;
long	lnum;
char	linebuf[LBSIZE+1];
char	holdsp[LBSIZE+1];
char	*spend;
char	*hspend;
int	nflag;
int	gflag;
char	*braelist[NBRA];
char	*braslist[NBRA];
long	tlno[NLINES];
int	nlno;
char	fname[12][LBSIZE+1];	/* V7 overran 40-byte names */
FILE	*fcode[12];
int	nfiles;

#define ACOM	01
#define BCOM	020
#define CCOM	02
#define	CDCOM	025
#define	CNCOM	022
#define COCOM	017
#define	CPCOM	023
#define DCOM	03
#define ECOM	015
#define EQCOM	013
#define FCOM	016
#define GCOM	027
#define CGCOM	030
#define HCOM	031
#define CHCOM	032
#define ICOM	04
#define LCOM	05
#define NCOM	012
#define PCOM	010
#define QCOM	011
#define RCOM	06
#define SCOM	07
#define TCOM	021
#define WCOM	014
#define	CWCOM	024
#define	YCOM	026
#define XCOM	033

char	*cp;
char	*reend;
char	*lbend;

/*
 * V7 overlaid two untagged member structures, relying on the PDP-11
 * compiler merging their names; re1 and lb1 share storage here instead.
 */
union	reptr {
	struct {
		char	*ad1;
		char	*ad2;
		union {
			char	*re1;
			union reptr	*lb1;
		};
		char	*rhs;
		FILE	*fcode;
		char	command;
		char	gfl;
		char	pfl;
		char	inar;
		char	negfl;
	};
} ptrspace[PTRSIZE], *rep;


/* Slack for one more compiled item before the reend check rejects it. */
char	respace[RESIZE + LBSIZE + ESIZE + YSIZE + 64];

struct label {
	char	asc[9];
	union reptr	*chain;
	union reptr	*address;
} ltab[LABSIZE];

struct label	*lab;
struct label	*labend;

int	f;
int	status;	/* 2 after an unopenable input or a write error */
int	depth;

int	eargc;
char	**eargv;

extern	unsigned char	bittab[];

union reptr	**cmpend[DEPTH];
int	depth;
union reptr	*pending;
char	*badp;
char	bad;
char	*compile();
char	*ycomp();
char	*address();
char	*text();
char	*compsub();
struct label	*search();
char	*gline();
char	*place();
char	*place1();
int	truncated;	/* replacement already cut at the line size */
char	compfl;
