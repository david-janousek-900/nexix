/*
 * kill - send signal to process
 */

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

main(argc, argv)
char **argv;
{
	register signo, pid, res;
	int errlev;
	extern char *sys_errlist[];


	errlev = 0;
	if (argc <= 1) {
	usage:
		printf("usage: kill [ -signo ] pid ...\n");
		exit(2);
	}
	if (*argv[1] == '-') {
		char *end;long value;
        errno=0;value=strtol(argv[1]+1,&end,10);
        if(!argv[1][1] || *end || errno || value<0 || value>INT_MAX)goto usage;
        signo=value;
		argc--;
		argv++;
	} else
		signo = SIGTERM;
	if(argc<=1)goto usage;
	argv++;
	while (argc > 1) {
		if (**argv<'0' || **argv>'9')
			goto usage;
		{ char *end;long value;errno=0;value=strtol(*argv,&end,10);
          if(*end || errno || value>INT_MAX)goto usage;pid=value; }
        res = kill(pid, signo);
		if (res<0) {
			printf("%u: %s\n", pid, sys_errlist[errno]);
			errlev = 1;
		}
		argc--;
		argv++;
	}
	return(errlev);
}
