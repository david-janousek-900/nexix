/*
 * C compiler, phase 1
 *
 *
 * Handles processing of declarations,
 * except for top-level processing of
 * externals.
 */

#include "c0.h"

/* The opaque System V va_list element: gp/fp offsets and two pointers. */
struct str vastr = { 24, 8, 0, NULL };
static int vadims[1] = { 1 };

/*
 * Process a sequence of declaration statements
 */
int
declist(int sclass)
{
	register int sc, offset;
	struct hshtab typer;

	offset = 0;
	sc = sclass;
	while (getkeywords(&sclass, &typer)) {
		offset = declare(sclass, &typer, offset);
		sclass = sc;
	}
	return(offset+align(CHAR, offset, 0, NULL));
}

/*
 * Is the symbol the start of a declaration's type?
 */
static int
istype(int o)
{
	if (o==NAME)
		return(csym->hclass==TYPEDEF);
	if (o!=KEYW)
		return(0);
	switch (cval) {
	case INT: case CHAR: case FLOAT: case DOUBLE: case STRUCT: case UNION:
	case ENUM: case LONG: case UNSIGN: case SHORTKW: case SIGNED: case VOIDKW:
	case QUALIF: case VALIST: case ATTRIB: case REG:
		return(1);
	}
	return(0);
}

/*
 * Read the keywords introducing a declaration statement
 * Store back the storage class, and fill in the type
 * entry, which looks like a hash table entry.
 */
int
getkeywords(int *scptr, struct hshtab *tptr)
{
	register int skw, tkw, longf;
	int o, isadecl, ismos, unsignf, signf, shortf;

	isadecl = 0;
	longf = 0;
	unsignf = signf = shortf = 0;
	tptr->htype = INT;
	tptr->hstrp = NULL;
	tptr->hsubsp = NULL;
	tkw = -1;
	skw = *scptr;
	ismos = skw==MOS||skw==MOU;
	for (;;) {
		mosflg = ismos && isadecl;
		if (mosflg) {
			memqual = curstr;
			memdecl = 1;
		}
		o = symbol();
		if (o==NAME && csym->hclass==TYPEDEF && tkw<0 && !longf && !unsignf
		 && !signf && !shortf) {
			tkw = csym->htype;
			tptr->hsubsp = csym->hsubsp;
			tptr->hstrp = csym->hstrp;
			isadecl++;
			continue;
		}
		switch (o==KEYW? cval: -1) {
		case AUTO:
		case STATIC:
		case EXTERN:
		case REG:
		case TYPEDEF:
			if (skw && skw!=cval) {
				if (skw==ARG && cval==REG)
					cval = AREG;
				else
					error("Conflict in storage class");
			}
			skw = cval;
			break;

		case UNSIGN:
			unsignf++;
			break;

		case SIGNED:
			signf++;
			break;

		case SHORTKW:
			shortf++;
			break;

		case LONG:
			longf++;
			break;

		case QUALIF:
			break;

		case ATTRIB:
			skipattr();
			continue;

		case VALIST:
			cval = STRUCT;
			if (tkw>=0)
				error("Type clash");
			tkw = ARRAY+STRUCT;
			tptr->hstrp = &vastr;
			tptr->hsubsp = vadims;
			break;

		case ENUM:
			strdec(ismos, cval);
			cval = INT;
			goto types;

		case VOIDKW:
			cval = VOID;
			goto types;

		case UNION:
		case STRUCT:
			tptr->hstrp = strdec(ismos, cval);
			cval = STRUCT;
		case INT:
		case CHAR:
		case FLOAT:
		case DOUBLE:
		types:
			if (tkw>=0)
				error("Type clash");
			tkw = cval;
			break;

		default:
			peeksym = o;
			if (isadecl==0)
				return(0);
			if (tkw<0)
				tkw = INT;
			if (skw==0)
				skw = blklev==0? DEFXTRN: AUTO;
			if (shortf) {
				if (tkw==INT)
					tkw = unsignf ? USHORT : SHORT;
				else
					error("Misplaced 'short'");
				unsignf = 0;
			}
			if (unsignf || signf) {
				if (unsignf && signf)
					error("Misplaced 'unsigned'");
				if (tkw==INT)
					tkw = unsignf ? UNSIGN : INT;
				else if (tkw==CHAR)
					tkw = unsignf ? UCHAR : CHAR;
				else if (tkw==SHORT || tkw==USHORT)
					;
				else
					error("Misplaced 'unsigned'");
			}
			if (longf) {
				if (tkw==FLOAT || tkw==DOUBLE)
					tkw = DOUBLE;	/* long double: native profile maps to double */
				else if (tkw==INT)
					tkw = LONG;
				else if (tkw==UNSIGN)
					tkw = ULONG;
				else
					error("Misplaced 'long'");
			}
			*scptr = skw;
			tptr->htype = tkw;
			return(1);
		}
		isadecl++;
	}
}

