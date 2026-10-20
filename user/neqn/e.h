#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#define YYSTYPE intptr_t
#define getline eqn_getline
#define inline eqn_inline
void error(int,const char *,...);
#include <stdio.h>

#define	FATAL	1
#define	ROM	'1'
#define	ITAL	'1'
#define	BLD	'1'

#define	VERT(n)	(20 * (n))
#define	EFFPS(p)	((p) >= 6 ? (p) : 6)

extern int	dbg;
extern int	ct;
extern int	lp[];
extern int	used[];	/* available registers */
extern int	ps;	/* dflt init pt size */
extern int	deltaps;	/* default change in ps */
extern int	gsize;	/* global size */
extern int	gfont;	/* global font */
extern int	ft;	/* dflt font */
extern FILE	*curfile;	/* current input file */
extern int	ifile;	/* input file number */
extern int	linect;	/* line number in current file */
extern int	eqline;	/* line where eqn started */
extern int	svargc;
extern char	**svargv;
extern int	eht[];
extern int	ebase[];
extern int	lfont[];
extern int	rfont[];
extern intptr_t	yyval;
extern intptr_t	*yypv;
extern intptr_t	yylval;
extern int	eqnreg, eqnht;
extern int	lefteq, righteq;
extern int	lastchar;	/* last character read by lex */
extern int	markline;	/* 1 if this EQ/EN contains mark or lineup */

typedef struct s_tbl {
	char	*name;
	char	*defn;
	struct s_tbl *next;
} tbl;
