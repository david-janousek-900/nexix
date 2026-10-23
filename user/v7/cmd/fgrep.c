/*
 * fgrep -- print all lines containing any of a set of keywords
 *
 *	status returns:
 *		0 - ok, and some matches
 *		1 - ok, but no matches
 *		2 - some error
 */

#include <stdio.h>
#include <stdlib.h>

/* Native sizes: V7 limited word lists to 6000 states and a 400-entry queue. */
#define	MAXSIZ 20000
#define QSIZE MAXSIZ
struct words {
	char 	inp;
	char	out;
	struct	words *nst;
	struct	words *link;
	struct	words *fail;
} w[MAXSIZ], *smax, *q;

long	lnum;
int	bflag, cflag, fflag, lflag, nflag, vflag, xflag;
int	hflag	= 1;
int	sflag;
int	nfile;
long	blkno;
int	nsucc;
long	tln;
FILE	*wordf;
char	*argptr;

main(argc, argv)
char **argv;
{
	while (--argc > 0 && (++argv)[0][0]=='-')
		switch (argv[0][1]) {

		case 's':
			sflag++;
			continue;

		case 'h':
			hflag = 0;
			continue;

		case 'b':
			bflag++;
			continue;

		case 'c':
			cflag++;
			continue;

		case 'e':
			argc--;
			argv++;
			goto out;

		case 'f':
			fflag++;
			continue;

		case 'l':
			lflag++;
			continue;

		case 'n':
			nflag++;
			continue;

		case 'v':
			vflag++;
			continue;

		case 'x':
			xflag++;
			continue;

		default:
			fprintf(stderr, "fgrep: unknown flag\n");
			continue;
		}
out:
	if (argc<=0)
		exit(2);
	if (fflag) {
		wordf = fopen(*argv, "r");
		if (wordf==NULL) {
			fprintf(stderr, "fgrep: can't open %s\n", *argv);
			exit(2);
		}
	}
	else argptr = *argv;
	argc--;
	argv++;

	cgotofn();
	cfail();
	nfile = argc;
	if (argc<=0) {
		if (lflag) exit(1);
		execute((char *)NULL);
	}
	else while (--argc >= 0) {
		execute(*argv);
		argv++;
	}
	if (fflush(stdout) == EOF || ferror(stdout))
		exit(2);
	exit(nsucc == 0);
}

/*
 * Lines accumulate in a growing buffer; V7 used a 1024-byte ring
 * that lines longer than the ring overwrote before they were printed.
 */
char	*line;
long	linesize;

execute(file)
char *file;
{
	register struct words *c;
	char buf[BUFSIZ];
	int f, n, ch, i;
	int failed, matched;
	long len;
	char *name = file ? file : "";
	if (file) {
		if ((f = open(file, 0)) < 0) {
			fprintf(stderr, "fgrep: can't open %s\n", file);
			exit(2);
		}
	}
	else f = 0;
	failed = matched = 0;
	len = 0;
	lnum = 1;
	tln = 0;
	blkno = 0;
	c = w;
	for (;;) {
		if ((n = read(f, buf, sizeof buf)) <= 0) {
			if (n < 0) {
				fprintf(stderr, "fgrep: read error on %s\n", file ? file : "standard input");
				exit(2);
			}
			break;
		}
		for (i = 0; i < n; i++) {
			ch = buf[i] & 0377;
			blkno++;
			if (len >= linesize) {
				char *grown = realloc(line, linesize ? linesize*2 : BUFSIZ);
				if (grown == NULL) {
					fprintf(stderr, "fgrep: line too long\n");
					exit(2);
				}
				line = grown;
				linesize = linesize ? linesize*2 : BUFSIZ;
			}
			line[len++] = ch;
			if (!matched) {
				/* A NUL never takes a transition (V7 followed null states). */
			nstate:
				if (ch && c->inp == ch) {
					c = c->nst;
				}
				else if (c->link != 0) {
					c = c->link;
					goto nstate;
				}
				else {
					c = c->fail;
					failed = 1;
					if (c==0) {
						c = w;
					istate:
						if (ch && c->inp == ch) {
							c = c->nst;
						}
						else if (c->link != 0) {
							c = c->link;
							goto istate;
						}
					}
					else goto nstate;
				}
				if (c->out)
					matched = 1;
			}
			if (ch == '\n') {
				if (endline(name, matched, failed, len) && lflag) {
					close(f);
					return;
				}
				len = 0;
				c = w;
				failed = matched = 0;
			}
		}
	}
	/* The final unterminated line is a line, printed as it was read. */
	if (len > 0 && endline(name, matched, failed, len) && lflag) {
		close(f);
		return;
	}
	close(f);
	if (cflag) {
		if (nfile > 1)
			printf("%s:", file);
		printf("%ld\n", tln);
	}
}