/*
 * Skip a GNU attribute list; it carries no V7 semantics.
 */
void
skipattr(void)
{
	register int o, depth;

	if ((o=symbol())!=LPARN) {
		decsyn(o);
		return;
	}
	depth = 1;
	while (depth && !eof) {
		o = symbol();
		if (o==LPARN)
			depth++;
		else if (o==RPARN)
			depth--;
		else if (o==STRING)
			while (mapch('"') >= 0)
				;
	}
}

/*
 * Process a structure, union, or enum declaration; a subroutine
 * of getkeywords.
 */
struct str *
strdec(int mosf, int kind)
{
	register int elsize, o;
	register struct hshtab *ssym;
	int savebits, savealign;
	struct str *savestr;
	struct hshtab **savememlist;
	int savenmems;
	struct str *strp;
	struct hshtab *ds;
	struct hshtab *mems[NMEMS];
	struct hshtab typer;
	int tagkind;

	memqual = NULL;
	if (kind!=ENUM) {
		tagkind = STRTAG;
		mosflg = 1;
	} else
		tagkind = ENUMTAG;
	ssym = 0;
	elsize = 0;
	if ((o=symbol())==NAME) {
		ssym = csym;
		mosflg = mosf;
		o = symbol();
		if (o==LBRACE && ssym->hblklev<blklev)
			pushdecl(ssym);
		if (ssym->hclass==0) {
			ssym->hclass = tagkind;
			ssym->hstrp = gblock(sizeof(*strp));
			funcbase = curbase;
			ssym->hstrp->ssize = 0;
			ssym->hstrp->salign = 1;
			ssym->hstrp->sid = ++nsid;
			ssym->hstrp->memlist = NULL;
		}
		if (ssym->hclass != tagkind)
			redec();
		strp = ssym->hstrp;
	} else {
		strp = gblock(sizeof(*strp));
		funcbase = curbase;
		strp->ssize = 0;
		strp->salign = 1;
		strp->sid = ++nsid;
		strp->memlist = NULL;
	}
	mosflg = 0;
	if (o != LBRACE) {
		if (ssym==0)
			goto syntax;
		if (ssym->hclass!=tagkind)
			error("Bad structure/union/enum name");
		peeksym = o;
	} else {
		ds = defsym;
		mosflg = 0;
		savebits = bitoffs;
		savememlist = memlist;
		savenmems = nmems;
		savealign = stralign;
		savestr = curstr;
		curstr = strp;
		memlist = mems;
		nmems = 2;
		bitoffs = 0;
		stralign = 1;
		if (kind==ENUM) {
			typer.htype = INT;
			typer.hstrp = strp;
			declare(ENUM, &typer, 0);
		} else
			elsize = declist(kind==UNION?MOU:MOS);
		bitoffs = savebits;
		defsym = ds;
		if (strp->ssize)
			error("%s redeclared", ssym?ssym->name:"structure");
		if (kind!=ENUM) {
			/* native natural alignment of the aggregate */
			strp->salign = stralign;
			elsize = (elsize + stralign - 1) & ~(stralign - 1);
		}
		stralign = savealign;
		curstr = savestr;
		strp->ssize = elsize;
		*memlist++ = NULL;
		strp->memlist = gblock((memlist-mems)*sizeof(*memlist));
		funcbase = curbase;
		for (o=0; &mems[o] != memlist; o++)
			strp->memlist[o] = mems[o];
		memlist = savememlist;
		nmems = savenmems;
		if ((o = symbol()) != RBRACE)
			goto syntax;
	}
	return(strp);
   syntax:
	decsyn(o);
	return(0);
}

/*
 * Process a comma-separated list of declarators
 */
