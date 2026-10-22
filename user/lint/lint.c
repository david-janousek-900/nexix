/*
 * lint1: the first lint pass over the native PCC front end.
 *
 * Native retargeting of the supplied lint.c.  The shared front end keeps
 * its LP64 sizes from macdefs; -p no longer switches to PDP-11 sizes but
 * still requests portability diagnostics and 6-character monocase
 * external names.  Names are significant to their full length, so the
 * supplied %.7s truncation of names and messages is gone.  Types are
 * 64-bit words written in octal; lint2 reads them back.  The native
 * clocal, offcon and cast are reused, with lint's conversion checks first.
 */
# include "mfile1"

# include "lmanifest"

# include <ctype.h>

# define VAL 0
# define EFF 1

int vflag = 1;  /* tell about unused argments */
int xflag = 0;  /* tell about unused externals */
int argflag = 0;  /* used to turn off complaints about arguments */
int libflag = 0;  /* used to generate library descriptions */
int vaflag = -1;  /* used to signal functions with a variable number of args */
int aflag = 0;  /* used th check precision of assignments */

char *flabel = "xxx";

NODE *pcc_native_clocal(NODE *);

# define LNAMES 100

struct lnm {
	short lid, flgs;
	}  lnames[LNAMES], *lnp;

static int labels = 10;

static void
pname( char *name, int flag, TWORD type, int nargs ){
	printf( "%s\t%03d\t%lo\t%d\t", exname(name), flag, (unsigned long)type, nargs );
	}

contx( p, down, pl, pr ) register NODE *p; register *pl, *pr; {

	*pl = *pr = VAL;
	switch( p->op ){

	case ANDAND:
	case OROR:
	case QUEST:
		*pr = down;
		break;

	case SCONV:
	case PCONV:
		if( p->type == VOID ) break;  /* (void) uses the value */
	case COLON:
		*pr = *pl = down;
		break;

	case COMOP:
		*pl = EFF;
		*pr = down;

	case FORCE:
	case INIT:
	case UNARY CALL:
	case STCALL:
	case UNARY STCALL:
	case CALL:
	case UNARY FORTCALL:
	case FORTCALL:
	case CBRANCH:
		break;

	default:
		if( asgop(p->op) ) break;
		if( p->op == UNARY MUL && ( p->type == STRTY || p->type == UNIONTY) ) {
			break;  /* the compiler does this... */
			}
		if( down == EFF && hflag ) werror( "null effect" );

		}
	return( 0 );
	}

int
ecode( p ) NODE *p; {
	/* compile code for p */

	fwalk( p, contx, EFF );
	lnp = lnames;
	lprt( p, EFF, 0 );
	return( 0 );
	}

int
ejobcode( flag ){
	/* called after processing each job */
	/* flag is nonzero if errors were detected */
	register k;
	register struct symtab *p;

	for( p=stab; p< &stab[SYMTSZ]; ++p ){

		if( p->stype != TNULL ) {

			if( p->stype == STRTY || p->stype == UNIONTY ){
				if( dimtab[p->sizoff+1] < 0 ){ /* never defined */
					if( hflag ) werror( "struct/union %s never defined", p->sname );
					}
				}

			switch( p->sclass ){

			case STATIC:
				if( p->suse > 0 ){
					k = lineno;
					lineno = p->suse;
					uerror( "static variable %s unused",
						p->sname );
					lineno = k;
					break;
					}

			case EXTERN:
			case USTATIC:
				/* with the xflag, worry about externs not used */
				/* the filename may be wrong here... */
				if( xflag && p->suse >= 0 && !libflag ){
					pname( p->sname, LDX, p->stype, 0 );
					/* we don't really know the file number; we know only the line
						number, so we put only that out */
					printf( "\"???\"\t%d\t%s\n", p->suse, flabel );
					}

			case EXTDEF:
				if( p->suse < 0 ){  /* used */
					pname( p->sname, LUM, p->stype, 0 );
					fident( -p->suse );
					}
				break;
				}

			}

		}
	if( fflush( stdout ) ) uerror( "lint output failed" );
	exit( 0 );
	}

