/*
 * System V structure, floating and variadic ABI fixture for the native
 * V7 C compiler.  tests/c_abi_driver.c, compiled by GCC, calls these
 * functions and is called back by them.  Each structure has its own
 * member names (the native profile).
 */
struct sp { int a, b; };
struct sd { double x, y; };
struct sm { long l; double d; };
struct sf { float f, g; int c; };
struct so { char c[3]; };
struct sb { long a, b, c; };

struct sp gccpair();
struct sb gccbig();
struct sd gccsd();
float gccfloat(float, double);
double gccvar(int, ...);

struct sp
mkpair(a, b)
int a, b;
{
	struct sp p;

	p.a = a;
	p.b = b;
	return(p);
}

int
sumpair(p)
struct sp p;
{
	return(p.a + p.b);
}

struct sd
scaled(struct sd d, double k)
{
	d.x = d.x * k;
	d.y = d.y * k;
	return(d);
}

struct sm
swapm(m)
struct sm m;
{
	struct sm r;

	r.l = m.d;
	r.d = m.l;
	return(r);
}

struct sf
addf(a, b)
struct sf a, b;
{
	struct sf r;

	r.f = a.f + b.f;
	r.g = a.g + b.g;
	r.c = a.c + b.c;
	return(r);
}

struct so
nexto(o)
struct so o;
{
	o.c[0]++;
	o.c[1]++;
	o.c[2]++;
	return(o);
}

struct sb
addb(a, b)
struct sb a, b;
{
	struct sb r;

	r.a = a.a + b.a;
	r.b = a.b + b.b;
	r.c = a.c + b.c;
	return(r);
}

struct sb
mkbig(a, b, c)
long a, b, c;
{
	struct sb r;

	r.a = a;
	r.b = b;
	r.c = c;
	return(r);
}

long
spill(a, b, c, d, e, f, g, h)
struct sp a, b, c, d, e, f, g, h;
{
	return(a.a + b.a*2 + c.a*3 + d.a*4 + e.a*5 + f.a*6 + g.a*7 + h.a*8 + h.b*100);
}

double
fsum(a, b, c, d, e, f, g, h, i, j)
double a, b, c, d, e, f, g, h, i, j;
{
	return(a + b + c + d + e + f + g + h + i + j*10);
}

float
fmul(float a, float b)
{
	return(a * b);
}

double
mixed(int i, float f, double d, long l)
{
	return(i + f + d + l);
}

int
nested()
{
	return(sumpair(mkpair(5, 6)) + mkpair(1, 2).b);
}

long
callgcc()
{
	struct sp p;
	struct sb a, b, r;
	struct sd d;

	p = gccpair(3, 4);
	a.a = 1; a.b = 2; a.c = 3;
	b.a = 10; b.b = 20; b.c = 30;
	r = gccbig(a, b);
	d = gccsd(p);
	return(p.a*10 + p.b + r.a + r.b + r.c + (long)d.x + (long)d.y);
}

double
callfloat()
{
	return(gccfloat(1.5, 2.25) + gccfloat(3, 1));
}

double
callvar()
{
	struct sp p;

	p.a = 4;
	p.b = 5;
	return(gccvar(4, 1.5, 7, 2.25, p));
}

double
vsum(int n, ...)
{
	__builtin_va_list ap;
	double s;
	int i;

	__builtin_va_start(ap, n);
	s = 0;
	for (i = 0; i < n; i++) {
		s = s + __builtin_va_arg(ap, int);
		s = s + __builtin_va_arg(ap, double);
	}
	__builtin_va_end(ap);
	return(s);
}

long
vcopy(int n, ...)
{
	__builtin_va_list ap, aq;
	long s;
	int i;

	__builtin_va_start(ap, n);
	__builtin_va_copy(aq, ap);
	s = 0;
	for (i = 0; i < n; i++)
		s = s + __builtin_va_arg(ap, long);
	for (i = 0; i < n; i++)
		s = s + 10 * __builtin_va_arg(aq, long);
	__builtin_va_end(aq);
	__builtin_va_end(ap);
	return(s);
}