int
declare(int askw, struct hshtab *tptr, int offset)
{
	register int o;
	register int skw, isunion;

	skw = askw;
	isunion = 0;
	if (skw==MOU) {
		skw = MOS;
		isunion++;
		mosflg = 1;
		memqual = curstr;
		memdecl = 1;
		if ((peeksym=symbol()) == SEMI) {
			o = slength(tptr);
			if (o>offset)
				offset = o;
		}
	}
	do {
		if (skw==ENUM && (peeksym=symbol())==RBRACE) {
			o = peeksym;
			peeksym = -1;
			break;
		}
		o = decl1(skw, tptr, isunion?0:offset, NULL);
		if (isunion) {
			o += align(CHAR, o, 0, NULL);
			if (o>offset)
				offset = o;
		} else
			offset += o;
	} while ((o=symbol()) == COMMA);
	if (o==RBRACE) {
		peeksym = o;
		o = SEMI;
	}
	if (o!=SEMI && (o!=RPARN || skw!=ARG1))
		decsyn(o);
	return(offset);
}

/*
 * Process a single declarator
 */
int
decl1(int askw, struct hshtab *atptr, int offset, struct hshtab *absname)
{
	int t1, chkoff, a, elsize;
	register int skw;
	int type;
	register struct hshtab *dsym;
	register struct hshtab *tptr;
	struct tdim dim;
	struct field *fldp;
	int *dp;
	int isinit, len, al;

	skw = askw;
	tptr = atptr;
	chkoff = 0;
	elsize = 0;
	mosflg = skw==MOS;
	if (mosflg) {
		memqual = curstr;
		memdecl = 1;
	}
	dim.rank = 0;
	if (((peeksym=symbol())==SEMI || peeksym==RPARN) && absname==NULL)
		return(0);
	/*
	 * Filler field
	 */
	if (peeksym==COLON && skw==MOS) {
		peeksym = -1;
		t1 = conexp();
		elsize = align(tptr->htype, offset, t1, NULL);
		bitoffs += t1;
		return(elsize);
	}
	csymflag = 0;
	t1 = getype(&dim, absname);
	if (t1 == -1)
		return(0);
	if (tptr->hsubsp) {
		type = tptr->htype;
		for (a=0; type&XTYPE;) {
			if ((type&XTYPE)==ARRAY) {
				if (dim.rank>=16) {
					error("Rank too large");
					break;
				}
				dim.dimens[dim.rank++] = tptr->hsubsp[a++];
			}
			type >>= TYLEN;
		}
	}
	type = tptr->htype & ~TYPE;
	while (t1&XTYPE) {
		if (type&BIGTYPE) {
			typov();
			type = t1 = 0;
		}
		type = type<<TYLEN | (t1 & XTYPE);
		t1 >>= TYLEN;
	}
	type |= tptr->htype&TYPE;
	if (absname) {
		/*
		 * A named prototype parameter is a real declarator only in
		 * the outermost parameter list of a file-level declaration.
		 */
		if (protolev && csymflag && protorec && skw==ARG1)
			absname = NULL;
		else {
			if (protolev && csymflag && defsym && defsym->hclass==0 && defsym->htype==0)
				defsym->hblklev = 0;
			defsym = absname;
		}
	}
	dsym = defsym;
	if (dsym->hblklev < blklev)
		pushdecl(dsym);
	if (dim.rank == 0)
		dsym->hsubsp = NULL;
	else {
		dp = gblock(dim.rank*sizeof(dim.rank));
		funcbase = curbase;
		if (skw==EXTERN)
			maxdecl = curbase;
		for (a=0; a<dim.rank; a++) {
			if ((t1 = dp[a] = dim.dimens[a])
			 && (dsym->htype&XTYPE) == ARRAY
			 && dsym->hsubsp && dsym->hsubsp[a] && t1!=dsym->hsubsp[a])
				redec();
		}
		dsym->hsubsp = dp;
	}
	if ((type&XTYPE) == FUNC) {
		if (skw==AUTO)
			skw = EXTERN;
		if ((skw!=EXTERN && skw!=TYPEDEF) && absname==NULL)
			error("Bad func. storage class");
	}
	if (!(dsym->hclass==0
	   || ((skw==ARG||skw==AREG) && dsym->hclass==ARG1)
	   || (skw==EXTERN && dsym->hclass==EXTERN && dsym->htype==type)))
		if (skw==MOS && dsym->hclass==MOS && dsym->htype==type)
			chkoff = 1;
		else if (skw==EXTERN && dsym->hclass==EXTERN && (type&XTYPE)==FUNC
		 && (dsym->htype&XTYPE)==FUNC) {
			/* V7 declarations may disagree with native prototypes */
			error("Warning: %s redeclared", dsym->name);
			nerror--;
		} else {
			redec();
			goto syntax;
		}
	if (dsym->hclass && (dsym->htype&TYPE)==STRUCT && (type&TYPE)==STRUCT)
		if (dsym->hstrp != tptr->hstrp) {
			error("Warning: structure redeclaration");
			nerror--;
		}
	dsym->htype = type;
	if (tptr->hstrp)
		dsym->hstrp = tptr->hstrp;
	if (skw==TYPEDEF) {
		dsym->hclass = TYPEDEF;
		return(0);
	}
	if (absname)
		return(0);
	if (skw==ARG1) {
		if (paraml==0)
			paraml = dsym;
		else
			parame->hnext = dsym;
		parame = dsym;
		dsym->hnext = NULL;
		/* a prototype parameter is declared by its own type */
		dsym->hclass = protolev ? ARG : skw;
		return(0);
	}
	elsize = 0;
	if (skw==MOS) {
		elsize = slength(dsym);
		if ((peeksym = symbol())==COLON) {
			elsize = 0;
			peeksym = -1;
			t1 = conexp();
			a = align(type, offset, t1, NULL);
			if (dsym->hflag&FFIELD) {
				if (((struct field *)dsym->hstrp)->bitoffs!=bitoffs
			 	 || ((struct field *)dsym->hstrp)->flen!=t1)
					redec();
			} else {
				fldp = gblock(sizeof(*fldp));
				dsym->hstrp = (struct str *)fldp;
				funcbase = curbase;
			}
			dsym->hflag |= FFIELD;
			((struct field *)dsym->hstrp)->bitoffs = bitoffs;
			((struct field *)dsym->hstrp)->flen = t1;
			bitoffs += t1;
		} else
			a = align(type, offset, 0, dsym->hstrp);
		elsize += a;
		offset += a;
		if (++nmems >= NMEMS) {
			error("Too many structure members");
			nmems -= NMEMS/2;
			memlist -= NMEMS/2;
		}
		if (chkoff && dsym->hoffset != offset)
			redec();
		dsym->hoffset = offset;
		*memlist++ = dsym;
		memalias(dsym);
	}
	if (skw==REG)
		if ((dsym->hoffset = goodreg(dsym)) < 0)
			skw = AUTO;
	dsym->hclass = skw;
	isinit = 0;
	if ((a=symbol())!=COMMA && a!=SEMI && a!=RBRACE)
		isinit++;
	if (a!=ASSIGN)
		peeksym = a;
	if (skw==AUTO) {
		len = slength(dsym);
		al = talign(dsym->htype, dsym->hstrp);
		if (al < 4)
			al = 4;
		autolen = (autolen - len) & ~(al - 1);
		dsym->hoffset = autolen;
		if (autolen < maxauto)
			maxauto = autolen;
		if (isinit)
			cinit(dsym, 0, AUTO);
	} else if (skw==STATIC) {
		dsym->hoffset = isn;
		if (isinit) {
			outcode("BBNBN", DATA, EVEN, talign(dsym->htype, dsym->hstrp), LABEL, isn++);
			cinit(dsym, 1, STATIC);
		} else
			outcode("BBNBNBN", BSS, EVEN, talign(dsym->htype, dsym->hstrp), LABEL, isn++,
			    SSPACE, srlength(dsym));
		outcode("B", PROG);
	} else if (skw==REG && isinit)
		cinit(dsym, 0, REG);
	else if (skw==ENUM) {
		if (type!=INT)
			error("Illegal enumeration %s", dsym->name);
		dsym->hclass = ENUMCON;
		dsym->hoffset = offset;
		if (isinit)
			cinit(dsym, 0, ENUMCON);
		elsize = dsym->hoffset-offset+1;
	}
	prste(dsym);
syntax:
	return(elsize);
}

