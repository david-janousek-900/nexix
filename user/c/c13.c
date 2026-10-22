/*
 * C second pass -- tables
 *
 * Operator dope and names as supplied; the instruction and branch
 * tables name AMD64 mnemonics (integer and floating forms).
 */
#include "c1.h"
/*
 * Operator dope table-- see description in c0.
 */
int opdope[256] = {
	000000,	/* EOFC */
	000000,	/* ; */
	000000,	/* { */
	000000,	/* } */
	036000,	/* [ */
	002000,	/* ] */
	036000,	/* ( */
	002000,	/* ) */
	014201,	/* : */
	007001,	/* , */
	000000,	/* field selection */
	000000,	/* reverse field selection */
	000001,	/* temporary field selection */
	000001,	/* int->ptr */
	000001,	/* ptr->int */
	000001,	/* long->ptr */
	000001,	/* field assignment */
	000001,	/* >> unsigned */
	000001,	/* >> unsigned */
	000000,	/* 19 */
	000400,	/* name */
	000400,	/* short constant */
	000400,	/* string */
	000400,	/* float */
	000400,	/* double */
	0000400,	/* long const */
	000400,	/* long const <= 16 bits */
	000400,	/* autoi, *r++ */
	000400,	/* autod, *--r */
	000000,	/* 29 */
	034203,	/* ++pre */
	034203,	/* --pre */
	034203,	/* ++post */
	034203,	/* --post */
	034220,	/* !un */
	034202,	/* &un */
	034220,	/* *un */
	034200,	/* -un */
	034220,	/* ~un */
	036001,	/* . (structure reference) */
	030101,	/* + */
	030001,	/* - */
	032101,	/* * */
	032001,	/* / */
	032001,	/* % */
	026061,	/* >> */
	026061,	/* << */
	020161,	/* & */
	016161,	/* | */
	016161,	/* ^ */
	036001,	/* -> */
	001000,	/* int -> double */
	001000,	/* double -> int */
	000001,	/* && */
	000001,	/* || */
	030001, /* &~ */
	001000,	/* double -> long */
	001000,	/* long -> double */
	001000,	/* integer -> long */
	000000,	/* long -> integer */
	022005,	/* == */
	022005,	/* != */
	024005,	/* <= */
	024005,	/* < */
	024005,	/* >= */
	024005,	/* > */
	024005,	/* <p */
	024005,	/* <=p */
	024005,	/* >p */
	024005,	/* >=p */
	012213,	/* =+ */
	012213,	/* =- */
	012213,	/* =* */
	012213,	/* =/ */
	012213,	/* =% */
	012253,	/* =>> */
	012253,	/* =<< */
	012253,	/* =& */
	012253,	/* =| */
	012253,	/* =^ */
	012213,	/* = */
	030001, /* & for tests */
	032001,	/*  * (long) */
	032001,	/*  / (long) */
	032001,	/* % (long) */
	012253,	/* =& ~ */
	012213,	/* =* (long) */
	012213,	/* / (long) */
	012213,	/* % (long) */
	000000,	/* 89 */
	014201,	/* ? */
	026061,	/* long << */
	012253,	/* long =<< */
	000101,	/* max */
	000101,	/* maxp */
	000101,	/* min */
	000101,	/* minp */
	000001,	/* , */
	000000,	/* 98 */
	000000,	/* 99 */
	036001,	/* call */
	036000,	/* mcall */
	000000,	/* goto */
	000000,	/* jump cond */
	000000,	/* branch cond */
	000400,	/* set nregs */
	000000, /* 106 */
	000000,	/* 107 */
	000000,	/* 108 */
	000000,	/* int->char */
	000000,	/* force r0 */
	000000,	/* 111 */
	000000,	/* 112 */
	000000,	/* 113 */
	000000,	/* 114 */
	000000,	/* structure assign */
	000001,	/* struct assignment setup */
};

