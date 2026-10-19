/*
 * grep -- print lines matching (or not matching) a pattern
 *
 *	status returns:
 *		0 - ok, and some matches
 *		1 - ok, but no matches
 *		2 - some error
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#define BSIZE 512

#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11
#define	CKET	12
#define	CBACK	18

#define	STAR	01

#define	LBSIZE	512
#define	ESIZE	256
#define	NBRA	9

char	expbuf[ESIZE];
long	lnum;
char	linebuf[LBSIZE+1];
char	ybuf[ESIZE];
int	bflag;
int	lflag;
int	nflag;
int	cflag;
int	vflag;
int	nfile;
int	hflag	= 1;
int	sflag;
int	yflag;
int	circf;
long	tln;
int	nsucc;
char	*braslist[NBRA];
char	*braelist[NBRA];
char	bittab[] = {
	1,
	2,
	4,
	8,
	16,
	32,
	64,
	128
};

main(argc, argv)
char **argv;
{
	while (--argc > 0 && (++argv)[0][0]=='-')
		switch (argv[0][1]) {

		case 'y':
			yflag++;
			continue;

		case 'h':
			hflag = 0;
			continue;

		case 's':
			sflag++;
			continue;

		case 'v':
			vflag++;
			continue;

		case 'b':
			bflag++;
			continue;

		case 'l':
			lflag++;
			continue;

		case 'c':
			cflag++;
			continue;

		case 'n':
			nflag++;
			continue;

		case 'e':
			--argc;
			++argv;
			goto out;

		default:
			errexit("grep: unknown flag\n", (char *)NULL);
			continue;
		}
out:
	if (argc<=0)
		exit(2);
	if (yflag) {
		register char *p, *s;
		for (s = ybuf, p = *argv; *p; ) {
			if (*p == '\\') {
				*s++ = *p++;
				if (*p)
					*s++ = *p++;
			} else if (*p == '[') {
				while (*p != '\0' && *p != ']') {
					if (s >= ybuf+ESIZE-2) errexit("grep: argument too long\n", (char *)NULL);
					*s++ = *p++;
				}
			} else if (islower(*p)) {
				*s++ = '[';
				*s++ = toupper(*p);
				*s++ = *p++;
				*s++ = ']';
			} else
				*s++ = *p++;
			if (s >= ybuf+ESIZE-5)
				errexit("grep: argument too long\n", (char *)NULL);
		}
		*s = '\0';
		*argv = ybuf;
	}
	compile(*argv);
	nfile = --argc;
	if (argc<=0) {
		if (lflag)
			exit(1);
		execute((char *)NULL);
	} else while (--argc >= 0) {
		argv++;
		execute(*argv);
	}
	exit(nsucc == 0);
}

compile(astr)
char *astr;
{
	register c;
	register char *ep, *sp;
	char *cstart;
	char *lastep;
	int cclcnt;
	char bracket[NBRA], *bracketp;
	int closed;
	char numbra;
	char neg;
	unsigned closed_mask = 0;

	ep = expbuf;
	sp = astr;
	lastep = 0;
	bracketp = bracket;
	closed = numbra = 0;
	if (*sp == '^') {
		circf++;
		sp++;
	}
	for (;;) {
		if (ep > expbuf + ESIZE - 2)
			goto cerror;
		if ((c = *sp++) != '*')
			lastep = ep;
		switch (c) {

		case '\0':
			if (bracketp != bracket) goto cerror;
			*ep++ = CEOF;
			return;

		case '.':
			*ep++ = CDOT;
			continue;

		case '*':
			if (lastep==0 || *lastep==CBRA || *lastep==CKET)
				goto defchar;
			*lastep |= STAR;
			continue;

		case '$':
			if (*sp != '\0')
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			if(ep > expbuf + ESIZE - 17)
				goto cerror;
			*ep++ = CCL;
			neg = 0;
			if((c = *sp++) == '^') {
				neg = 1;
				c = *sp++;
			}
			cstart = sp;
			do {
				if (c=='\0' || c > 127)
					goto cerror;
				if (c=='-' && sp>cstart && *sp!=']' && *sp && (unsigned char)*sp < 128) {
					for (c = sp[-2]; c<*sp; c++)
						ep[c>>3] |= bittab[c&07];
					sp++;
				}
				ep[c>>3] |= bittab[c&07];
			} while((c = *sp++) != ']');
			if(neg) {
				for(cclcnt = 0; cclcnt < 16; cclcnt++)
					ep[cclcnt] ^= -1;
				ep[0] &= 0376;
			}

			ep += 16;

			continue;

		case '\\':
			if (!(c = *sp++)) goto cerror;
			if(c == '(') {
				if(numbra >= NBRA) {
					goto cerror;
				}
				*bracketp++ = numbra;
				*ep++ = CBRA;
				*ep++ = numbra++;
				continue;
			}
			if(c == ')') {
				if(bracketp <= bracket) {
					goto cerror;
				}
				*ep++ = CKET;
				closed_mask |= 1u << *--bracketp;
				*ep++ = *bracketp;
				closed++;
				continue;
			}

			if(c >= '1' && c <= '9') {
				if (!(closed_mask & (1u << (c -= '1'))))
					goto cerror;
				*ep++ = CBACK;
				*ep++ = c;
				continue;
			}

		defchar:
		default:
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
    cerror:
	errexit("grep: RE error\n", (char *)NULL);
}

execute(file)
char *file;
{
	register char *p1, *p2;
	register c;

	if (file) {
		if (freopen(file, "r", stdin) == NULL)
			errexit("grep: can't open %s\n", file);
	}
	lnum = 0;
	tln = 0;
	for (;;) {
		lnum++;
		p1 = linebuf;
		while ((c = getchar()) != '\n') {
			if (c == EOF) {
				if (ferror(stdin)) errexit("grep: read error\n", (char *)NULL);
				if (cflag) {
					if (nfile>1)
						printf("%s:", file);
					printf("%ld\n", tln);
				}
				return;
			}
			*p1++ = c;
			if (p1 >= &linebuf[LBSIZE-1])
				break;
		}
		*p1++ = '\0';
		p1 = linebuf;
		memset(braslist, 0, sizeof braslist);
		memset(braelist, 0, sizeof braelist);
		p2 = expbuf;
		if (circf) {
			if (advance(p1, p2))
				goto found;
			goto nfound;
		}
		/* fast check for first character */
		if (*p2==CCHR) {
			c = p2[1];
			do {
				if (*p1!=c)
					continue;
				if (advance(p1, p2))
					goto found;
			} while (*p1++);
			goto nfound;
		}
		/* regular algorithm */
		do {
			if (advance(p1, p2))
				goto found;
		} while (*p1++);
	nfound:
		if (vflag)
			succeed(file);
		continue;
	found:
		if (vflag==0)
			succeed(file);
	}
}