/*
 * Enter the first declaration of each member name under its plain name
 * too, for V7 references through pointers that are not structures.
 */
void
memalias(struct hshtab *dsym)
{
	char *at, save[NCPS+16];
	struct hshtab *cs;
	int ms;

	if ((at = strchr(dsym->name, '@')) == NULL)
		return;
	strcpy(save, symbuf);
	ms = mossym;
	memcpy(symbuf, dsym->name, at-dsym->name);
	symbuf[at-dsym->name] = 0;
	mossym = FMOS;
	cs = csym;
	if (lookup()==NAME && csym->hclass==0) {
		int kw = csym->hflag&FKEYW;

		cpysymb(csym, dsym);
		csym->hflag = (csym->hflag&~FKEYW) | kw;
		csym->hblklev = dsym->hblklev;
		csym->hclass = MOS;
	}
	csym = cs;
	mossym = ms;
	strcpy(symbuf, save);
}

/*
 * Push down an outer-block declaration
 * after redeclaration in an inner block.
 */
void
pushdecl(struct hshtab *asp)
{
	register struct hshtab *sp, *nsp;

	sp = asp;
	nsp = gblock(sizeof(*nsp));
	maxdecl = funcbase = curbase;
	cpysymb(nsp, sp);
	sp->hclass = 0;
	sp->hflag &= (FKEYW|FMOS);
	sp->htype = 0;
	sp->hoffset = 0;
	sp->hblklev = blklev;
	sp->hpdown = nsp;
}

