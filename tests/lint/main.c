#include <stdio.h>
#include <string.h>
#include "defs.h"
extern int twice(), lonelyext;
long total;
main(argc, argv) char **argv; {
	int n, m, k;
	n = twice(3, 4);
	m = strlen(argv[0]);
	printf("%d %d\n", n, m);
	helper();
	vfun("x", 1, 2);
	lonelyext = n;
	if (n == m) return(LIMIT);
	return(1);
}
