#include <stdio.h>
#include <limits.h>
main(argc, argv)
char **argv;
{
	int c; unsigned n;
	char *s;

	n = 0;
	if(argc < 2) {
		printf("arg count\n");
		exit(0);
	}
	s = argv[1];
	while(c = *s++) {
		if(c<'0' || c>'9') {
			printf("bad character\n");
			exit(0);
		}
		if(n>(UINT_MAX-(unsigned)(c-'0'))/10) { printf("argument too large\n");exit(1); }
		n = n*10 + c - '0';
	}
	sleep(n);
    return 0;
}
