/*
 * lint2: the second lint pass.
 *
 * Native retargeting of the supplied lpass2.c.  Type words are 64-bit
 * and names are significant to their full length.  The supplied driver
 * piped the first pass output through sort -u; this pass sorts and
 * removes duplicate lines itself (byte order, as sort -u), so it reads
 * either the raw or the sorted file.  Malformed lines are diagnosed.
 */
# include "lmanifest"
# include "../mip/manifest.h"
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <stdint.h>

typedef uint64_t TWORD;

# define USED 01
# define VUSED 02
# define EUSED 04
# define RVAL 010
# define VARARGS 0100

# define NAMESZ 256
# define MAXARGS 50

typedef struct { TWORD aty; int extra; } atype;

struct line {
	char name[NAMESZ];
	int decflag;
	atype type;
	int nargs;
	atype atyp[MAXARGS];
	int fline;
	char file[NAMESZ];
	}

	l1,
	l2,
	*pd,	/* pointer to line having definition */
	*pc,	/* pointer to current line read */
	*p3;	/* used for swapping pc and pd */

int uses = USED;
int hflag = 0;
int pflag = 0;
int xflag = 0;
int uflag = 1;

static char **lines;	/* the sorted, unique input */
static size_t nlines, curline;
static char *lp;	/* read position in the current line */

static void error();
static void cleanup();
static void readall();
static void lread();
static void chkcompat();
static void lastone();
static void setuse();
static void viceversa();
static int chktype();

int
main( argc, argv ) char *argv[]; {

	register char *p;

	/* first argument is - options */

	if( argc>=2 && argv[1][0] == '-' ){
		for( p=argv[1]; *p; ++p ){
			switch( *p ){

			case 'h':
				hflag = 1;
				break;

			case 'p':
				pflag = 1;
				break;

			case 'x':
				xflag = 1;
				break;

			case 'u':
				uflag = 0;
				break;

				}
			}
		}

	readall();

	pd = &l1;
	pc = &l2;
	pd->name[0] = '\0' ;
	pd->fline = 0;
	pd->file[0] = '\0';
	pd->decflag = LDI;

	/* main loop: read a line;
		if same as last line, check compatibility
		if not same as last line, becomes df.
		*/

	for(;;){
		lread();
		if( strcmp(pc->name, pd->name) == 0 ) chkcompat();
		else {
			lastone();
			setuse();
			p3=pc;
			pc = pd;
			pd = p3;
			}
		}

	}

static int
lcmp( a, b ) const void *a, *b; {
	return( strcmp( *(char *const *)a, *(char *const *)b ) );
	}

static void
readall(){ /* read stdin, then sort -u it */
	static char buf[4096];
	size_t n, cap, i, j;
	char *s;

	cap = 0;
	while( fgets( buf, sizeof(buf), stdin ) ){
		n = strlen( buf );
		if( n == 0 || buf[n-1] != '\n' ) error( "line too long or unterminated" );
		buf[n-1] = '\0';
		if( nlines == cap ){
			cap = cap ? 2*cap : 256;
			if( !(lines = realloc( lines, cap*sizeof(*lines) )) ) error( "out of memory" );
			}
		if( !(s = malloc( n )) ) error( "out of memory" );
		memcpy( s, buf, n );
		lines[nlines++] = s;
		}
	if( ferror( stdin ) ) error( "read error" );
	if( nlines ) qsort( lines, nlines, sizeof(*lines), lcmp );
	for( i=j=0; i<nlines; ++i ){
		if( j && strcmp( lines[j-1], lines[i] ) == 0 ) free( lines[i] );
		else lines[j++] = lines[i];
		}
	nlines = j;
	}

static int
getch(){
	return( *lp ? (unsigned char)*lp++ : '\n' );
	}

static void
getnam(p) char *p; {
	register c, n;
	n = 0;
	while( (c=getch()) != '\t' ){
		if( c == '\n' ) error( "rotten name" );
		if( ++n >= NAMESZ ) error( "name too long" );
		*p++ = c;
		}
	*p = '\0';
	}

static int
rdin10(){
	register val, c, s;

	val = 0;
	s = 1;

	while( (c=getch()) != '\t' ){
		if( c == '\n' ) error( "unexpected end of line" );
		else if( c == '-' ) {
			s = -1;
			continue;
			}
		else if( c<'0' || c>'9' ) {
			error("rotten digit: %o", c );
			}
		if( val > 100000000 ) error( "number too large" );
		val = val*10 + c - '0';
		}
	return( val*s );
	}

static void
rdinty( p ) atype *p; {
	register c;
	TWORD val;

	val = 0;

	while( (c=getch()) != '\t' && c!= '<' ){
		if( c == '\n' ) error( "unexpected end of line" );
		else if( c<'0' || c>'7' ) {
			error("rotten digit: %o", c );
			}
		val = (val<<3) + c - '0';
		}
	p->aty = val;
	if( c == '<' ) p->extra = rdin10();
	else p->extra = 0;
	}

static void
lread(){ /* read a line into pc */

	register i, n;

	if( curline >= nlines ) cleanup();
	lp = lines[curline++];
	getnam( pc->name );

	pc->decflag = rdin10();
	rdinty( &pc->type );
	n = pc->nargs = rdin10();
	if( n<0 ) n = -n;
	if( n > MAXARGS ) error( "too many arguments" );

	for( i=0; i<n; ++i ){
		rdinty( &pc->atyp[i] );
		}

	getnam( pc->file );
	pc->fline = rdin10();
	}

/* VARARGS */
static void
error( s, a ) char *s; {

	fflush( stdout );
	fprintf( stderr, "pass 2 error: " );
	fprintf( stderr, s, a );
	fprintf( stderr, "\n" );
	exit(1);
	}