/*
 * Copy the non-name part of a symbol
 */
void
cpysymb(struct hshtab *s1, struct hshtab *s2)
{
	register struct hshtab *rs1, *rs2;

	rs1 = s1;
	rs2 = s2;
	rs1->hclass = rs2->hclass;
	rs1->hflag = rs2->hflag;
	rs1->htype = rs2->htype;
	rs1->hoffset = rs2->hoffset;
	rs1->hsubsp = rs2->hsubsp;
	rs1->hstrp = rs2->hstrp;
	rs1->hblklev = rs2->hblklev;
	rs1->hpdown = rs2->hpdown;
}

/*
 * Parse a prototype parameter list after its "(".  Parameters of the
 * outermost list of a file-level declarator are recorded like V7
 * identifier lists, already carrying their types; the others only
 * describe types and are discarded, as are all prototype types at calls.
 */
int
protolist(struct hshtab *ds)
{
	register int o;
	int savecsym, saverec, first;
	struct hshtab typer, dummy;
	int sc;

	savecsym = csymflag;
	saverec = protorec;
	protorec = blklev==0 && protolev==0;
	protolev++;
	if (protorec)
		blklev++;
	first = 1;
	for (;;) {
		o = symbol();
		if (o==RPARN)
			break;
		if (o==ELLIPS) {
			if (protorec)
				variadic = 1;
			if ((o=symbol())!=RPARN)
				decsyn(o);
			break;
		}
		if (first && o==KEYW && cval==VOIDKW && nextchar()==')') {
			symbol();
			break;
		}
		first = 0;
		peeksym = o;
		sc = ARG;
		if (!getkeywords(&sc, &typer)) {
			decsyn(symbol());
			break;
		}
		memset(&dummy, 0, sizeof(dummy));
		dummy.name = nullname;
		dummy.hblklev = blklev;
		decl1(ARG1, &typer, 0, &dummy);
		if (sc==AREG && protorec && parame && parame->hclass==ARG)
			parame->hclass = AREG;
		if ((o=symbol())==RPARN)
			break;
		if (o!=COMMA) {
			decsyn(o);
			break;
		}
	}
	if (protorec)
		blklev--;
	protolev--;
	protorec = saverec;
	csymflag = savecsym;
	defsym = ds;
	return(0);
}

/*
 * Read a declarator and get the implied type
 */