/* Retained V7 bytecode; bounded scans never step before the line or past NUL. */
advance(lp, ep)
register char *lp, *ep;
{
    char *start, *end, *bbeg;
    int op, atom, ct, c;
    for (;;) {
        op = (unsigned char)*ep++;
        if (op == CEOF) return 1;
        if (op == CBRA) { braslist[(unsigned char)*ep++] = lp; continue; }
        if (op == CKET) { braelist[(unsigned char)*ep++] = lp; continue; }
        if (op == CDOL) { if (*lp) return 0; continue; }
        atom = op & ~STAR;
        ct = 1;
        bbeg = 0;
        if (atom == CBACK) {
            c = (unsigned char)*ep++;
            bbeg = braslist[c];
            if (!bbeg || !braelist[c] || braelist[c] < bbeg) return 0;
            ct = braelist[c] - bbeg;
            if (!ct) continue;
        }
        start = lp;
        do {
            if (atom == CBACK) {
                if (!ecmp(bbeg, lp, ct)) break;
                lp += ct;
            } else {
                if (!*lp) break;
                if (atom == CCHR && *lp != *ep) break;
                if (atom == CCL && !(ep[(*lp & 127)>>3] & bittab[*lp & 7])) break;
                if (atom != CCHR && atom != CDOT && atom != CCL) errexit("grep RE botch\n", (char *)0);
                lp++;
            }
        } while (op & STAR);
        if (atom == CCHR) ep++;
        if (atom == CCL) ep += 16;
        if (!(op & STAR)) { if (lp == start) return 0; continue; }
        end = lp;
        for (;;) {
            if (advance(end, ep)) return 1;
            if (end == start) return 0;
            end -= ct;
        }
    }
}

succeed(f)
char *f;
{
	long ftell();
	nsucc = 1;
	if (sflag)
		return;
	if (cflag) {
		tln++;
		return;
	}
	if (lflag) {
		printf("%s\n", f);
		fseek(stdin, 0l, 2);
		return;
	}
	if (nfile > 1 && hflag)
		printf("%s:", f);
	if (bflag)
		printf("%ld:", (ftell(stdin)-1)/BSIZE);
	if (nflag)
		printf("%ld:", lnum);
	printf("%s\n", linebuf);
}

ecmp(a, b, count)
char	*a, *b;
{
	register cc = count;
	while(cc--)
		if(!*b || *a++ != *b++)	return(0);
	return(1);
}

errexit(s, f)
char *s, *f;
{
	fprintf(stderr, s, f);
	exit(2);
}
