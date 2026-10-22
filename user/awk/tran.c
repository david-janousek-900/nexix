#include "stdio.h"
#include "awk.def"
#include "awk.h"

static char *okfmt();
static cell *zombies;	/* cells of replaced arrays, see freesymtab */
cell *symtab[MAXSYM];	/* symbol table pointers */

char	**FS;	/* initial field sep */
char	**RS;	/* initial record sep */
char	**OFS;	/* output field sep */
char	**ORS;	/* output record sep */
char	**OFMT;	/*output format for numbers*/
awkfloat *NF;	/* number of fields in current record */
awkfloat *NR;	/* number of current record */
char	**FILENAME;	/* current filename argument */

cell	*recloc;	/* location of record */
cell	*nrloc;		/* NR */
cell	*nfloc;		/* NF */

syminit()
{
	setsymtab("0", tostring("0"), 0.0, NUM|STR|CON|FLD, symtab);
	recloc = setsymtab("$record", record, 0.0, STR|FLD, symtab);
	dprintf("recloc %o lookup %o\n", recloc, lookup("$record", symtab), NULL);
	FS = &setsymtab("FS", tostring(" "), 0.0, STR|FLD, symtab)->sval;
	RS = &setsymtab("RS", tostring("\n"), 0.0, STR|FLD, symtab)->sval;
	OFS = &setsymtab("OFS", tostring(" "), 0.0, STR|FLD, symtab)->sval;
	ORS = &setsymtab("ORS", tostring("\n"), 0.0, STR|FLD, symtab)->sval;
	OFMT = &setsymtab("OFMT", tostring("%.6g"), 0.0, STR|FLD, symtab)->sval;
	FILENAME = &setsymtab("FILENAME", NULL, 0.0, STR|FLD, symtab)->sval;
	nfloc = setsymtab("NF", NULL, 0.0, NUM, symtab);
	NF = &nfloc->fval;
	nrloc = setsymtab("NR", NULL, 0.0, NUM, symtab);
	NR = &nrloc->fval;
}

cell **makesymtab()
{
	int i;
	cell **cp;

	cp = (char *) malloc(MAXSYM * sizeof(cell *));
	if (cp == NULL)
		error(FATAL, "out of space in makesymtab");
	for (i = 0; i < MAXSYM; i++)
		*((cell **) cp + i) = 0;
	return(cp);
}

freesymtab(ap)	/* free symbol table */
cell *ap;
{
	cell *cp, *np, **tp;
	int i;

	/* Nexix: split replaces an array while an enclosing expression may
	 * still hold one of its elements (a[$1] = split($0, a)), so the cells
	 * wait on a list that reapsymtab frees when the next statement starts.
	 * V7 freed them at once and read each link after freeing its cell. */
	if (!(ap->tval & ARR))
		return;
	tp = (cell **) ap->sval;
	for (i = 0; i < MAXSYM; i++) {
		for (cp = tp[i]; cp != NULL; cp = np) {
			np = cp->nextval;
			cp->nextval = zombies;
			zombies = cp;
		}
	}
	xfree(tp);
}

reapsymtab()
{
	cell *cp;

	while ((cp = zombies) != NULL) {
		zombies = cp->nextval;
		xfree(cp->nval);
		xfree(cp->sval);
		free(cp);
	}
}

cell *setsymtab(n, s, f, t, tab)
char *n, *s;
awkfloat f;
unsigned t;
cell **tab;
{
	register h;
	register cell *p;
	cell *lookup();

	if (n != NULL && (p = lookup(n, tab)) != NULL) {
		xfree(s);
		dprintf("setsymtab found %o: %s", p, p->nval, NULL);
		dprintf(" %s %g %o\n", p->sval, p->fval, p->tval);
		return(p);
	}
	p = (cell *) malloc(sizeof(cell));
	if (p == NULL)
		error(FATAL, "symbol table overflow at %s", n);
	p->nval = tostring(n);
	p->sval = s;
	p->fval = f;
	p->tval = t;
	h = hash(n);
	p->nextval = tab[h];
	tab[h] = p;
	dprintf("setsymtab set %o: %s", p, p->nval, NULL);
	dprintf(" %s %g %o\n", p->sval, p->fval, p->tval);
	return(p);
}

hash(s)	/* form hash value for string s */
register char *s;
{
	register int hashval;

	for (hashval = 0; *s != '\0'; )
		hashval += *s++;
	return(hashval % MAXSYM);
}

cell *lookup(s, tab)	/* look for s in tab */
register char *s;
cell **tab;
{
	register cell *p;

	for (p = tab[hash(s)]; p != NULL; p = p->nextval)
		if (strcmp(s, p->nval) == 0)
			return(p);	/* found it */
	return(NULL);	/* not found */
}

