/*
 * Print horizontally as possible, thus saving paper
 */
#include <stdio.h>
#include <stdlib.h>
int tab[8] = {8,7,6,5,4,3,2,1};
int next;	/*next output position on line*/
int slen;	/*length at start of line*/
int alen;	/*actual length*/
int elen;	/*length on current line*/
/* Grown on demand; V7 used a fixed 256-byte line. */
char *buf;
int bufsize;
getit()
{	register int i;
	register c;
	slen=alen=elen=0;
	for(i=0;;i++)
	{	if(i>=bufsize)
		{	char *grown = bufsize <= 0x3fffffff ? realloc(buf, bufsize ? bufsize*2 : 256) : NULL;
			if(grown==NULL) {fprintf(stderr,"sp: line too long\n"); exit(1);}
			buf=grown;
			bufsize = bufsize ? bufsize*2 : 256;
		}
		buf[i]=c=getchar();
		switch(c)
		{
		case '\n':	if(i==0) continue;
		case EOF:
			alen=i;
			return(i);
		case '\t':
			elen+= tab[(next+elen)%8];
			slen += tab[slen%8];
			continue;
		default:
			elen++;
			slen++;
			continue;
		}
	}
}
putit(ntab)
{	register int i;
	for(i=0;i<ntab;i++) putchar('\t');
	for(i=0;i<alen;i++) putchar(buf[i]);
}
clean()
{
	putchar('\n');
}
main(argc,argv) char *argv[];
{	int len,ntab;
	int i;
	len=80;
	if(argc>1)
	{	i=atoi(argv[1]);
		if(i<0) i= i<-10000 ? 10000 : -i;
		if(i>10000) i=10000;
		len=(i<1?1:i);
	}
	else len=80;
	for(;;)
	{	if(next==0) ntab=0;
		else if(tab[next%8]<1)
			{	ntab=2;
				next+= tab[next%8];
				next += tab[next%8];
			}
			else
			{	ntab=1;
				next += tab[next%8];
			}
		if(getit()<=0)
		{	clean();
			if(ferror(stdin)) {fprintf(stderr,"sp: read error\n"); exit(1);}
			exit(fflush(stdout)==EOF || ferror(stdout));
		}
		if(elen+next>=len)
		{	clean();
			next=slen;
			putit(0);
		}
		else
		{	next += elen;
			putit(ntab);
		}
	}
}
