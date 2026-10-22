/*
 * Floating-point fixture for the native V7 C compiler: arithmetic,
 * conversions, comparisons (including unordered values), constants,
 * assignment operators, arrays and structures of floating values.
 * Results print as integers so the GCC oracle comparison is exact.
 */
double	gd = 2.5;
float	gf = 0.75;
double	dtab[4] = { 1.0, -2.5, 1e10, 0.125 };
struct	fp {
	float	fa;
	double	fb;
};
struct	fp gfp = { 1.5, -3.25 };

double
poly(x)
double x;
{
	return(((3.0 * x - 2.0) * x + 0.5) / 4.0);
}

long
fix(d)
double d;
{
	return((long)(d * 1000.0));
}

int
rel(a, b)
double a, b;
{
	return((a < b) + 2*(a <= b) + 4*(a > b) + 8*(a >= b) + 16*(a == b) + 32*(a != b));
}

int
truth(a)
double a;
{
	if (a)
		return(1);
	return(0);
}

double
assigns(p)
double *p;
{
	p[0] += 1.5;
	p[1] -= 0.25;
	p[2] *= 3.0;
	p[3] /= 2.0;
	return(p[0] + p[1] + p[2] + p[3]);
}

float
fassigns(p)
float *p;
{
	float r;

	*p += 2.0;
	*p *= 1.5;
	r = *p;
	r++;
	return(r);
}

double
convert(i, u, l, ul)
int i;
unsigned u;
long l;
unsigned long ul;
{
	return(i + u + l + (double)ul);
}

long
back(d)
double d;
{
	int i;
	unsigned u;
	long l;
	unsigned long ul;
	char c;

	i = d;
	u = d;
	l = d;
	ul = d * 1e9;
	c = d;
	return(i + u + l + ul/1000 + c);
}

double
negs(d)
double d;
{
	double e;

	e = -d;
	return(-e + -(-d));
}

double
loopsum(n)
int n;
{
	double s;
	int i;

	s = 0.0;
	for (i = 1; i <= n; i++)
		s = s + 1.0 / i;
	return(s);
}

double
recur(n)
int n;
{
	if (n <= 0)
		return(1.0);
	return(n * recur(n - 1));
}

double
many(a, b, c, d, e, f, g, h, i, j, k)
double a, b, c, d, e, f, g, h, i, j;
int k;
{
	return(a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h + 9*i + 10*j + k);
}

main()
{
	double nan, d, a[4];
	float f;

	printf("%ld %ld %ld\n", fix(poly(2.0)), fix(poly(-1.5)), fix(gd * gf));
	printf("%d %d %d\n", rel(1.0, 2.0), rel(2.0, 1.0), rel(3.5, 3.5));
	nan = 0.0;
	nan = nan / nan;
	printf("%d %d %d %d\n", rel(nan, 1.0), rel(1.0, nan), truth(0.0), truth(-0.5));
	printf("%d %d\n", nan == nan, nan != nan);
	a[0] = 1.0; a[1] = 2.0; a[2] = 3.0; a[3] = 4.0;
	d = assigns(a);
	printf("%ld %ld %ld %ld %ld\n", fix(d), fix(a[0]), fix(a[1]), fix(a[2]), fix(a[3]));
	f = 1.0;
	d = fassigns(&f);
	printf("%ld %ld\n", fix(d), fix(f));
	printf("%ld\n", fix(convert(-3, 4000000000U, -5000000000L, 0xfffffffffffff000UL) / 1e9));
	printf("%ld %ld\n", back(123.75), back(-7.5));
	printf("%ld %ld %ld\n", fix(negs(2.25)), fix(loopsum(10)), fix(recur(10) / 1e3));
	printf("%ld\n", fix(many(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.5, 7)));
	printf("%ld %ld %ld %ld\n", fix(dtab[1]), fix(dtab[2] / 1e6), fix(gfp.fa), fix(gfp.fb));
	gfp.fa = gfp.fb * 2;
	printf("%ld %d %d\n", fix(gfp.fa), sizeof gfp, sizeof(float));
	printf("%d %d\n", (int)2.999, (int)-2.999);
	return(0);
}