awkfloat setfval(vp, f)
register cell *vp;
awkfloat f;
{
	dprintf("setfval: %o %g\n", vp, f, NULL);
	checkval(vp);
	if (vp == recloc)
		error(FATAL, "can't set $0");
	vp->tval &= ~STR;	/* mark string invalid */
	vp->tval |= NUM;	/* mark number ok */
	if ((vp->tval & FLD) && vp->nval == 0)
		donerec = 0;
	return(vp->fval = f);
}

char *setsval(vp, s)
register cell *vp;
char *s;
{
	char *t;

	dprintf("setsval: %o %s\n", vp, s, NULL);
	checkval(vp);
	if (vp == recloc)
		error(FATAL, "can't set $0");
	vp->tval &= ~NUM;
	vp->tval |= STR;
	if ((vp->tval & FLD) && vp->nval == 0)
		donerec = 0;
	/* Nexix: s may be vp's own string (a = a), or an unset field's */
	t = tostring(s != NULL ? s : "");
	if (!(vp->tval&FLD))
		xfree(vp->sval);
	vp->tval &= ~FLD;
	return(vp->sval = t);
}

awkfloat getfval(vp)
register cell *vp;
{
	awkfloat atof();

	if (vp->sval == record && donerec == 0)
		recbld();
	dprintf("getfval: %o", vp, NULL, NULL);
	checkval(vp);
	if ((vp->tval & NUM) == 0) {
		/* the problem is to make non-numeric things */
		/* have unlikely numeric variables, so that */
		/* $1 == $2 comparisons sort of make sense when */
		/* one or the other is numeric */
		if (vp->sval != NULL && isnumber(vp->sval)) {
			vp->fval = atof(vp->sval);
			if (!(vp->tval & CON))	/* don't change type of a constant */
				vp->tval |= NUM;
		}
		else
			vp->fval = 0.0;	/* not a very good idea */
	}
	dprintf("  %g\n", vp->fval, NULL, NULL);
	return(vp->fval);
}

char *getsval(vp)
register cell *vp;
{
	char s[100];

	if (vp->sval == record && donerec == 0)
		recbld();
	dprintf("getsval: %o", vp, NULL, NULL);
	checkval(vp);
	if ((vp->tval & STR) == 0) {
		if (!(vp->tval&FLD))
			xfree(vp->sval);
		/* Nexix: integers within a native long print exactly, as %ld
		 * (the digits of V7's %.20g); V7's 32-bit long sent larger
		 * values through OFMT. */
		if (vp->fval > -9.2e18 && vp->fval < 9.2e18 && (long)vp->fval==vp->fval)
			if (vp->fval == 0)	/* keeps the sign of -0 */
				snprintf(s, sizeof(s), "%g", vp->fval);
			else
				snprintf(s, sizeof(s), "%ld", (long)vp->fval);
		else
			snprintf(s, sizeof(s), okfmt(*OFMT), vp->fval);
		vp->sval = tostring(s);
		vp->tval &= ~FLD;
		vp->tval |= STR;
	}
	dprintf("  %s\n", vp->sval, NULL, NULL);
	return(vp->sval ? vp->sval : "");	/* Nexix: unset fields are empty */
}

/* Nexix: OFMT must be one floating conversion, or %.6g is used; V7 passed
 * any format to sprintf with a double argument. */
static char *okfmt(f)
char *f;
{
	char *p;
	int n;

	for (n = 0, p = f; p != NULL && *p; p++) {
		if (*p != '%')
			continue;
		if (*++p == '%')
			continue;
		while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0')
			p++;
		while (*p >= '0' && *p <= '9')
			p++;
		if (*p == '.')
			for (p++; *p >= '0' && *p <= '9'; p++)
				;
		if (*p == '\0' || strchr("eEfgG", *p) == NULL)
			return("%.6g");
		n++;
	}
	return(n == 1 ? f : "%.6g");
}

checkval(vp)
register cell *vp;
{
	if (vp->tval & ARR)
		error(FATAL, "illegal reference to array %s", vp->nval);
	if ((vp->tval & (NUM | STR)) == 0)
		error(FATAL, "funny variable %o: %s %s %g %o", vp, vp->nval,
			vp->sval, vp->fval, vp->tval);
}

char *tostring(s)
register char *s;
{
	register char *p;

	p = malloc(strlen(s)+1);
	if (p == NULL)
		error(FATAL, "out of space in tostring on %s", s);
	strcpy(p, s);
	return(p);
}
#ifndef yfree
yfree(a) char *a;
{
	printf("%o\n", a);
	free(a);
}
#endif
#ifdef malloc
#undef malloc
char *ymalloc(u) unsigned u;
{	char *p;
	p = malloc(u);
	printf("%o %o\n", u, p);
	return(p);
}
#endif
