/*
 * who
 */

#include <stdio.h>
#include <utmp.h>
#include <pwd.h>
struct utmp utmp;
struct passwd *pw;
struct passwd *getpwuid();

char *ttyname(), *rindex(), *ctime(), *strcpy(), *index();
main(argc, argv)
char **argv;
{
	register char *tp, *s;
	register FILE *fi;

	s = "/etc/utmp";
	if(argc == 2)
		s = argv[1];
	if (argc==3) {
		tp = ttyname(0);
		if (tp)
			tp = index(tp+1, '/') + 1;
		else {	/* no tty - use best guess from passwd file */
			pw = getpwuid(getuid());
			strncpy(utmp.ut_name, pw?pw->pw_name: "?", sizeof(utmp.ut_name));
			strcpy(utmp.ut_line, "tty??");
			time(&utmp.ut_time);
			putline();
			exit(0);
		}
	}
	if ((fi = fopen(s, "r")) == NULL) {
		puts("who: cannot open utmp");
		exit(1);
	}
	while (fread((char *)&utmp, sizeof(utmp), 1, fi) == 1) {
		if(argc==3) {
			if (strncmp(utmp.ut_line, tp, sizeof(utmp.ut_line)))
				continue;
#ifdef interdata
			printf("(Interdata) ");
#endif
			putline();
			exit(0);
		}
		if(utmp.ut_name[0] == '\0' && argc==1)
			continue;
		putline();
	}
	if (ferror(fi)) { fclose(fi); return(1); }
	fclose(fi);
	return(0);
}

putline()
{
	register char *cbuf;

	printf("%-8.8s %-8.8s", utmp.ut_name, utmp.ut_line);
	cbuf = ctime(&utmp.ut_time);
	printf("%.12s\n", cbuf ? cbuf+4 : "??? ?? ??:??");
}