static void
chkcompat(){
	/* are the types, etc. in pc and pd compatible */
	register int i;

	setuse();

	/* argument check */

	if( pd->decflag & (LDI|LIB|LUV|LUE) ){
		if( pc->decflag & (LUV|LIB|LUE) ){
			if( pd->nargs != pc->nargs ){
				if( !(uses&VARARGS) ){
					printf( "%s: variable # of args.", pd->name );
					viceversa();
					}
				if( pc->nargs > pd->nargs ) pc->nargs = pd->nargs;
				if( !(pd->decflag & (LDI|LIB) ) ) {
					pd->nargs = pc->nargs;
					uses |= VARARGS;
					}
				}
			for( i=0; i<pc->nargs; ++i ){
				if( chktype(&pd->atyp[i], &pc->atyp[i]) ){
					printf( "%s, arg. %d used inconsistently",
						pd->name, i+1 );
					viceversa();
					}
				}
			}
		}

	if( (pd->decflag&(LDI|LIB|LUV)) && pc->decflag==LUV ){
		if( chktype( &pc->type, &pd->type ) ){
			printf( "%s value used inconsistently", pd->name );
			viceversa();
			}
		}

	/* check for multiple declaration */

	if( (pd->decflag&LDI) && (pc->decflag&(LDI|LIB)) ){
		printf( "%s multiply declared", pd->name );
		viceversa();
		}

	/* do a bit of checking of definitions and uses... */

	if( (pd->decflag & (LDI|LIB|LDX|LDC)) && (pc->decflag & (LDX|LDC)) && pd->type.aty != pc->type.aty ){
		printf( "%s value declared inconsistently", pd->name );
		viceversa();
		}

	/* better not call functions which are declared to be structure or union returning */

	if( (pd->decflag & (LDI|LIB|LDX|LDC)) && (pc->decflag & LUE) && pd->type.aty != pc->type.aty ){
		/* only matters if the function returns union or structure */
		TWORD ty;
		ty = pd->type.aty;
		if( ISFTN(ty) && ((ty = DECREF(ty))==STRTY || ty==UNIONTY ) ){
			printf( "%s function value type must be declared before use", pd->name );
			viceversa();
			}
		}

	if( pflag && pd->decflag==LDX && pc->decflag == LUM && !ISFTN(pd->type.aty) ){
		/* make the external declaration go away */
		/* in effect, it was used without being defined */

		/* swap pc and pd */
		p3 = pc;
		pc = pd;
		pd = p3;
		}

	}

static void
viceversa(){
	/* print out file comparison */
	printf( "	%s(%d)  ::  %s(%d)\n", pd->file, pd->fline, pc->file, pc->fline );
	}

	/* messages for defintion/use */
char *
mess[2][2] = {
	"",
	"%s used( %s(%d) ), but not defined\n",
	"%s defined( %s(%d) ), but never used\n",
	"%s declared( %s(%d) ), but never used or defined\n"
	};

static void
lastone(){

	/* called when pc and pd are at last different */
	register nu, nd;

	nu = nd = 0;

	if( !(uses&USED) && pd->decflag != LIB ) {
		if( strcmp(pd->name,"main") )
			nu = 1;
		}

	if( !ISFTN(pd->type.aty) ){
		switch( pd->decflag ){

		case LIB:
			nu = nd = 0;  /* don't complain about uses on libraries */
			break;
		case LDX:
			if( !xflag ) break;
		case LUV:
		case LUE:
		case LUM:
			nd = 1;
			}
		}

	if( uflag && ( nu || nd ) ) printf( mess[nu][nd], pd->name, pd->file, pd->fline );

	if( (uses&(RVAL+EUSED)) == (RVAL+EUSED) ){
		printf( "%s returns value which is %s ignored\n", pd->name,
			uses&VUSED ? "sometimes" : "always" );
		}

	if( (uses&(RVAL+VUSED)) == (VUSED) && (pd->decflag&(LDI|LIB)) ){
		printf( "%s value is used, but none returned\n", pd->name );
		}

	/* clean up pc, in preparation for the next thing */

	uses = 0;
	if( pc->nargs < 0 ){
		pc->nargs = -pc->nargs;
		uses = VARARGS;
		}

	}

static void
cleanup(){ /* call lastone and die gracefully */
	lastone();
	if( fflush( stdout ) ) exit( 1 );
	exit(0);
	}

static void
setuse(){ /* check new type to ensure that it is used */

	switch( pc->decflag ){

	case LRV:
		uses |= RVAL;
		return;
	case LUV:
		uses |= VUSED+USED;
		return;
	case LUE:
		uses |= EUSED+USED;
		return;
	case LUM:
		uses |= USED;
		return;

		}
	}

static int
chktype( pt1, pt2 ) register atype *pt1, *pt2; {

	/* check the two type words to see if they are compatible */
	/* for the moment, enums are turned into ints, and should be checked as such */
	if( pt1->aty == ENUMTY ) pt1->aty =  INT;
	if( pt2->aty == ENUMTY ) pt2->aty = INT;

	if( pt2->extra ){ /* constant passed in */
		if( pt1->aty == UNSIGNED && pt2->aty == INT ) return( 0 );
		else if( pt1->aty == ULONG && pt2->aty == LONG ) return( 0 );
		}
	else if( pt1->extra ){ /* for symmetry */
		if( pt2->aty == UNSIGNED && pt1->aty == INT ) return( 0 );
		else if( pt2->aty == ULONG && pt1->aty == LONG ) return( 0 );
		}

	return( pt1->aty != pt2->aty );
	}