fident( line ){ /* like ident, but lineno = line */
	register temp;
	temp = lineno;
	lineno = line;
	ident();
	lineno = temp;
	}

ident(){ /* write out file and line identification  */
	printf( "%s\t%d\t%s\n", ftitle[0] ? ftitle : "-", lineno, flabel );
	}

int
bfcode( a, n ) int a[]; {
	/* code for the beginning of a function; a is an array of
		indices in stab for the arguments; n is the number */
	/* this must also set retlab */
	register i;
	register struct symtab *cfp;
	TWORD t;

	retlab = 1;
	cfp = &stab[curftn];

	/* a prototype ending in , ... checks its named arguments, as VARARGS n */
	if( vaflag < 0 && pcc_definition_variadic() ) vaflag = n;

	/* if variable number of arguments, only print the ones which will be checked */
	if( vaflag > 0 ){
		if( n < vaflag ) werror( "declare the VARARGS arguments you want checked!" );
		else n = vaflag;
		}
	pname( cfp->sname, libflag?LIB:LDI, cfp->stype, vaflag>=0?-n:n );
	vaflag = -1;

	for( i=0; i<n; ++i ) {
		stab[a[i]].sclass = PARAM;
		switch( t = stab[a[i]].stype ){

		case ULONG:
			break;

		case CHAR:
		case SHORT:
			t = INT;
			break;

		case UCHAR:
		case USHORT:
		case UNSIGNED:
			t = UNSIGNED;
			break;

			}

		printf( "%lo\t", (unsigned long)t );
		}
	ident();
	return( 0 );
	}

ctargs( p ) NODE *p; {
	/* count arguments; p points to at least one */
	/* the arguemnts are a tower of commasto the left */
	register c;
	c = 1; /* count the rhs */
	while( p->op == CM ){
		++c;
		p = p->left;
		}
	return( c );
	}

lpta( p ) NODE *p; {
	TWORD t;

	if( p->op == CM ){
		lpta( p->left );
		p = p->right;
		}
	switch( t = p->type ){

		case CHAR:
		case SHORT:
			t = INT;
		case LONG:
		case ULONG:
		case INT:
		case UNSIGNED:
			break;

		case UCHAR:
		case USHORT:
			t = UNSIGNED;
			break;

		case FLOAT:
			printf( "%lo\t", (unsigned long)DOUBLE );
			return;

		default:
			printf( "%lo\t", (unsigned long)p->type );
			return;
		}

	if( p->op == ICON ) printf( "%lo<1\t", (unsigned long)t );
	else printf( "%lo\t", (unsigned long)t );
	}

# define VALSET 1
# define VALUSED 2
# define VALASGOP 4
# define VALADDR 8

