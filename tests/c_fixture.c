/*
 * Integer, pointer, control-flow and call fixture for the native V7 C
 * compiler (c0/c1).  The same source compiled by GCC is the oracle; the
 * printed lines must agree exactly.  V7 dialect: K&R definitions, member
 * names shared by all structures, spaces around "= -" style assignments.
 */
long	gl = 4294967297L;
int	gi = -7;
unsigned gu = 4000000000U;
char	gc = -3;
short	gs = -12345;
unsigned short gus = 60000;
unsigned char guc = 250;
int	tab[5] = { 1, 2, 3, 4, 5 };
char	msg[] = "native";
char	*pmsg = "pointer";
long	*plong = &gl;
int	zero[16];
struct pt {
	int	px;
	long	py;
	char	pc;
};
struct bits {
	unsigned bf1:3;
	unsigned bf2:5;
	unsigned bf3:9;
};
union un {
	long	ul;
	int	ui;
	char	uc;
};
struct pt gpt = { 3, 4294967296L, 'z' };
struct bits gbits;
union un gun;
struct pt pts[3];

int
add(a, b)
int a, b;
{
	return(a + b);
}

long
ladd(a, b)
long a, b;
{
	return(a + b);
}

unsigned
udiv(a, b)
unsigned a, b;
{
	return(a / b);
}

int
sdiv(a, b)
int a, b;
{
	return(a / b);
}

int
smod(a, b)
int a, b;
{
	return(a % b);
}

long
divl(a, b)
long a, b;
{
	return(a / b + a % b);
}

unsigned long
uldiv(a, b)
unsigned long a, b;
{
	return(a / b);
}

int
shifts(x, n)
int x, n;
{
	unsigned u;

	u = x;
	return((x >> n) + (u >> n) + (x << n));
}

long
lshifts(x, n)
long x;
int n;
{
	unsigned long u;

	u = x;
	return((x >> n) ^ (u >> n) ^ (x << n));
}

int
many(a, b, c, d, e, f, g, h, i)
int a, b, c, d, e, f, g, h, i;
{
	return(a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h + 9*i);
}

long
lmany(a, b, c, d, e, f, g, h)
long a, b, c, d, e, f, g, h;
{
	return(a - b + c - d + e - f + g - h);
}

int
fact(n)
int n;
{
	if (n <= 1)
		return(1);
	return(n * fact(n-1));
}

int
fib(n)
register int n;
{
	register int a, b, t;

	a = 0;
	b = 1;
	while (n-- > 0) {
		t = a + b;
		a = b;
		b = t;
	}
	return(a);
}

int
apply(f, x)
int (*f)();
int x;
{
	return((*f)(x, 10));
}

int
sumarr(p, n)
int *p;
int n;
{
	int s;

	s = 0;
	while (n--)
		s += *p++;
	return(s);
}

long
ptrdiff(p, q)
char *p, *q;
{
	return(p - q);
}

int
intdiff(p, q)
int *p, *q;
{
	return(p - q);
}

int
sw(n)
int n;
{
	switch (n) {
	case 0:
		return(10);
	case 1:
		return(11);
	case 2:
		return(12);
	case 3:
	case 4:
		return(14);
	case 6:
		return(16);
	default:
		return(-1);
	}
}

int
sparse(n)
int n;
{
	switch (n) {
	case -100: return(1);
	case 7: return(2);
	case 1000: return(3);
	case 99999: return(4);
	}
	return(0);
}

int
hashed(n)
int n;
{
	switch (n) {
	case 3: return(1);
	case 17: return(2);
	case 50: return(3);
	case 101: return(4);
	case 222: return(5);
	case 999: return(6);
	case 1234: return(7);
	case 5000: return(8);
	case 7777: return(9);
	case 10001: return(10);
	case 32000: return(11);
	case 65536: return(12);
	}
	return(0);
}

int
loops(n)
int n;
{
	int i, j, s;

	s = 0;
	for (i = 0; i < n; i++) {
		if (i == 3)
			continue;
		for (j = 0; j < i; j++)
			s += j;
		if (s > 100)
			break;
	}
	do
		s++;
	while (s < 50);
	if (n > 100)
		goto out;
	s *= 2;
out:
	return(s);
}

int
logic(a, b)
int a, b;
{
	int r;

	r = 0;
	if (a && b)
		r |= 1;
	if (a || b)
		r |= 2;
	if (!a)
		r |= 4;
	if (a > b)
		r |= 8;
	if (a <= b && b != 0)
		r |= 16;
	r |= (a == b) << 5;
	r |= (a ? b : 7) << 6;
	return(r);
}

int
ucompare(a, b)
unsigned a, b;
{
	return((a < b) + 2*(a > b) + 4*(a <= b) + 8*(a >= b));
}

int
lcompare(a, b)
long a, b;
{
	return((a < b) + 2*(a > b) + 4*(a == b));
}

int
narrow(c, s, uc, us)
char c;
short s;
unsigned char uc;
unsigned short us;
{
	return(c + s + uc + us);
}

int
casts(x)
int x;
{
	char c;
	unsigned char uc;
	short s;

	c = x;
	uc = x;
	s = x;
	return(c + uc + s + (char)x + (unsigned char)(x+1) + (short)(x*1000));
}

int
compound(p, n)
int *p;
int n;
{
	p[0] += n;
	p[1] -= n;
	p[2] *= n;
	p[3] /= n;
	p[4] %= n;
	*p <<= 2;
	p[1] >>= 1;
	p[2] &= 0xff;
	p[3] |= 0x100;
	p[4] ^= 0x55;
	return(p[0] + p[1] + p[2] + p[3] + p[4]);
}

