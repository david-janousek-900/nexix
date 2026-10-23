#include <stdio.h>
#include <stdlib.h>

/* reverse lines of a file */

int N = 256;
char *line;
FILE *input;

main(argc,argv)
char **argv;
{
	register i,c;
	int status = 0;
	if((line = malloc(N)) == NULL) {
		fprintf(stderr,"rev: out of memory\n");
		exit(1);
	}
	input = stdin;
	do {
		if(argc>1) {
			if((input=fopen(argv[1],"r"))==NULL) {
				fprintf(stderr,"rev: cannot open %s\n",
					argv[1]);
				exit(1);
			}
		}
		for(;;){
			for(i=0;;i++) {
				if(i==N) {
					char *grown = N <= 0x3fffffff ? realloc(line, N*2) : NULL;
					if(grown == NULL) {
						fprintf(stderr,"rev: line too long\n");
						exit(1);
					}
					line = grown;
					N *= 2;
				}
				line[i] = c = getc(input);
				switch(c) {
				case EOF:
					goto eof;
				default:
					continue;
				case '\n':
					break;
				}
				break;
			}
			while(--i>=0)
				putc(line[i],stdout);
			putc('\n',stdout);
		}
eof:
		/* An unterminated final line is reversed and terminated. */
		if(i>0) {
			while(--i>=0)
				putc(line[i],stdout);
			putc('\n',stdout);
		}
		if(ferror(input)) {
			fprintf(stderr,"rev: read error\n");
			status = 1;
		}
		if(input != stdin)
			fclose(input);
		argc--;
		argv++;
	} while(argc>1);
	if(fflush(stdout)==EOF || ferror(stdout))
		status = 1;
	exit(status);
}
