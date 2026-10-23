#include <stdio.h>
#include <stdlib.h>
#

/* diff3 - 3-way differential file comparison*/

/* diff3 [-e] d13 d23 f1 f2 f3 
 *
 * d13 = diff report on f1 vs f3
 * d23 = diff report on f2 vs f3
 * f1, f2, f3 the 3 files
*/

struct  range {int from,to; };
	/* from is first in range of changed lines
	 * to is last+1
	 * from=to=line after point of insertion
	* for added lines
	*/
struct diff {struct range old, new;};

/* de is used to gather editing scripts,
 * that are later spewed out in reverse order.
 * its first element must be all zero
 * the "new" component of de contains line positions
 * or byte positions depending on when you look(!?)
 *
 * The tables and line grow on demand; V7 allowed 200 changes and
 * 255-byte lines, and aborted on longer ones.
*/
struct diff *d13;
struct diff *d23;
struct diff *de;
int nde;
char *line;
int linesize;
FILE *fp[3];
int linct[3] = {0,0,0};
/*	the number of the last-read line in each file
 *	is kept in cline[0-2]
*/
int cline[3];
/*	the latest known correspondence between line
 *	numbers of the 3 files is stored in last[1-3]
*/
int last[4];
int eflag;
int debug  = 0;

main(argc,argv)
char **argv;
{
	register i,m,n;
	if(argc>1 && *argv[1]=='-') {
		switch(argv[1][1]) {
		default:
			eflag = 3;
			break;
		case '3':
			eflag = 2;
			break;
		case 'x':
			eflag = 1;
		}
		argv++;
		argc--;
	}
	if(argc<6) {
		fprintf(stderr,"diff3: arg count\n");
		exit(1);
	}
	m = readin(argv[1],&d13);
	n = readin(argv[2],&d23);
	for(i=0;i<=2;i++)
		if((fp[i] = fopen(argv[i+3],"r")) == NULL) {
			printf("diff3: can't open %s\n",argv[i+3]);
			exit(1);
		}
	merge(m,n);
	if(fflush(stdout) == EOF || ferror(stdout))
		exit(2);
	exit(0);
}

char *more(p, n, size)	/* grow table p to hold n entries of size */
char *p;
{
	if((p = realloc(p, (size_t)n*size)) == NULL) {
		fprintf(stderr,"diff3: out of memory\n");
		exit(2);
	}
	return(p);
}

/*pick up the line numbers of allcahnges from
 * one change file
 * (this puts the numbers in a vector, which is not
 * strictly necessary, since the vector is processed
 * in one sequential pass. The vector could be optimized
 * out of existence)
*/

readin(name,ddp)
char *name;
struct diff **ddp;
{
	register i;
	int a,b,c,d;
	char kind;
	char *p;
	struct diff *dd = 0;
	if((fp[0] = fopen(name,"r")) == NULL) {
		printf("diff3: can't open %s\n",name);
		exit(1);
	}
	for(i=0;;i++) {
		dd = (struct diff *)more((char *)dd, i+2, sizeof *dd);
		if(!getchange(fp[0]))
			break;
		p = line;
		a = b = number(&p);
		if(*p==',') {
			p++;
			b = number(&p);
		}
		kind = *p++;
		c = d = number(&p);
		if(*p==',') {
			p++;
			d = number(&p);
		}
		if(kind=='a')
			a++;
		if(kind=='d')
			c++;
		b++;
		d++;
		dd[i].old.from = a;
		dd[i].old.to = b;
		dd[i].new.from = c;
		dd[i].new.to = d;
	}
	/* the sentinel; V7 read dd[-1] when there were no changes */
	dd[i].old.from = i ? dd[i-1].old.to : 1;
	dd[i].new.from = i ? dd[i-1].new.to : 1;
	fclose(fp[0]);
	*ddp = dd;
	return(i);
}

number(lc)
char **lc;
{
	register nn;
	nn = 0;
	while(digit(**lc)) {
		if(nn > 100000000)
			trouble();
		nn = nn*10 + *(*lc)++ - '0';
	}
	return(nn);
}

