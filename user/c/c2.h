/*
 * Header for object code improver
 *
 * Native AMD64 retargeting of the supplied V7 c2: the input is the
 * AT&T-syntax assembly written by c1 (and by the portable compiler).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	JBR	1	/* jmp label */
#define	CBR	2	/* conditional jump */
#define	JMP	3	/* indirect or non-label jmp */
#define	LABEL	4	/* .Ln: */
#define	DLABEL	5	/* other label */
#define	EROU	7	/* .globl: end of a routine block */
#define	JSW	9	/* .quad .Ln in a switch table */
#define	MOV	10
#define	CALL	11
#define	RET	12
#define	TEXT	30
#define	DATA	31
#define	BSS	32
#define	END	48

/* conditional jumps; revbr[] gives the inverse condition */
#define	JEQ	0
#define	JNE	1
#define	JLE	2
#define	JGE	3
#define	JLT	4
#define	JGT	5
#define	JLO	6
#define	JHI	7
#define	JLOS	8
#define	JHIS	9
#define	JP	10
#define	JNP	11
#define	JS	12
#define	JNS	13

#define	LSIZE	4096

struct node {
	int	op;
	int	subop;
	struct	node	*forw;
	struct	node	*back;
	struct	node	*ref;
	int	labno;
	char	*code;
	int	refc;
};

struct optab {
	char	*opstring;
	int	opcode;
};
extern struct optab optab[];

extern char	line[LSIZE];
extern struct	node	first;
extern char	*curlp;
extern int	nbrbr;
extern int	redunm;
extern int	iaftbr;
extern int	njp1;
extern int	nrlab;
extern int	nxjump;
extern int	ncmot;
extern int	nrevbr;
extern int	loopiv;
extern int	nredunj;
extern int	nskip;
extern int	ncomj;

extern int	nchange;
extern int	isn;
extern int	debug;
extern int	lastseg;
extern char	revbr[];

/* c2-created labels never collide with c0, c1 or the portable compiler */
#define	C2LABELS	1500000000
#define	LABHS	127

struct	node *nonlab(struct node *);
char	*copy(int, char *, char *);
struct	node *insertl(struct node *);
struct	node *codemove(struct node *);
void	*alloc(int);
int	input(void);
int	getline2(void);
int	getnum(char *);
void	output(void);
void	opsetup(void);
int	oplook(void);
void	refcount(void);
void	iterate(void);
void	xjump(struct node *);
void	comjump(void);
void	backjmp(struct node *, struct node *);
void	rmove(void);
int	jumpsw(void);
int	equop(struct node *, struct node *);
void	decref(struct node *);
void	clearreg(void);
void	movedat(void);
int	iabs(int);