/* Decide one complete line as V7 did; returns 1 when it succeeded. */
endline(file, matched, failed, len)
char *file;
long len;
{
	if (matched) {
		if ( (vflag && (failed == 0 || xflag == 0)) || (vflag == 0 && xflag && failed) )
			goto nomatch;
	}
	else if (!vflag)
		goto nomatch;
	nsucc = 1;
	if (cflag) tln++;
	else if (sflag)
		;	/* ugh */
	else if (lflag) {
		printf("%s\n", file);
		lnum++;
		return(1);
	}
	else {
		if (nfile > 1 && hflag) printf("%s:", file);
		/* V7 reported the block before the newline's offset. */
		if (bflag) printf("%ld:", (blkno-2)/512);
		if (nflag) printf("%ld:", lnum);
		fwrite(line, 1, len, stdout);
	}
	lnum++;
	return(1);
nomatch:
	lnum++;
	return(0);
}

getargc()
{
	register c;
	if (wordf)
		return(getc(wordf));
	if ((c = *argptr++) == '\0')
		return(EOF);
	return(c);
}

cgotofn() {
	register c;
	register struct words *s;

	s = smax = w;
nword:	for(;;) {
		c = getargc();
		if (c==EOF)
			return;
		if (c == '\n') {
			if (xflag) {
				for(;;) {
					if (s->inp == c) {
						s = s->nst;
						break;
					}
					if (s->inp == 0) goto nenter;
					if (s->link == 0) {
						if (smax >= &w[MAXSIZ -1]) overflo();
						s->link = ++smax;
						s = smax;
						goto nenter;
					}
					s = s->link;
				}
			}
			s->out = 1;
			s = w;
		} else {
		loop:	if (s->inp == c) {
				s = s->nst;
				continue;
			}
			if (s->inp == 0) goto enter;
			if (s->link == 0) {
				if (smax >= &w[MAXSIZ - 1]) overflo();
				s->link = ++smax;
				s = smax;
				goto enter;
			}
			s = s->link;
			goto loop;
		}
	}

	enter:
	do {
		s->inp = c;
		if (smax >= &w[MAXSIZ - 1]) overflo();
		s->nst = ++smax;
		s = smax;
	} while ((c = getargc()) != '\n' && c!=EOF);
	if (xflag) {
	nenter:	s->inp = '\n';
		if (smax >= &w[MAXSIZ -1]) overflo();
		s->nst = ++smax;
	}
	smax->out = 1;
	s = w;
	if (c != EOF)
		goto nword;
}

overflo() {
	fprintf(stderr, "fgrep: wordlist too large\n");
	exit(2);
}
cfail() {
	static struct words *queue[QSIZE];
	struct words **front, **rear;
	struct words *state;
	register char c;
	register struct words *s;
	s = w;
	front = rear = queue;
init:	if ((s->inp) != 0) {
		*rear++ = s->nst;
		if (rear >= &queue[QSIZE - 1]) overflo();
	}
	if ((s = s->link) != 0) {
		goto init;
	}

	while (rear!=front) {
		s = *front;
		if (front == &queue[QSIZE-1])
			front = queue;
		else front++;
	cloop:	if ((c = s->inp) != 0) {
			*rear = (q = s->nst);
			if (front < rear)
				if (rear >= &queue[QSIZE-1])
					if (front == queue) overflo();
					else rear = queue;
				else rear++;
			else
				if (++rear == front) overflo();
			state = s->fail;
		floop:	if (state == 0) state = w;
			if (state->inp == c) {
				q->fail = state->nst;
				if ((state->nst)->out == 1) q->out = 1;
				continue;
			}
			else if ((state = state->link) != 0)
				goto floop;
		}
		if ((s = s->link) != 0)
			goto cloop;
	}
}