lprt( p, down, uses ) register NODE *p; {
	register struct symtab *q;
	register id;
	register acount;
	register down1, down2;
	register use1, use2;
	register struct lnm *np1, *np2;

	/* first, set variables which are set... */

	use1 = use2 = VALUSED;
	if( p->op == ASSIGN ) use1 = VALSET;
	else if( p->op == UNARY AND ) use1 = VALADDR;
	else if( asgop( p->op ) ){ /* =ops */
		use1 = VALUSED|VALSET;
		if( down == EFF ) use1 |= VALASGOP;
		}


	/* print the lines for lint */

	down2 = down1 = VAL;
	acount = 0;
	np1 = lnp;

	switch( p->op ){

	case EQ:
	case NE:
	case GT:
	case GE:
	case LT:
	case LE:
		if( p->left->type == CHAR && p->right->op==ICON && p->right->lval < 0 ){
			werror( "nonportable character comparison" );
			}
		if( (p->op==EQ || p->op==NE ) && ISUNSIGNED(p->left->type) && p->right->op == ICON ){
			if( p->right->lval < 0 && p->right->rval == NONAME && !ISUNSIGNED(p->right->type) ){
				werror( "comparison of unsigned with negative constant" );
				}
			}
		break;

	case UGE:
	case ULT:
		if( p->right->op == ICON && p->right->lval == 0 && p->right->rval == NONAME ){
			werror( "unsigned comparison with 0?" );
			break;
			}
	case UGT:
	case ULE:
		if( p->right->op == ICON && p->right->lval <= 0 && !ISUNSIGNED(p->right->type) && p->right->rval == NONAME ){
			werror( "degenerate unsigned comparison" );
			}
		break;

	case COMOP:
		down1 = EFF;

	case ANDAND:
	case OROR:
	case QUEST:
		down2 = down;
		/* go recursively left, then right  */
		np1 = lnp;
		lprt( p->left, down1, use1 );
		np2 = lnp;
		lprt( p->right, down2, use2 );
		lmerge( np1, np2, 0 );
		return;

	case SCONV:
	case PCONV:
		if( p->type == VOID ) break;  /* (void) uses the value */
	case COLON:
		down1 = down2 = down;
		break;

	case CALL:
	case STCALL:
	case FORTCALL:
		acount = ctargs( p->right );
	case UNARY CALL:
	case UNARY STCALL:
	case UNARY FORTCALL:
		if( p->left->op == ICON && (id=p->left->rval) != NONAME && id >= 0 ){ /* used to be &name */
			pname( stab[id].sname, down==EFF ? LUE : LUV,
				DECREF(p->left->type), acount );
			if( acount ) lpta( p->right );
			ident();
			}
		break;

	case ICON:
		/* look for &name case */
		if( (id = p->rval) >= 0 && id != NONAME ){
			q = &stab[id];
			q->sflags |= (SREF|SSET);
			}
		return;

	case NAME:
		if( (id = p->rval) >= 0 && id != NONAME ){
			q = &stab[id];
			if( (uses&VALUSED) && !(q->sflags&SSET) ){
				if( q->sclass == AUTO || q->sclass == REGISTER ){
					if( !ISARY(q->stype ) && !ISFTN(q->stype) && q->stype!=STRTY ){
						werror( "%s may be used before set", q->sname );
						q->sflags |= SSET;
						}
					}
				}
			if( uses & VALASGOP ) break;  /* not a real use */
			if( uses & VALSET ) q->sflags |= SSET;
			if( uses & VALUSED ) q->sflags |= SREF;
			if( uses & VALADDR ) q->sflags |= (SREF|SSET);
			if( p->lval == 0 ){
				lnp->lid = id;
				lnp->flgs = (uses&VALADDR)?0:((uses&VALSET)?VALSET:VALUSED);
				if( ++lnp >= &lnames[LNAMES] ) --lnp;
				}
			}
		return;

		}

	/* recurse, going down the right side first if we can */

	switch( optype(p->op) ){

	case BITYPE:
		np1 = lnp;
		lprt( p->right, down2, use2 );
	case UTYPE:
		np2 = lnp;
		lprt( p->left, down1, use1 );
		}

	if( optype(p->op) == BITYPE ){
		if( p->op == ASSIGN && p->left->op == NAME ){ /* special case for a =  .. a .. */
			lmerge( np1, np2, 0 );
			}
		else lmerge( np1, np2, p->op != COLON );
		/* look for assignments to fields, and complain */
		if( p->op == ASSIGN && p->left->op == FLD && p->right->op == ICON ) fldcon( p );
		}

	}

lmerge( np1, np2, flag ) struct lnm *np1, *np2; {
	/* np1 and np2 point to lists of lnm members, for the two sides
	 * of a binary operator
	 * flag is 1 if commutation is possible, 0 otherwise
	 * lmerge returns a merged list, starting at np1, resetting lnp
	 * it also complains, if appropriate, about side effects
	 */

	register struct lnm *npx, *npy;

	for( npx = np2; npx < lnp; ++npx ){

		/* is it already there? */
		for( npy = np1; npy < np2; ++npy ){
			if( npx->lid == npy->lid ){ /* yes */
				if( npx->flgs == 0 || npx->flgs == (VALSET|VALUSED) )
					;  /* do nothing */
				else if( (npx->flgs|npy->flgs)== (VALSET|VALUSED) ||
					(npx->flgs&npy->flgs&VALSET) ){
					if( flag ) werror( "%s evaluation order undefined", stab[npy->lid].sname );
					}
				if( npy->flgs == 0 ) npx->flgs = 0;
				else npy->flgs |= npx->flgs;
				goto foundit;
				}
			}

		/* not there: update entry */
		np2->lid = npx->lid;
		np2->flgs = npx->flgs;
		++np2;

		foundit: ;
		}

	/* all finished: merged list is at np1 */
	lnp = np2;
	}

