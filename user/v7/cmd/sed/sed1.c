#include	<stdio.h>
#include "sed.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

char	*trans[040]  = {
	"\\01",
	"\\02",
	"\\03",
	"\\04",
	"\\05",
	"\\06",
	"\\07",
	"<-",
	">-",
	"\n",
	"\\13",
	"\\14",
	"\\15",
	"\\16",
	"\\17",
	"\\20",
	"\\21",
	"\\22",
	"\\23",
	"\\24",
	"\\25",
	"\\26",
	"\\27",
	"\\30",
	"\\31",
	"\\32",
	"\\33",
	"\\34",
	"\\35",
	"\\36",
	"\\37"
};
char	rub[] = {"\177"};

execute(file)
char *file;
{
	register char *p1, *p2;
	register union reptr	*ipc;
	int	c;
	char	*execp;

	if (file) {
		if ((f = open(file, 0)) < 0) {
			fprintf(stderr, "Can't open %s\n", file);
			status = 2;
		}
	} else
		f = 0;

	ebp = ibuf;
	cbp = ibuf;

	if(pending) {
		ipc = pending;
		pending = 0;
		goto yes;
	}

	for(;;) {
		if((execp = gline(linebuf)) == badp) {
			if(f >= 0)
				close(f);
			f = -1;
			return;
		}
		spend = execp;

		for(ipc = ptrspace; ipc->command; ) {

			p1 = ipc->ad1;
			p2 = ipc->ad2;

			if(p1) {

				if(ipc->inar) {
					if(*p2 == CEND) {
						p1 = 0;
					} else if(*p2 == CLNUM) {
						c = p2[1];
						if(lnum > tlno[c]) {
							ipc->inar = 0;
							if(ipc->negfl)
								goto yes;
							ipc++;
							continue;
						}
						if(lnum == tlno[c]) {
							ipc->inar = 0;
						}
					} else if(match(p2, 0)) {
						ipc->inar = 0;
					}
				} else if(*p1 == CEND) {
					if(!dolflag) {
						if(ipc->negfl)
							goto yes;
						ipc++;
						continue;
					}

				} else if(*p1 == CLNUM) {
					c = p1[1];
					if(lnum != tlno[c]) {
						if(ipc->negfl)
							goto yes;
						ipc++;
						continue;
					}
					if(p2)
						ipc->inar = 1;
				} else if(match(p1, 0)) {
					if(p2)
						ipc->inar = 1;
				} else {
					if(ipc->negfl)
						goto yes;
					ipc++;
					continue;
				}
			}

			if(ipc->negfl) {
				ipc++;
				continue;
			}
	yes:
			command(ipc);

			if(delflag)
				break;

			if(jflag) {
				jflag = 0;
				if((ipc = ipc->lb1) == 0) {
					ipc = ptrspace;
					break;
				}
			} else
				ipc++;

		}
		if(!nflag && !delflag) {
			for(p1 = linebuf; p1 < spend; p1++)
				putc(*p1, stdout);
			putc('\n', stdout);
		}

		if(aptr > abuf) {
			arout();
		}

		delflag = 0;

	}
}
match(expbuf, gf)
char	*expbuf;
{
	register char	*p1, *p2, c;

	if(gf) {
		if(*expbuf)	return(0);
		p1 = linebuf;
		p2 = genbuf;
		while(*p1++ = *p2++);
		locs = p1 = loc2;
	} else {
		p1 = linebuf;
		locs = 0;
	}

	p2 = expbuf;
	if(*p2++) {
		loc1 = p1;
		if(*p2 == CCHR && p2[1] != *p1)
			return(0);
		return(advance(p1, p2));
	}

	/* fast check for first character */

	if(*p2 == CCHR) {
		c = p2[1];
		do {
			if(*p1 != c)
				continue;
			if(advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while(*p1++);
		return(0);
	}

	do {
		if(advance(p1, p2)) {
			loc1 = p1;
			return(1);
		}
	} while(*p1++);
	return(0);
}
advance(alp, aep)
char	*alp, *aep;
{
	register char *lp, *ep, *curlp;
	char	c;
	char *bbeg;
	int	ct;

/*fprintf(stderr, "*lp = %c, %o\n*ep = %c, %o\n", *lp, *lp, *ep, *ep);	/*DEBUG*/

	lp = alp;
	ep = aep;
	for (;;) switch (*ep++) {

	case CCHR:
		if (*ep++ == *lp++)
			continue;
		return(0);

	case CDOT:
		if (*lp++)
			continue;
		return(0);

	case CNL:
	case CDOL:
		if (*lp == 0)
			continue;
		return(0);

	case CEOF:
		loc2 = lp;
		return(1);

	case CCL:
		c = *lp++;	/* all eight bits (V7: & 0177) */
		if(ep[c>>3] & bittab[c & 07]) {
			ep += CCLSIZE;
			continue;
		}
		return(0);

	case CBRA:
		braslist[*ep++] = lp;
		continue;

	case CKET:
		braelist[*ep++] = lp;
		continue;

	case CBACK:
		bbeg = braslist[*ep];
		ct = braelist[*ep++] - bbeg;

		if(ecmp(bbeg, lp, ct)) {
			lp += ct;
			continue;
		}
		return(0);

	case CBACK|STAR:
		bbeg = braslist[*ep];
		ct = braelist[*ep++] - bbeg;
		curlp = lp;
		if(ct == 0)	/* an empty group: V7 looped forever */
			continue;
		while(ecmp(bbeg, lp, ct))
			lp += ct;

		while(lp >= curlp) {
			if(advance(lp, ep))	return(1);
			lp -= ct;
		}
		return(0);


	case CDOT|STAR:
		curlp = lp;
		while (*lp++);
		goto star;

	case CCHR|STAR:
		curlp = lp;
		while (*lp++ == *ep);
		ep++;
		goto star;

	case CCL|STAR:
		curlp = lp;
		do {
			c = *lp++;
		} while(c && ep[c>>3] & bittab[c & 07]);
		ep += CCLSIZE;
		goto star;

	star:
		if(--lp == curlp) {
			continue;
		}

		if(*ep == CCHR) {
			c = ep[1];
			do {
				if(*lp != c)
					continue;
				if(advance(lp, ep))
					return(1);
			} while(lp-- > curlp);
			return(0);
		}

		if(*ep == CBACK) {
			c = *(braslist[ep[1]]);
			do {
				if(*lp != c)
					continue;
				if(advance(lp, ep))
					return(1);
			} while(lp-- > curlp);
			return(0);
		}

		do {
			if(lp == locs)	break;
			if (advance(lp, ep))
				return(1);
		} while (lp-- > curlp);
		return(0);

	default:
		fprintf(stderr, "RE botch, %o\n", *--ep);
		return(0);	/* V7 kept interpreting */
	}
}
substitute(ipc)
union reptr	*ipc;
{
	char	*prev;

	if(match(ipc->re1, 0) == 0)	return(0);
	truncated = 0;

	sflag = 1;
	dosub(ipc->rhs);

	if(ipc->gfl) {
		prev = loc2;
		while(*loc2) {
			if(match(ipc->re1, 1) == 0) break;
			/*
			 * An empty match where the previous one ended is skipped
			 * by one byte; V7 substituted it again forever.
			 */
			if(loc1 == loc2 && loc1 == prev) {
				loc2++;
				continue;
			}
			dosub(ipc->rhs);
			prev = loc2;
		}
	}
	return(1);
}

/*
 * Replacement text is truncated at the line size with one diagnostic per
 * command; V7 reported each excess byte and kept writing past genbuf.
 */
char	*glimit = &genbuf[LBSIZE];

dosub(rhsbuf)
char	*rhsbuf;
{
	register char *lp, *sp, *rp;
	int c;

	lp = linebuf;
	sp = genbuf;
	rp = rhsbuf;
	while (lp < loc1)
		*sp++ = *lp++;
	while(c = *rp++) {
		if (c == '&') {
			sp = place(sp, loc1, loc2);
			continue;
		} else if (c == '\\') {
			c = *rp++;
			if (c >= '1' && c < NBRA+'1') {
				sp = place(sp, braslist[c-'1'], braelist[c-'1']);
				continue;
			}
		}
		sp = place1(sp, c);
	}
	lp = loc2;
	loc2 = sp - genbuf + linebuf;
	while (*lp)
		sp = place1(sp, *lp++);
	*sp = 0;
	lp = linebuf;
	sp = genbuf;
	while (*lp++ = *sp++);
	spend = lp-1;
}

char	*place1(sp, c)
char	*sp;
{
	if (sp >= glimit) {
		if (!truncated++)
			fprintf(stderr, "Output line too long.\n");
		return(sp);
	}
	*sp++ = c;
	return(sp);
}

char	*place(asp, al1, al2)
char	*asp, *al1, *al2;
{
	register char *sp, *l1, *l2;

	sp = asp;
	l1 = al1;
	l2 = al2;
	while (l1 < l2)
		sp = place1(sp, *l1++);
	return(sp);
}

command(ipc)
union reptr	*ipc;
{
	register int	i;
	register char	*p1, *p2, *p3;
	char	*execp;


	switch(ipc->command) {

		case ACOM:
			if(aptr >= &abuf[ABUFSIZE-1]) {
				fprintf(stderr, "Too many appends after line %ld\n",
					lnum);
				break;	/* V7 kept storing past abuf */
			}
			*aptr++ = ipc;
			*aptr = 0;
			break;

		case CCOM:
			delflag = 1;
			if(!ipc->inar || dolflag) {
				for(p1 = ipc->re1; *p1; )
					putc(*p1++, stdout);
				putc('\n', stdout);
			}
			break;
		case DCOM:
			delflag++;
			break;
		case CDCOM:
			p1 = p2 = linebuf;

			while(*p1 != '\n') {
				if(*p1++ == 0) {
					delflag++;
					return;
				}
			}

			p1++;
			while(*p2++ = *p1++);
			spend = p2-1;
			jflag++;
			break;

		case EQCOM:
			fprintf(stdout, "%ld\n", lnum);
			break;

		case GCOM:
			p1 = linebuf;
			p2 = holdsp;
			while(*p1++ = *p2++);
			spend = p1-1;
			break;

		case CGCOM:	/* bounded and terminated; V7 could do neither */
			if(spend < lbend)
				*spend++ = '\n';
			p1 = spend;
			p2 = holdsp;
			while(p1 < lbend && (*p1 = *p2++))
				p1++;
			*p1 = 0;
			spend = p1;
			break;

		case HCOM:
			p1 = holdsp;
			p2 = linebuf;
			while(*p1++ = *p2++);
			hspend = p1-1;
			break;

		case CHCOM:
			if(hspend < hend)
				*hspend++ = '\n';
			p1 = hspend;
			p2 = linebuf;
			while(p1 < hend && (*p1 = *p2++))
				p1++;
			*p1 = 0;
			hspend = p1;
			break;

		case ICOM:
			for(p1 = ipc->re1; *p1; )
				putc(*p1++, stdout);
			putc('\n', stdout);
			break;

		case BCOM:
			jflag = 1;
			break;

		case LCOM:
			p1 = linebuf;
			p2 = genbuf;
			genbuf[72] = 0;
			while(*p1)
				if(*p1 >= 040) {
					if(*p1 == 0177) {
						p3 = rub;
						while(*p2++ = *p3++)
							if(p2 >= lcomend) {
								*p2 = '\\';
								fprintf(stdout, "%s\n", genbuf);
								p2 = genbuf;
							}
						p2--;
						p1++;
						continue;
					}
					*p2++ = *p1++;
					if(p2 >= lcomend) {
						*p2 = '\\';
						fprintf(stdout, "%s\n", genbuf);
						p2 = genbuf;
					}
				} else {
					p3 = trans[*p1-1];
					while(*p2++ = *p3++)
						if(p2 >= lcomend) {
							*p2 = '\\';
							fprintf(stdout, "%s\n", genbuf);
							p2 = genbuf;
						}
					p2--;
					p1++;
				}
			*p2 = 0;
			fprintf(stdout, "%s\n", genbuf);
			break;

		case NCOM:
			if(!nflag) {
				for(p1 = linebuf; p1 < spend; p1++)
					putc(*p1, stdout);
				putc('\n', stdout);
			}

			if(aptr > abuf)
				arout();
			if((execp = gline(linebuf)) == badp) {
				pending = ipc;
				delflag = 1;
				break;
			}
			spend = execp;

			break;
		case CNCOM:
			if(aptr > abuf)
				arout();
			if(spend < lbend)	/* V7 wrote past a full buffer */
				*spend++ = '\n';
			if((execp = gline(spend)) == badp) {
				pending = ipc;
				delflag = 1;
				break;
			}
			spend = execp;
			break;

		case PCOM:
			for(p1 = linebuf; p1 < spend; p1++)
				putc(*p1, stdout);
			putc('\n', stdout);
			break;
		case CPCOM:
	cpcom:
			for(p1 = linebuf; *p1 != '\n' && *p1 != '\0'; )
				putc(*p1++, stdout);
			putc('\n', stdout);
			break;

		case QCOM:
			if(!nflag) {
				for(p1 = linebuf; p1 < spend; p1++)
					putc(*p1, stdout);
				putc('\n', stdout);
			}
			if(aptr > abuf)	arout();
			if(fclose(stdout) == EOF)
				exit(2);
			exit(status);
		case RCOM:

			if(aptr >= &abuf[ABUFSIZE-1]) {
				fprintf(stderr, "Too many reads after line%ld\n",
					lnum);
				break;
			}
			*aptr++ = ipc;
			*aptr = 0;

			break;

		case SCOM:
			i = substitute(ipc);
			if(ipc->pfl && i)
				if(ipc->pfl == 1) {
					for(p1 = linebuf; p1 < spend; p1++)
						putc(*p1, stdout);
					putc('\n', stdout);
				}
				else
					goto cpcom;
			if(i && ipc->fcode)
				goto wcom;
			break;

		case TCOM:
			if(sflag == 0)	break;
			sflag = 0;
			jflag = 1;
			break;

		wcom:
		case WCOM:
			fprintf(ipc->fcode, "%s\n", linebuf);
			break;
		case XCOM:
			p1 = linebuf;
			p2 = genbuf;
			while(*p2++ = *p1++);
			p1 = holdsp;
			p2 = linebuf;
			while(*p2++ = *p1++);
			spend = p2 - 1;
			p1 = genbuf;
			p2 = holdsp;
			while(*p2++ = *p1++);
			hspend = p2 - 1;
			break;

		case YCOM:
			p1 = linebuf;
			p2 = ipc->re1;
			while(*p1 = p2[*p1])	p1++;
			break;
	}

}

char	*
gline(addr)
char	*addr;
{
	register char	*p1, *p2;
	register	c;
	p1 = addr;
	p2 = cbp;
	for (;;) {
		if (p2 >= ebp) {
			if (f < 0 || (c = read(f, ibuf, 512)) <= 0) {
				/* A final unterminated line is a line (V7 dropped it). */
				if (p1 == addr || f < 0)
					return(badp);
				close(f);
				f = -1;
				if(eargc == 0)
					dolflag = 1;
				p2 = ebp = ibuf;
				break;
			}
			p2 = ibuf;
			ebp = ibuf+c;
		}
		if ((c = *p2++) == '\n') {
			if(p2 >=  ebp) {
				if((c = read(f, ibuf, 512)) <= 0) {
					close(f);
					f = -1;
					if(eargc == 0)
							dolflag = 1;
					c = 0;
				}

				p2 = ibuf;
				ebp = ibuf + c;
			}
			break;
		}
		if(c)
		if(p1 < lbend)
			*p1++ = c;
	}
	lnum++;
	*p1 = 0;
	cbp = p2;

	return(p1);
}

ecmp(a, b, count)
char	*a, *b;
{
	while(count--)
		if(*a++ != *b++)	return(0);
	return(1);
}

arout()
{
	register char	*p1;
	FILE	*fi;
	char	c;
	int	t;

	aptr = abuf - 1;
	while(*++aptr) {
		if((*aptr)->command == ACOM) {
			for(p1 = (*aptr)->re1; *p1; )
				putc(*p1++, stdout);
			putc('\n', stdout);
		} else {
			if((fi = fopen((*aptr)->re1, "r")) == NULL)
				continue;
			while((t = getc(fi)) != EOF) {
				c = t;
				putc(c, stdout);
			}
			fclose(fi);
		}
	}
	aptr = abuf;
	*aptr = 0;
}