int
getype(struct tdim *adimp, struct hshtab *absname)
{
	static struct hshtab argtype;
	int type;
	register int o;
	register struct hshtab *ds;
	register struct tdim *dimp;

	ds = defsym;
	dimp = adimp;
	type = 0;
	switch(o=symbol()) {

	case TIMES:
		while ((o=symbol())==KEYW && (cval==QUALIF || cval==ATTRIB))
			if (cval==ATTRIB)
				skipattr();
		peeksym = o;
		type = getype(dimp, absname);
		if (type==-1)
			return(type);
		if (type&BIGTYPE) {
			typov();
			type = 0;
		}
		return(type<<TYLEN | PTR);

	case LPARN:
		if (absname==NULL || nextchar()!=')') {
			o = symbol();
			peeksym = o;
			if (absname && (istype(o) || o==ELLIPS)) {
				/* abstract function declarator: (int, char *) */
				protolist(absname);
				defsym = ds = absname;
				absname = NULL;
				if (type&BIGTYPE) {
					typov();
					type = 0;
				}
				type = type<<TYLEN | FUNC;
				goto getf;
			}
			type = getype(dimp, absname);
			if (type==-1)
				return(type);
			ds = defsym;
			if ((o=symbol()) != RPARN)
				goto syntax;
			goto getf;
		}

	default:
		peeksym = o;
		if (absname) {
			defsym = ds = absname;
			absname = NULL;
			goto getf;
		}
		break;

	case KEYW:
		if (cval==ATTRIB) {
			skipattr();
			return(getype(dimp, absname));
		}
		peeksym = o;
		if (absname) {
			defsym = ds = absname;
			absname = NULL;
			goto getf;
		}
		break;

	case NAME:
		defsym = ds = csym;
		csymflag = 1;
	getf:
		switch(o=symbol()) {

		case LPARN:
			o = symbol();
			peeksym = o;
			if (istype(o) || o==ELLIPS) {
				protolist(ds);
			} else if (blklev==0 && protolev==0) {
				blklev++;
				ds = defsym;
				declare(ARG1, &argtype, 0);
				defsym = ds;
				blklev--;
			} else
				if ((o=symbol()) != RPARN)
					goto syntax;
			if (type&BIGTYPE) {
				typov();
				type = 0;
			}
			type = type<<TYLEN | FUNC;
			goto getf;

		case LBRACK:
			if (dimp->rank>=16) {
				error("Rank too large");
				dimp->rank = 15;
			}
			if ((o=symbol()) != RBRACK) {
				peeksym = o;
				cval = conexp();
				defsym = ds;
				if ((o=symbol())!=RBRACK)
					goto syntax;
			} else {
				if (dimp->rank!=0)
					error("Null dimension");
				cval = 0;
			}
			if (cval < 0 || cval > INT32_MAX)
				error("Bad dimension");
			dimp->dimens[dimp->rank++] = cval;
			if (type&BIGTYPE) {
				typov();
				type = 0;
			}
			type = type<<TYLEN | ARRAY;
			goto getf;

		case KEYW:
			if (cval==ATTRIB) {
				skipattr();
				goto getf;
			}
		}
		peeksym = o;
		return(type);
	}
syntax:
	decsyn(o);
	return(-1);
}

/*
 * More bits required for type than allowed.
 */
void
typov(void)
{
	error("Type is too complicated");
}

/*
 * Enforce alignment restrictions in structures,
 * including bit-field considerations.  Native members use their
 * natural System V alignment; fields pack into 32-bit units.
 */
int
align(int type, int offset, int aflen, struct str *strp)
{
	register int a, t, flen, al;
	char *ftl;

	flen = aflen;
	a = offset;
	t = type;
	ftl = "Field too long";
	if (flen==0) {
		a += (NBPC+bitoffs-1) / NBPC;
		bitoffs = 0;
	}
	while ((t&XTYPE)==ARRAY)
		t = decref(t);
	al = flen ? (type==CHAR||type==UCHAR ? 1 : SZINT) : talign(t, strp);
	if (al > 1) {
		a = (a+al-1) & ~(al-1);
		if (a>offset)
			bitoffs = 0;
	}
	if (al > stralign)
		stralign = al;
	if (flen) {
		if (type==INT || type==UNSIGN) {
			if (flen > NBPW)
				error(ftl);
			if (flen+bitoffs > NBPW) {
				bitoffs = 0;
				a += NCPW;
			}
		} else if (type==CHAR || type==UCHAR) {
			if (flen > NBPC)
				error(ftl);
			if (flen+bitoffs > NBPC) {
				bitoffs = 0;
				a += 1;
			}
		} else
			error("Bad type for field");
	}
	return(a-offset);
}

/*
 * Complain about syntax error in declaration
 */
void
decsyn(int o)
{
	error("Declaration syntax");
	errflush(o);
}

/*
 * Complain about a redeclaration
 */
void
redec(void)
{
	error("%s redeclared", defsym->name);
}

/*
 * Determine if a variable is suitable for storage in
 * a register; if so return the register number.
 * Native register variables are the callee-saved
 * rbx and r12-r15.
 */
int
goodreg(struct hshtab *hp)
{
	int type;

	type = hp->htype;
	if ((!isinttype(type) && !islongtype(type) && (type&XTYPE)==0)
	 || (type&XTYPE)>PTR || regvar<=0)
		return(-1);
	return(--regvar);
}
