/* GCC-built caller and callee half of the native V7 C ABI fixture. */
#include <stdio.h>
#include <stdarg.h>
struct sp { int a, b; };
struct sd { double x, y; };
struct sm { long l; double d; };
struct sf { float f, g; int c; };
struct so { char c[3]; };
struct sb { long a, b, c; };
struct sp mkpair(int, int); int sumpair(struct sp); struct sd scaled(struct sd, double);
struct sm swapm(struct sm); struct sf addf(struct sf, struct sf); struct so nexto(struct so);
struct sb addb(struct sb, struct sb); struct sb mkbig(long, long, long);
long spill(struct sp, struct sp, struct sp, struct sp, struct sp, struct sp, struct sp, struct sp);
double fsum(double, double, double, double, double, double, double, double, double, double);
float fmul(float, float); double mixed(int, float, double, long); int nested(void);
long callgcc(void); double callfloat(void); double callvar(void);
double vsum(int, ...); long vcopy(int, ...);
struct sp gccpair(int a, int b) { struct sp p = { a*2, b*3 }; return p; }
struct sb gccbig(struct sb a, struct sb b) { struct sb r = { a.a*b.a, a.b*b.b, a.c*b.c }; return r; }
struct sd gccsd(struct sp p) { struct sd d = { p.a + 0.5, p.b + 0.25 }; return d; }
float gccfloat(float f, double d) { return f * (float)d; }
double gccvar(int n, ...)
{
	va_list ap; double s; int i; struct sp p;
	va_start(ap, n);
	s = va_arg(ap, double); i = va_arg(ap, int); s += va_arg(ap, double); p = va_arg(ap, struct sp);
	va_end(ap);
	return s + i + n + p.a + p.b;
}
#define CHECK(x) do { if (!(x)) { printf("FAIL abi line %d\n", __LINE__); return 1; } } while (0)
int main(void)
{
	struct sp p = mkpair(3, -9); CHECK(p.a == 3 && p.b == -9 && sumpair(p) == -6);
	{ struct sd d = { 1.5, -2.25 }; d = scaled(d, 2.0); CHECK(d.x == 3.0 && d.y == -4.5); }
	{ struct sm m = { 7, 2.75 }; m = swapm(m); CHECK(m.l == 2 && m.d == 7.0); }
	{ struct sf a = { 1.5f, 2.5f, 3 }, b = { 0.25f, 0.5f, -10 }, r = addf(a, b); CHECK(r.f == 1.75f && r.g == 3.0f && r.c == -7); }
	{ struct so o = { { 1, 2, 3 } }; o = nexto(o); CHECK(o.c[0] == 2 && o.c[1] == 3 && o.c[2] == 4); }
	{ struct sb a = { 1, 2, 3 }, b = { 4000000000L, 5, -6 }, r = addb(a, b); CHECK(r.a == 4000000001L && r.b == 7 && r.c == -3); }
	{ struct sb r = mkbig(5, 6, 7); CHECK(r.a == 5 && r.b == 6 && r.c == 7); }
	{ struct sp s[8]; int i; for (i = 0; i < 8; i++) { s[i].a = i+1; s[i].b = i*11; }
	  CHECK(spill(s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]) == 204 + 7700); }
	CHECK(fsum(1, 2, 3, 4, 5, 6, 7, 8, 9, 10) == 145.0);
	CHECK(fmul(1.5f, 2.5f) == 3.75f && mixed(1, 0.5f, 2.25, 4) == 7.75);
	CHECK(nested() == 13);
	CHECK(callgcc() == 6*10 + 12 + 10 + 40 + 90 + 6 + 12);
	CHECK(callfloat() == 3.375 + 3.0);
	CHECK(callvar() == 1.5 + 2.25 + 7 + 4 + 4 + 5);
	CHECK(vsum(3, 1, 0.5, 2, 0.25, 3, 0.125) == 6.875);
	CHECK(vsum(10, 1, 1.0, 1, 1.0, 1, 1.0, 1, 1.0, 1, 1.0, 1, 1.0, 1, 1.0, 1, 1.0, 1, 1.0, 1, 1.0) == 20.0);
	CHECK(vcopy(8, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L) == 396);
	puts("PASS: native V7 C structure, floating and variadic ABI");
	return 0;
}