int
postpre(p)
int *p;
{
	int a, b, c, d;

	a = (*p)++;
	b = ++*p;
	c = (*p)--;
	d = --*p;
	return(a*1000 + b*100 + c*10 + d);
}

int
fields(bp)
struct bits *bp;
{
	bp->bf1 = 5;
	bp->bf2 = 9;
	bp->bf3 = 300;
	bp->bf1 += 1;
	bp->bf3++;
	return(bp->bf1 + bp->bf2 + bp->bf3);
}

long
structs(p)
struct pt *p;
{
	struct pt local;

	local.px = p->px * 2;
	local.py = p->py + 1;
	local.pc = p->pc;
	pts[1] = local;
	return(pts[1].px + pts[1].py + pts[1].pc);
}

int
statics()
{
	static int count = 3;

	return(count++);
}

static int	sarr[3];
static long	scount = 5;

int
arrays()
{
	int a[10], i, *q;
	char buf[16];

	for (i = 0; i < 10; i++)
		a[i] = i * i;
	q = &a[5];
	buf[0] = 'x';
	buf[1] = msg[1];
	buf[2] = pmsg[2];
	buf[3] = 0;
	sarr[1] = q[-1] + q[1];
	return(a[9] + *q + sarr[1] + buf[1] + buf[2] + sizeof(a) + sizeof(buf));
}

int
globals()
{
	gi += 5;
	gc -= 1;
	gs += 1;
	gus += 10000;
	guc += 10;
	return(gi + gc + gs + gus + guc + (gu > 3000000000U) + tab[4] + scount);
}

int
commas(x)
int x;
{
	int y;

	y = (x++, x++, x * 2);
	return(y + x);
}

unsigned
bitops(a, b)
unsigned a, b;
{
	return((a & b) | (a ^ b) << 1 | ~a & 0xf);
}

int
regvars(n)
int n;
{
	register int i, s;
	register char *p;

	s = 0;
	p = msg;
	for (i = 0; i < n; i++)
		s += *p++;
	return(s);
}

long
mixed(i, l, u)
int i;
long l;
unsigned u;
{
	return(i + l + u + (long)i * l);
}

int
recurse(n, acc)
int n, acc;
{
	if (n == 0)
		return(acc);
	return(recurse(n - 1, acc + n));
}

int
nested(x)
int x;
{
	return(add(add(x, 1), add(fact(3), many(1, 1, 1, 1, 1, 1, 1, 1, 1))));
}

int
unionuse()
{
	gun.ul = 0x123456789L;
	return(gun.ui + gun.uc);
}

int
deref(pp)
int **pp;
{
	**pp += 3;
	return(**pp);
}

main()
{
	int a[5], *ip, r1, r2;
	long lr;

	printf("%d %ld %u %d\n", add(3, 4), ladd(gl, 1L), udiv(4000000000U, 3), sdiv(-17, 5));
	printf("%d %ld %lu\n", smod(-17, 5), divl(-100000000000L, 7L), uldiv(0xffffffffffffffffUL, 10L));
	printf("%d %ld\n", shifts(-100, 3), lshifts(-1234567890123L, 7));
	printf("%d %ld\n", many(1, 2, 3, 4, 5, 6, 7, 8, 9), lmany(1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L));
	printf("%d %d %d\n", fact(10), fib(30), apply(add, 5));
	printf("%d %ld %d\n", sumarr(tab, 5), ptrdiff(msg+5, msg), intdiff(&tab[4], &tab[1]));
	printf("%d %d %d %d %d %d\n", sw(0), sw(2), sw(4), sw(5), sw(6), sw(99));
	printf("%d %d %d %d %d\n", sparse(-100), sparse(7), sparse(1000), sparse(99999), sparse(8));
	printf("%d %d %d %d %d\n", hashed(3), hashed(222), hashed(65536), hashed(32000), hashed(4));
	printf("%d %d\n", loops(10), loops(200));
	printf("%d %d %d %d\n", logic(0, 0), logic(3, 0), logic(2, 5), logic(4, 4));
	printf("%d %d %d\n", ucompare(1, 0xffffffffU), ucompare(0xffffffffU, 1), ucompare(5, 5));
	printf("%d %d %d\n", lcompare(-1L, 1L), lcompare(4294967296L, 1L), lcompare(7L, 7L));
	printf("%d %d\n", narrow(-1, -2, 255, 65535), casts(300));
	return(rest());
}

int
rest()
{
	int a[5], *ip, r1, r2;
	long lr;

	a[0] = 10; a[1] = 20; a[2] = 30; a[3] = 400; a[4] = 50;
	r1 = compound(a, 7);
	printf("%d %d %d %d %d %d\n", r1, a[0], a[1], a[2], a[3], a[4]);
	a[0] = 5;
	r1 = postpre(a);
	printf("%d %d\n", r1, a[0]);
	r1 = fields(&gbits);
	printf("%d %u %u %u\n", r1, gbits.bf1, gbits.bf2, gbits.bf3);
	lr = structs(&gpt);
	printf("%ld %d %ld %d\n", lr, pts[1].px, pts[1].py, pts[1].pc);
	r1 = statics();
	r2 = statics();
	printf("%d %d %d\n", r1, r2, arrays());
	printf("%d %u %d\n", commas(5), bitops(0xf0f0U, 0x0ff0U), regvars(6));
	printf("%ld %d %d\n", mixed(-5, 1000000000000L, 4000000000U), recurse(100, 0), nested(2));
	printf("%d %d\n", unionuse(), globals());
	ip = &a[2];
	r1 = deref(&ip);
	printf("%d %d\n", r1, a[2]);
	printf("%d %d %d\n", sizeof(struct pt), sizeof(struct bits), sizeof(union un));
	printf("%s %s %c\n", msg, pmsg, gpt.pc);
	return(0);
}
