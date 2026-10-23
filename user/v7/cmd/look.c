#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

FILE *dfile;
char *filenam  = "/usr/dict/words";

int fold;
int dict;
int tab;
/* Grown on demand; V7 used 250-byte entries and a 50-byte key. */
char *entry;
char *word;
char *key;
size_t size;

main(argc,argv)
char **argv;
{
	register c;
	long top,bot,mid;
	while(argc>=2 && *argv[1]=='-') {
		for(;;) {
			switch(*++argv[1]) {
			case 'd':
				dict++;
				continue;
			case 'f':
				fold++;
				continue;
			case 't':
				tab = argv[1][1];
				if(tab)
					++argv[1];
				continue;
			case 0:
				break;
			default:
				continue;
			}
			break;
		}
		argc --;
		argv++;
	}
	if(argc<=1)
		exit(0);
	if(argc==2) {
		fold++;
		dict++;
	} else
		filenam = argv[2];
	dfile = fopen(filenam,"r");
	if(dfile==NULL) {
		fprintf(stderr,"look: can't open %s\n",filenam);
		exit(2);
	}
	if((key = malloc(strlen(argv[1])+1)) == NULL)
		nomem();
	canon(argv[1],key);
	bot = 0;
	fseek(dfile,0L,2);
	top = ftell(dfile);
	for(;;) {
		mid = (top+bot)/2;
		fseek(dfile,mid,0);
		do {
			c = getc(dfile);
			mid++;
		} while(c!=EOF && c!='\n');
		if(!getword(entry))
			break;
		canon(entry,word);
		switch(compare(key,word)) {
		case -2:
		case -1:
		case 0:
			if(top<=mid)
				break;
			top = mid;
			continue;
		case 1:
		case 2:
			bot = mid;
			continue;
		}
		break;
	}
	fseek(dfile,bot,0);
	while(ftell(dfile)<top) {
		if(!getword(entry))
			done();
		canon(entry,word);
		switch(compare(key,word)) {
		case -2:
			done();
		case -1:
		case 0:
			puts(entry);
			break;
		case 1:
		case 2:
			continue;
		}
		break;
	}
	while(getword(entry)) {
		canon(entry,word);
		switch(compare(key,word)) {
		case -1:
		case 0:
			puts(entry);
			continue;
		}
		break;
	}
	done();
}

done()
{
	if(ferror(dfile)) {
		fprintf(stderr,"look: read error\n");
		exit(2);
	}
	exit(fflush(stdout)==EOF || ferror(stdout) ? 2 : 0);
}

nomem()
{
	fprintf(stderr,"look: out of memory\n");
	exit(2);
}

compare(s,t)
register char *s,*t;
{
	for(;*s==*t;s++,t++)
		if(*s==0)
			return(0);
	return(*s==0? -1:
		*t==0? 1:
		*s<*t? -2:
		2);
}

getword(w)
char *w;
{
	register c;
	size_t n = 0;
	for(;;) {
		if(n+1 >= size) {
			size_t grown = size ? size*2 : 256;
			if((entry = realloc(entry, grown)) == NULL ||
			   (word = realloc(word, grown)) == NULL)
				nomem();
			size = grown;
		}
		c = getc(dfile);
		if(c==EOF)
			return(0);
		if(c=='\n')
			break;
		entry[n++] = c;
	}
	entry[n] = 0;
	return(1);
}

canon(old,new)
char *old,*new;
{
	register c;
	for(;;) {
		*new = c = *old++;
		if(c==0||c==tab) {
			*new = 0;
			break;
		}
		if(dict) {
			if(!isalnum(c))
				continue;
		}
		if(fold) {
			if(isupper(c))
				*new += 'a' - 'A';
		}
		new++;
	}
}
