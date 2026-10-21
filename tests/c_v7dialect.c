/*
 * V7 dialect accepted by the retained c0: old assignment operators,
 * initializers without "=", the \/ and /\ max/min operators, members
 * through untyped pointers and the single member name space.
 */
int	x 5;
int	tab[] { 1, 2, 3 };
struct	{
	int	aa;
	int	bb;
} s;

main()
{
	int a, b, *ip;

	a = 10;
	a =+ 5;
	a =- 3;
	a =* 2;
	b = a;
	b =/ 5;
	b =% 3;
	a =| 1;
	a =& 0xff;
	a =^ 3;
	a =<< 2;
	a =>> 1;
	printf("%d %d %d %d\n", x, tab[2], a, b);
	a = -1;
	b = -a;
	printf("%d %d %d %d\n", a, b, 3 \/ 7, 3 /\ 7);
	printf("%d %d\n", a \/ b, a /\ b);
	ip = &s;
	ip->bb = 4;
	s.aa = ip->bb + 1;
	printf("%d %d %d\n", s.aa, s.bb, sizeof s);
	return(0);
}
