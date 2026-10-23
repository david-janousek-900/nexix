#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long count = 1000;
int	fnumber;
char	fname[100];
char	*ifil;
char	*ofil;
FILE	*is;
FILE	*os;

usage()
{
	fprintf(stderr,"usage: split [-n] [file [name]]\n");
	exit(1);
}

closeout()
{
	if(fclose(os) == EOF) {
		fprintf(stderr,"Cannot write output\n");
		exit(1);
	}
}

main(argc, argv)
char *argv[];
{
	register i, c, f;
	unsigned long n;
	int iflg = 0;
	char *s;

	for(i=1; i<argc; i++)
		if(argv[i][0] == '-')
			switch(argv[i][1]) {
		
			case '\0':
				iflg = 1;
				continue;
		
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				/* Checked native count; zero would never advance. */
				count = 0;
				for(s=argv[i]+1; *s>='0' && *s<='9'; s++) {
					if(count > (0xffffffffUL - (*s-'0'))/10)
						usage();
					count = count*10 + *s-'0';
				}
				if(*s || count == 0)
					usage();
				continue;
			default:
				usage();
			}
		else if(iflg)
			ofil = argv[i];
		else {
			ifil = argv[i];
			iflg = 2;
		}
	if(iflg != 2)
		is = stdin;
	else
		if((is=fopen(ifil,"r")) == NULL) {
			fprintf(stderr,"cannot open input\n");
			exit(1);
		}
	if(ofil == 0)
		ofil = "x";
	if(strlen(ofil) > sizeof fname - 3) {
		fprintf(stderr,"split: name too long\n");
		exit(1);
	}

loop:
	f = 1;
	for(n=0; n<count; n++)
	do {
		c = getc(is);
		if(c == EOF) {
			if(f == 0)
				closeout();
			if(ferror(is)) {
				fprintf(stderr,"split: read error\n");
				exit(1);
			}
			exit(0);
		}
		if(f) {
			/* Two suffix letters name at most 676 pieces. */
			if(fnumber >= 26*26) {
				fprintf(stderr,"split: too many output files\n");
				exit(1);
			}
			for(f=0; ofil[f]; f++)
				fname[f] = ofil[f];
			fname[f++] = fnumber/26 + 'a';
			fname[f++] = fnumber%26 + 'a';
			fname[f] = '\0';
			fnumber++;
			if((os=fopen(fname,"w")) == NULL) {
				fprintf(stderr,"Cannot create output\n");
				exit(1);
			}
			f = 0;
		}
		putc(c, os);
	} while(c != '\n');
	closeout();
	goto loop;
}