digit(c)
{
	return(c>='0'&&c<='9');
}

getchange(b)
FILE *b;
{
	while(getln(b))
		if(digit(line[0]))
			return(1);
	return(0);
}

getln(b)	/* renamed from getline, which libc now defines */
FILE *b;
{
	register i, c;
	for(i=0;;i++) {
		if(i+2 >= linesize) {
			linesize = linesize ? linesize*2 : 256;
			line = more(line, linesize, 1);
		}
		c = getc(b);
		if(c==EOF)
			break;
		line[i] = c;
		if(c=='\n') {
			line[++i] = 0;
			return(i);
		}
	}
	return(0);
}

merge(m1,m2)
{
	register struct diff *d1, *d2, *d3;
	int dup;
	int j;
	int t1,t2;
	d1 = d13;
	d2 = d23;
	j = 0;
	for(;(t1 = d1<d13+m1) | (t2 = d2<d23+m2);) {
		if(debug) {
			printf("%d,%d=%d,%d %d,%d=%d,%d\n",
			d1->old.from,d1->old.to,
			d1->new.from,d1->new.to,
			d2->old.from,d2->old.to,
			d2->new.from,d2->new.to);
		}
/*			first file is different from others*/
		if(!t2||t1&&d1->new.to < d2->new.from) {
/*			stuff peculiar to 1st file */
			if(eflag==0) {
				separate("1");
				change(1,&d1->old,0);
				keep(2,&d1->old,&d1->new);
				change(3,&d1->new,0);
			}
			d1++;
			continue;
		}
/*			second file is different from others*/
		if(!t1||t2&&d2->new.to < d1->new.from) {
			if(eflag==0) {
				separate("2");
				keep(1,&d2->old,&d2->new);
				change(2,&d2->old,0);
				change(3,&d2->new,0);
			}
			d2++;
			continue;
		}
/*			merge overlapping changes in first file
 *			this happens after extension see below*/
		if(d1+1<d13+m1 &&
		   d1->new.to>=d1[1].new.from) {
			d1[1].old.from = d1->old.from;
			d1[1].new.from = d1->new.from;
			d1++;
			continue;
		}
/*			merge overlapping changes in second*/
		if(d2+1<d23+m2 &&
		   d2->new.to>=d2[1].new.from) {
			d2[1].old.from = d2->old.from;
			d2[1].new.from = d2->new.from;
			d2++;
			continue;
		}
/*			stuff peculiar to third file or different in all*/
		if(d1->new.from==d2->new.from&&
		   d1->new.to==d2->new.to) {
			dup = duplicate(&d1->old,&d2->old);
/*				dup=0 means all files differ
 *				dup =1 meands files 1&2 identical*/
			if(eflag==0) {
				separate(dup?"3":"");
				change(1,&d1->old,dup);
				change(2,&d2->old,0);
				d3 = d1->old.to>d1->old.from?d1:d2;
				change(3,&d3->new,0);
			} else
				j = edit(d1,dup,j);
			d1++;
			d2++;
			continue;
		}
/*			overlapping changes from file1 & 2
 *			extend changes appropriately to
 *			make them coincide*/
		 if(d1->new.from<d2->new.from) {
			d2->old.from -= d2->new.from-d1->new.from;
			d2->new.from = d1->new.from;
		}
		else if(d2->new.from<d1->new.from) {
			d1->old.from -= d1->new.from-d2->new.from;
			d1->new.from = d2->new.from;
		}
		if(d1->new.to >d2->new.to) {
			d2->old.to += d1->new.to - d2->new.to;
			d2->new.to = d1->new.to;
		}
		else if(d2->new.to >d1->new.to) {
			d1->old.to += d2->new.to - d1->new.to;
			d1->new.to = d2->new.to;
		}
	}
	if(eflag)
		edscript(j);
}

separate(s)
char *s;
{
	printf("====%s\n",s);
}

