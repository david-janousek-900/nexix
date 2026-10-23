#include "../session.h"
#include "../hostname-file.h"
#include <signal.h>
#include <sys/types.h>
#include <utmp.h>
#include <setjmp.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;
static char *recovery_environment[]={"HOME=/root","PATH=/sbin:/bin:/usr/sbin:/usr/bin","TERM=ansi",0};
static void reset(int);
static void merge_signal(int);
int merge(void);
int configured(void);
int initial_boot = 1;

#define	TABSIZ	100
#define	ALL	p = &itab[0]; p < &itab[TABSIZ]; p++
#define	EVER	;;

char	shell[]	= "/bin/sh";
char	getty[]	 = "/usr/libexec/getty";
char	minus[]	= "-";
char	runc[]	= "/etc/rc";
char	netrc[]	= "/etc/netstart";
char	ifile[]	= "/etc/ttys";
char	utmp[]	= "/etc/utmp";
char	wtmpf[]	= "/usr/adm/wtmp";
char	ctty[]	= "/dev/console";
char	dev[]	= "/dev/";

struct
{
	char	line[8];
	char	comn;
	char	flag;
} line;
struct	tab
{
	char	line[8];
	char	comn;
	int	pid;
	int	failed;
} itab[TABSIZ];

int	fi;
char	tty[20];
jmp_buf	sjbuf;

main()
{
	if (getpid() != 1) {
		fprintf(stderr, "init: must be pid 1\n");
		exit(1);
	}
	/* Load before the first recovery shell or supervised login. */
	{
		char name[NX_UTS_FIELD_SIZE];
		if(read_hostname_file("/etc/hostname",name)==0)
			(void)sethostname(name,strlen(name));
	}
	(void)v7_accounting_reset();
	setjmp(sjbuf);
	signal(SIGHUP, reset);
	for(EVER) {
		shutdown();
		if (!initial_boot || !configured())
			single();
		initial_boot = 0;
		runcom();
		netstart();
		merge();
		multiple();
	}
}

shutdown()
{
	register i;
	register struct tab *p;

	signal(SIGINT, SIG_IGN);
	for(ALL)
		term(p);
	signal(SIGALRM, reset);
	alarm(60);
	for(i=0; i<5; i++)
		kill(-1, SIGKILL);
	while(wait((int *)0) != -1)
		;
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	for(i=0; i<10; i++)
		close(i);
}

single()
{
	register pid;

	pid = fork();
	if(pid == 0) {
/*
		alarm(300);
*/
		signal(SIGHUP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGALRM, SIG_DFL);
		open(ctty, 2);
		dup(0);
		dup(0);
		if (!initial_boot)
			write(1, "init: single-user recovery\n", 27);
		recovery_environment[2]=v7_terminal_environment(0);
		environ=recovery_environment;
		execl(shell, minus, (char *)0);
		exit(0);
	}
	while(wait((int *)0) != pid)
		;
}

runcom()
{
	register pid;

	pid = fork();
	if(pid == 0) {
		open("/", 0);
		dup(0);
		dup(0);
		execl(shell, shell, runc, (char *)0);
		exit(0);
	}
	while(wait((int *)0) != pid)
		;
}

/* Optional network startup (task 429): after /etc/rc, if /etc/netstart
 * exists it is run with sh, output on the console. Without it no interface
 * beyond loopback is configured. init runs it rather than rc, so upgraded
 * disks keep their own rc and still get the hook. */
netstart()
{
	register pid;

	if (access(netrc, 4) < 0)
		return;
	pid = fork();
	if(pid == 0) {
		open("/dev/null", 0);
		open(ctty, 1);
		dup(1);
		execl(shell, shell, netrc, (char *)0);
		exit(0);
	}
	while(wait((int *)0) != pid)
		;
}

multiple()
{
	register struct tab *p;
	register pid;

	for(EVER) {
		pid = wait((int *)0);
		if(pid == -1 && errno == EINTR)
			continue;
		if(pid == -1)
			return;
		for(ALL)
			if(p->pid == pid || p->pid == -1) {
				rmut(p);
				if (p->pid > 1)
					kill(-p->pid, SIGKILL);
				if (pid == p->pid && p->failed)
					p->pid = 0; /* Failed line setup: retry on configuration reload. */
				else
					dfork(p);
			}
	}
}