int
efcode(){
	/* code for the end of a function */
	register struct symtab *cfp;

	cfp = &stab[curftn];
	if( retstat & RETVAL ){
		pname( cfp->sname, LRV, DECREF( cfp->stype), 0 );
		ident();
		}
	if( !vflag ){
		vflag = argflag;
		argflag = 0;
		}
	if( retstat == RETVAL+NRETVAL )
		werror( "function %s has return(e); and return;", cfp->sname);
	return( 0 );
	}

int
aocode(p) struct symtab *p; {
	/* called when automatic p removed from stab */
	register struct symtab *cfs;
	cfs = &stab[curftn];
	if(p->suse>0 && !(p->sflags&SMOS) ){
		if( p->sclass == PARAM ){
			if( vflag ) werror( "argument %s unused in function %s",
				p->sname,
				cfs->sname );
			}
		else {
			if( p->sclass != TYPEDEF ) werror( "%s unused in function %s",
				p->sname, cfs->sname );
			}
		}

	if( p->suse < 0 && (p->sflags & (SSET|SREF|SMOS)) == SSET &&
		!ISARY(p->stype) && !ISFTN(p->stype) ){

		werror( "%s set but not used in function %s", p->sname, cfs->sname );
		}

	if( p->stype == STRTY || p->stype == UNIONTY || p->stype == ENUMTY ){
		if( dimtab[p->sizoff+1] < 0 ) werror( "structure %s never defined", p->sname );
		}
	return( 0 );
	}

int
defnam( p ) register struct symtab *p; {
	/* define the current location as the name p->sname */

	if( p->sclass == STATIC && p->slevel>1 ) return( 0 );

	if( !ISFTN( p->stype ) ){
		pname( p->sname, libflag?LIB:LDI, p->stype, 0 );
		ident();
		}
	return( 0 );
	}

int
zecode( n ){
	/* n integer words of zeros */
	OFFSZ temp;
	temp = n;
	inoff += temp*SZINT;
	return( 0 );
	}

int
andable( p ) NODE *p; {  /* p is a NAME node; can it accept & ? */
	register r;

	if( p->op != NAME ) cerror( "andable error" );

	if( (r = p->rval) < 0 ) return(1);  /* labels are andable */

	if( stab[r].sclass == AUTO || stab[r].sclass == PARAM ) return(0);
	return(1);
	}

NODE *
clocal(p) NODE *p; {

	/* lint's conversion checks; the native clocal does the rewriting */

	register o;
	TWORD t, tl;

	switch( o = p->op ){

	case SCONV:
	case PCONV:
		if( p->left->type==ENUMTY ){
			p->left = pconvert( p->left );
			}
		t = p->type;
		tl = p->left->type;
		if( aflag && (tl==LONG||tl==ULONG) && (t!=LONG&&t!=ULONG) ){
			werror( "long assignment may lose accuracy" );
			}
		if( ISPTR(tl) && ISPTR(t) ){
			tl = DECREF(tl);
			t = DECREF(t);
			switch( ISFTN(t) + ISFTN(tl) ){

			case 0:  /* neither is a function pointer */
				if( t != VOID && talign(t,p->csiz) > talign(tl,p->left->csiz) ){
					if( hflag||pflag ) werror( "possible pointer alignment problem" );
					}
				break;

			case 1:
				werror( "questionable conversion of function pointer" );

			case 2:
				;
				}
			}
		}

	return( pcc_native_clocal(p) );
	}