char	*opntab[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	":",
	",",
	"field select",
	0,
	0,
	"int->ptr",
	"ptr->int",
	"long->ptr",
	"field assign",
	">>",
	">>",
	0,
	"name",
	"short constant",
	"string",
	"float",
	"double",
	"long constant",
	"long constant",
	"*r++",
	"*--r",
	0,
	"++pre",
	"--pre",
	"++post",
	"--post",
	"!un",
	"&",
	"*",
	"-",
	"~",
	".",
	"+",
	"-",
	"*",
	"/",
	"%",
	">>",
	"<<",
	"&",
	"|",
	"^",
	"->",
	"int->double",
	"double->int",
	"&&",
	"||",
	"&~",
	"double->long",
	"long->double",
	"integer->long",
	"long->integer",
	"==",
	"!=",
	"<=",
	"<",
	">=",
	">",
	"<p",
	"<=p",
	">p",
	">=p",
	"+=",
	"-=",
	"*=",
	"/=",
	"%=",
	">>=",
	"<<=",
	"&=",
	"|=",
	"^=",
	"=",
	"& for tests",
	"*",
	"/",
	"%",
	"&= ~",
	"*=",
	"/=",
	"%=",
	0,
	"?",
	"<<",
	"<<=",
	"\\/",
	"\\/",
	"/\\",
	"/\\",
	0,
	"call",
	"call",
	"call",
	0,
	"goto",
	"jump cond",
	"branch cond",
	"set nregs",
	"load value",
	0,
	0,
	"int->char",
	"force register",
	"",
	"",
	"",
	"",
	"=structure",
	"= (struct setup)",
};

/*
 * Instruction tables, accessed by
 * I (integer form) or for floating trees the second form.
 */

struct instab instab[] = {
	{LOAD,	"mov",	"mov"},
	{ASSIGN,"mov",	"mov"},
	{EQUAL,	"cmp",	"ucomi"},
	{NEQUAL,"cmp",	"ucomi"},
	{LESSEQ,"cmp",	"ucomi"},
	{LESS,	"cmp",	"ucomi"},
	{GREATEQ,"cmp",	"ucomi"},
	{GREAT,	"cmp",	"ucomi"},
	{LESSEQP,"cmp",	"ucomi"},
	{LESSP,	"cmp",	"ucomi"},
	{GREATQP,"cmp",	"ucomi"},
	{GREATP,"cmp",	"ucomi"},
	{PLUS,	"add",	"add"},
	{ASPLUS,"add",	"add"},
	{MINUS,	"sub",	"sub"},
	{ASMINUS,"sub",	"sub"},
	{INCBEF,"add",	"add"},
	{DECBEF,"sub",	"sub"},
	{INCAFT,"add",	"add"},
	{DECAFT,"sub",	"sub"},
	{TIMES,	"imul",	"mul"},
	{ASTIMES,"imul","mul"},
	{DIVIDE,"idiv",	"div"},
	{ASDIV,	"idiv",	"div"},
	{MOD,	"idiv",	"div"},
	{ASMOD,	"idiv",	"div"},
	{PTOI,	"idiv",	"div"},
	{LSHIFT,"shl",	"shl"},
	{ASLSH,	"shl",	"shl"},
	{ULSH,	"shr",	"shr"},
	{ASULSH,"shr",	"shr"},
	{AND,	"and",	"and"},
	{ASAND,	"and",	"and"},
	{TAND,	"test",	"test"},
	{OR,	"or",	"or"},
	{ASOR,	"or",	"or"},
	{EXOR,	"xor",	"xor"},
	{ASXOR,	"xor",	"xor"},
	{NEG,	"neg",	"neg"},
	{COMPL,	"not",	"not"},
	{0,	0,	0}};

/*
 * Similar table for relationals.
 * The first string is for the positive
 * test, the second for the inverted one.
 * The '200+' entries are 
 * used in tests against 0, where the carry flag is clear,
 * so unsigned tests reduce to equality (or never/always;
 * a null entry means no branch).
 */
struct instab branchtab[] = {
	{EQUAL,	"je",	"jne"},
	{NEQUAL,"jne",	"je"},
	{LESSEQ,"jle",	"jg"},
	{LESS,	"jl",	"jge"},
	{GREATEQ,"jge",	"jl"},
	{GREAT,	"jg",	"jle"},
	{LESSEQP,"jbe",	"ja"},
	{LESSP,	"jb",	"jae"},
	{GREATQP,"jae",	"jb"},
	{GREATP,"ja",	"jbe"},
	{200+EQUAL,	"je",	"jne"},
	{200+NEQUAL,	"jne",	"je"},
	{200+LESSEQ,	"jle",	"jg"},
	{200+LESS,	"jl",	"jge"},
	{200+GREATEQ,	"jge",	"jl"},
	{200+GREAT,	"jg",	"jle"},
	{200+LESSEQP,	"je",	"jne"},
	{200+LESSP,	0,	"jmp"},
	{200+GREATQP,	"jmp",	0},
	{200+GREATP,	"jne",	"je"},
	{0,	0,	0}};

/* Pass state; the V7 header declared these as common definitions. */
int	line;
int	nerror;
int	nstack;
int	nfloat;
char	*funcbase;
char	*curbase;
char	*coremax;
long	totspace;
char	filename[1024];
int	sfsize, sfclass, sfhidden, sftemp;
