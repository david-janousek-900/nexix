/* Address-constant initializers, in the shapes of generated lex tables. */
struct acw { int v; } acw[4] = { 10, 11, 12, 13 };
struct acs { struct acw *w; struct acs *o; int *x; };
extern struct acs acs[];
struct acs acs[] = {
	acw+3, 0, 0,
	2+acw, acs+1, &acw[1].v,
	&acw[3]-2, acs, 0,
	(acw+1) - 1 - 1 + 2, acs + (2 - 1), 0,
};
static int ac[4] = { 1, 2, 3, 4 };
int *acp = ac - (1 - 4);
char *acm = "address"+3;
static char *acn = 2+"xyz";

main()
{
	printf("%d %d %d %d\n", acs[0].w->v, acs[1].w->v, acs[2].w->v, acs[3].w->v);
	printf("%d %d %d %d\n", acs[1].o == &acs[1], *acs[1].x, acs[2].o == acs, acs[3].o == &acs[1]);
	printf("%d %s %s\n", *acp, acm, acn);
	return(0);
}
