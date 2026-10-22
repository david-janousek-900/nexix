int twice(n) { return(n * 2); }
int helper() { return(1); }
int total;
int lonelyext;
/* VARARGS1 */
int vfun(fmt, a) char *fmt; { return(0); }
setonly(p) int p; { int x; x = 1; }
/* ARGSUSED */
quiet(p, q) { return(0); }
static int lonely(u) unsigned u; { if (u < 0) return(1); return(0); }
int order(i) { int a[4]; a[i] = i++; return(a[0]); }
mixed(i) { if (i) return(i); return; }
noret() { }
int usenoret() { return(noret() + quiet(1, 2) + order(1) + mixed(2) + lonely(3)); }