/*	the range of ines rold.from thru rold.to in file i
 *	is to be changed. it is to be printed only if
 *	it does not duplicate something to be printed later
*/
change(i,rold,dup)
struct range *rold;
{
	printf("%d:",i);
	last[i] = rold->to;
	prange(rold);
	if(dup)
		return;
	if(debug)
		return;
	i--;
	skip(i,rold->from,(char *)0);
	skip(i,rold->to,"  ");
}

/*	print the range of line numbers, rold.from  thru rold.to
 *	as n1,n2 or n1
*/
prange(rold)
struct range *rold;
{
	if(rold->to<=rold->from)
		printf("%da\n",rold->from-1);
	else {
		printf("%d",rold->from);
		if(rold->to > rold->from+1)
			printf(",%d",rold->to-1);
		printf("c\n");
	}
}

/*	no difference was reported by diff between file 1(or 2)
 *	and file 3, and an artificial dummy difference (trange)
 *	must be ginned up to correspond to the change reported
 *	in the other file
*/
keep(i,rold,rnew)
struct range *rold, *rnew;
{
	register delta;
	struct range trange;
	delta = last[3] - last[i];
	trange.from = rnew->from - delta;
	trange.to = rnew->to - delta;
	change(i,&trange,1);
}

/*	skip to just befor line number from in file i
 *	if "pr" is nonzero, print all skipped stuff
 * w	with string pr as a prefix
*/
skip(i,from,pr)
char *pr;
{
	register j,n;
	for(n=0;cline[i]<from-1;n+=j) {
		if((j=getln(fp[i]))==0)
			trouble();
		if(pr)
			printf("%s%s",pr,line);
		cline[i]++;
	}
	return(n);
}

/*	return 1 or 0 according as the old range
 *	(in file 1) contains exactly the same data
 *	as the new range (in file 2)
*/
duplicate(r1,r2)
struct range *r1, *r2;
{
	register c,d;
	register nchar;
	int nline;
	if(r1->to-r1->from != r2->to-r2->from)
		return(0);
	skip(0,r1->from,(char *)0);
	skip(1,r2->from,(char *)0);
	nchar = 0;
	for(nline=0;nline<r1->to-r1->from;nline++) {
		do {
			c = getc(fp[0]);
			d = getc(fp[1]);
			if(c== -1||d== -1)
				trouble();
			nchar++;
			if(c!=d) {
				repos(nchar);
				return(0);	/* V7 returned no value */
			}
		} while(c!= '\n');
	}
	repos(nchar);
	return(1);
}

repos(nchar)
{
	register i;
	for(i=0;i<2;i++) 
		fseek(fp[i], (long)-nchar, 1);
}

trouble()
{
	fprintf(stderr,"diff3: logic error\n");
	exit(2);	/* V7 aborted with a core image */
}

/*	collect an editing script for later regurgitation
*/
edit(diff,dup,j)
struct diff *diff;
{
	if(((dup+1)&eflag)==0)
		return(j);
	j++;
	if(j+1 > nde) {
		nde = nde ? nde*2 : 64;
		de = (struct diff *)more((char *)de, nde, sizeof *de);
		if(j == 1)
			de[0].old.from = de[0].old.to = de[0].new.from = de[0].new.to = 0;
	}
	de[j].old.from = diff->old.from;
	de[j].old.to = diff->old.to;
	de[j].new.from = de[j-1].new.to
	    +skip(2,diff->new.from,(char *)0);
	de[j].new.to = de[j].new.from
	    +skip(2,diff->new.to,(char *)0);
	return(j);
}

/*		regurgitate */
edscript(n)
{
	register j,k;
	char block[512];
	for(n=n;n>0;n--) {
		prange(&de[n].old);
		fseek(fp[2], (long)de[n].new.from, 0);
		for(k=de[n].new.to-de[n].new.from;k>0;k-= j) {
			j = k>512?512:k;
			if(fread(block,1,j,fp[2])!=j)
				trouble();
			fwrite(block, 1, j, stdout);
		}
		printf(".\n");
	}
}