int
cinit( p, sz ) NODE *p; { /* initialize p into size sz */
	inoff += sz;
	if( p->op == INIT ){
		if( p->left->op == ICON ) return( 0 );
		if( p->left->op == NAME && p->left->type == MOE ) return( 0 );
		}
	uerror( "illegal initialization" );
	return( 0 );
	}

char *
exname( p ) char *p; {
	/* make a name look like an external name in the local machine */
	static char aa[8];
	register int i;

	if( !pflag ) return(p);
	for( i=0; i<6; ++i ){
		if( isupper(*p ) ) aa[i] = tolower( *p );
		else aa[i] = *p;
		if( *p ) ++p;
		}
	aa[6] = '\0';
	return( aa );
	}

	/* a number of dummy routines, unneeded by lint */

int getlab(){ return( ++labels ); }
void pcc_note_frame(unsigned n){;}
int locctr(n){ return( 0 ); }
int branch(n){ return( 0 ); }
int defalign(n){ return( 0 ); }
int deflab(n){ return( 0 ); }
int bycode(t,i){ return( 0 ); }
int bccode(){ return( 0 ); }
int aobeg(){ return( 0 ); }
int aoend(){ return( 0 ); }
int genswitch(p,n) struct sw *p; { return( 0 ); }
int incode(p,s) NODE *p; { inoff += s; return( 0 ); }
int fincode(long double d,int s){ inoff += s; return( 0 ); }
int vfdzero(n){ inoff += n; return( 0 ); }

int
fldty(p) struct symtab *p; {
	return( 0 ); /* all types are OK here... */
	}

int
fldal(t) TWORD t; { /* field alignment... */
	if( t == ENUMTY ) return( ALCHAR );  /* this should be thought through better... */
	if( ISPTR(t) ){ /* really for the benefit of honeywell (and someday IBM) */
		if( pflag ) uerror( "nonportable field type" );
		}
	else uerror( "illegal field type" );
	return(ALINT);
	}

main( argc, argv ) char *argv[]; {
	char *p;

	/* handle options */

	for( p=argc>1?argv[1]:""; *p; ++p ){

		switch( *p ){

		case '-':
			continue;

		case 'L':  /* produced by driver program */
			flabel = p;
			goto break2;

		case 'b':
			brkflag = 1;
			continue;

		case 'p':
			pflag = 1;
			continue;

		case 'c':
			cflag = 1;
			continue;

		case 's':
			/* for the moment, -s triggers -h */

		case 'h':
			hflag = 1;
			continue;

		case 'v':
			vflag = 0;
			continue;

		case 'x':
			xflag = 1;
			continue;

		case 'a':
			aflag = 1;
		case 'u':	/* done in second pass */
		case 'n':	/* done in shell script */
			continue;

		case 't':
			werror( "option %c now default: see `man 6 lint'", *p );
			continue;

		default:
			uerror( "illegal option: %c", *p );
			continue;

			}
		}

	break2:
	pcc_native_source = 1;	/* modern =op lexing, as ccom --native-source */
	return( mainp1( argc, argv ) );
	}

commdec( i ){
	/* put out a common declaration */
	register struct symtab *p;
	p = &stab[i];
	pname( p->sname, libflag?LIB:LDC, p->stype, 0 );
	ident();
	return( 0 );
	}

fldcon( p ) register NODE *p; {
	/* p is an assignment of a constant to a field */
	/* check to see if the assignment is going to overflow, or otherwise cause trouble */
	register s;
	CONSZ v;

	if( !hflag & !pflag ) return;

	s = UPKFSZ(p->left->rval);
	v = p->right->lval;

	switch( p->left->type ){

	case CHAR:
	case INT:
	case SHORT:
	case LONG:
	case ENUMTY:
		if( v>=0 && (v>>(s-1))==0 ) return;
		werror( "precision lost in assignment to (possibly sign-extended) field" );
	default:
		return;

	case UNSIGNED:
	case UCHAR:
	case USHORT:
	case ULONG:
		if( v<0 || (v>>s)!=0 ) werror( "precision lost in field assignment" );

		return;
		}

	}
