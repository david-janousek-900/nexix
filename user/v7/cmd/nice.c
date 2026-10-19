/* nice */

#include <stdio.h>
#include <errno.h>

main(argc, argv)
int argc;
char *argv[];
{
	int nicarg = 10;

	extern char *sys_errlist[];

	if(argc > 1 && argv[1][0] == '-') {
		char *end;long n;errno=0;n=strtol(argv[1]+1,&end,10);
        if(!argv[1][1] || *end || errno || n < -40 || n > 40) { fputs("usage: nice [ -n ] command\n",stderr);exit(1); }
        nicarg=n;
		argc--;
		argv++;
	}
	if(argc < 2) {
		fputs("usage: nice [ -n ] command\n", stderr);
		exit(1);
	}
	nice(nicarg);
	execvp(argv[1], &argv[1]);
	fprintf(stderr, "%s: %s\n", sys_errlist[errno], argv[1]);
	exit(1);
}