term(p)
register struct tab *p;
{

	if(p->pid > 0) {
		rmut(p);
		kill(p->pid, SIGKILL);
		if (p->pid > 1)
			kill(-p->pid, SIGKILL);
	}
	p->pid = 0;
	p->line[0] = 0;
}

rline()
{
	register c, i;

	c = get();
	if(c < 0)
		return(0);
	if(c == 0)
		goto bad;
	line.flag = c;
	c = get();
	if(c <= 0)
		goto bad;
	line.comn = c;
	for(i=0; i<8; i++)
		line.line[i] = 0;
	for(i=0; i<7; i++) {
		c = get();
		if(c <= 0)
			break;
		line.line[i] = c;
	}
	while(c > 0)
		c = get();
	/* The kernel selects one console; multi is the explicit legacy test profile.
	 * Keep inactive configured lines from starting duplicate or failed gettys. */
	{
		char *mode = getenv("CONSOLE");
		if (mode && strcmp(mode, "multi") && strcmp(line.line, "console"))
			goto bad;
	}
	maktty(line.line);
	if(access(tty, 06) < 0)
		goto bad;
	return(1);

bad:
	line.flag = '0';
	return(1);
}

maktty(lin)
char *lin;
{
	register i, j;

	for(i=0; dev[i]; i++)
		tty[i] = dev[i];
	for(j=0; lin[j]; j++) {
		tty[i] = lin[j];
		i++;
	}
	tty[i] = 0;
}

get()
{
	char b;

	if(read(fi, &b, 1) != 1)
		return(-1);
	if(b == '\n')
		return(0);
	return(b);
}

merge()
{
	register struct tab *p, *q;
	register i;

	signal(SIGINT, merge_signal);
	fi = open(ifile, 0);
	if(fi < 0)
		return;
	q = &itab[0];
	while(rline()) {
		if(line.flag == '0')
			continue;
		for(ALL) {
			if(p->line[0] != 0)
			for(i=0; i<8; i++)
				if(p->line[i] != line.line[i])
					goto contin;
			if(p >= q) {
				i = p->pid;
				p->pid = q->pid;
				q->pid = i;
				i = p->failed;
				p->failed = q->failed;
				q->failed = i;
				for(i=0; i<8; i++)
					p->line[i] = q->line[i];
				p->comn = q->comn;
				for(i=0; i<8; i++)
					q->line[i] = line.line[i];
				q->comn = line.comn;
				q++;
			}
			break;
		contin:
			;
		}
	}
	close(fi);
	for(; q < &itab[TABSIZ]; q++)
		term(q);
	for(ALL)
		if(p->line[0] != 0 && p->pid == 0)
			dfork(p);
}

dfork(p)
struct tab *p;
{
	register pid;
	int ready[2], n;
	char failed;

	if (pipe(ready) < 0) {
		p->pid = -1;
		return;
	}
	pid = fork();
	if(pid == 0) {
		close(ready[0]);
		if (dup2(ready[1], 19) != 19)
			_exit(126);
		close(ready[1]);
		if (fcntl(19, F_SETFD, FD_CLOEXEC) < 0)
			goto failed;
		signal(SIGHUP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		maktty(p->line);
		if (chown(tty, 0, 0) < 0 || chmod(tty, 0622) < 0 ||
		    open(tty, 2) != 0 || dup(0) != 1 || dup(0) != 2 ||
		    setpgrp() != getpid())
			goto failed;
		tty[0] = p->comn;
		tty[1] = 0;
		execl(getty, minus, tty, (char *)0);
	failed:
		write(19, "f", 1);
		_exit(127);
	}
	close(ready[1]);
	if (pid > 0) {
		do n = read(ready[0], &failed, 1); while(n < 0 && errno == EINTR);
		p->failed = n != 0;
	}
	close(ready[0]);
	p->pid = pid;
}

rmut(p)
register struct tab *p;
{
	(void)v7_logout_record(p->line);
}

static void merge_signal(int n) { (void)n; merge(); }

static void reset(int n)
{
    (void)n;
	initial_boot = 0;
	longjmp(sjbuf, 1);
}

int configured(void)
{
	int enabled = 0;
	fi = open(ifile, 0);
	if (fi < 0)
		return(0);
	while(rline())
		if(line.flag != '0') {
			enabled = 1;
			break;
		}
	close(fi);
	return(enabled);
}
