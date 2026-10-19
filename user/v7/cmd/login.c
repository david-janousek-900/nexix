/*
 * login [ name ]
 */

#include "../session.h"
#include <sys/types.h>
#include <sgtty.h>
#include <utmp.h>
#include <signal.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#define SCPYN(a, b)	strncpy(a, b, sizeof(a))

char	maildir[30] =	"/usr/spool/mail/";
struct	passwd nouser = {"", "nope"};
struct	sgttyb ttyb;
struct	utmp utmp;
char	minusnam[16] = "-";
char	homedir[64] = "HOME=";
char	*envinit[] = {homedir, "PATH=:/bin:/usr/bin", 0};
struct	passwd *pwd;

struct	passwd *getpwnam();
char	*strcat();
char	*ttyname();
char	*crypt();
char	*getpass();
char	*rindex(), *index();
extern	char **environ;
static void catch(int);
char loginname[9];

main(argc, argv)
char **argv;
{
	register char *namep;
	int t, c;
	char *ttyn;

	alarm(60);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	nice(-40); /* Native nice accepts bounded increments; reset to -20. */
	nice(20);
	nice(0);
	gtty(0, &ttyb);
	ttyn = ttyname(0);
	/* Preserve getty/stty erase and kill settings, including /dev/console. */
	stty(0, &ttyb);
	for (t=3; t<20; t++)
		close(t);
	ttyn = ttyname(0);
	if (ttyn==0)
		ttyn = "/dev/tty??";

    loop:
	SCPYN(utmp.ut_name, "");
	if (argc>1) {
		SCPYN(utmp.ut_name, argv[1]);
		argc = 0;
	}
	while (utmp.ut_name[0] == '\0') {
		namep = utmp.ut_name;
		printf("login: ");
		while ((c = getchar()) != '\n') {
			if(c == ' ')
				c = '_';
			if (c == EOF)
				exit(0);
			if (namep < utmp.ut_name+8)
				*namep++ = c;
		}
	}
	setpwent();
	memcpy(loginname, utmp.ut_name, 8);
	loginname[8] = 0;
	if ((pwd = getpwnam(loginname)) == NULL)
		pwd = &nouser;
	endpwent();
	if (*pwd->pw_passwd != '\0') {
		namep = getpass("Password:");
		if (namep == NULL)
			exit(1);
		namep = crypt(namep,pwd->pw_passwd);
		if (namep == NULL || strcmp(namep, pwd->pw_passwd)) {
			printf("Login incorrect\n");
			goto loop;
		}
	}
	if(chdir(pwd->pw_dir) < 0) {
		printf("No directory\n");
		goto loop;
	}
	(void)v7_login_record(utmp.ut_name, ttyn, ttyslot());
	if (chown(ttyn, pwd->pw_uid, pwd->pw_gid) < 0 ||
	    setgid(pwd->pw_gid) < 0 || setuid(pwd->pw_uid) < 0) {
		printf("Cannot set credentials\n");
		exit(1);
	}
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = "/bin/sh";
	environ = envinit;
	strncat(homedir, pwd->pw_dir, sizeof(homedir)-6);
	if ((namep = rindex(pwd->pw_shell, '/')) == NULL)
		namep = pwd->pw_shell;
	else
		namep++;
	strcat(minusnam, namep);
	alarm(0);
	umask(02);
	showmotd();
	strcat(maildir, pwd->pw_name);
	if(access(maildir,4)==0) {
		struct stat statb;
		stat(maildir, &statb);
		if (statb.st_size)
			printf("You have mail.\n");
	}
	signal(SIGQUIT, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	execlp(pwd->pw_shell, minusnam, (char *)0);
	printf("No shell\n");
	exit(1);
}

int	stopmotd;
static void catch(int number)
{
	(void)number;
	signal(SIGINT, SIG_IGN);
	stopmotd++;
}

showmotd()
{
	FILE *mf;
	register c;

	signal(SIGINT, catch);
	if((mf = fopen("/etc/motd","r")) != NULL) {
		while((c = getc(mf)) != EOF && stopmotd == 0)
			putchar(c);
		fclose(mf);
	}
	signal(SIGINT, SIG_IGN);
}
