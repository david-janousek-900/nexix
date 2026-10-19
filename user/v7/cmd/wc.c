/* wc line and word count */

#include <stdio.h>
#include <stdlib.h>

main(argc, argv)
char **argv;
{
	int i, token;
	int errors = 0;
	char *option;
	register FILE *fp;
	long linect, wordct, charct;
	long tlinect=0, twordct=0, tcharct=0;
	char *wd;
	register int c;

	wd = "lwc";
	if(argc > 1 && *argv[1] == '-') {
		wd = ++argv[1];
		argc--;
		argv++;
	}

	for (option=wd; *option; option++)
		if (*option!='l' && *option!='w' && *option!='c') {
			fprintf(stderr, "wc: unknown option\n"); exit(1);
		}
	i = 1;
	fp = stdin;
	do {
		if(argc>1 && (fp=fopen(argv[i], "r")) == NULL) {
			fprintf(stderr, "wc: can't open %s\n", argv[i]);
			errors = 1;
			continue;
		}
		linect = 0;
		wordct = 0;
		charct = 0;
		token = 0;
		for(;;) {
			c = getc(fp);
			if (c == EOF)
				break;
			charct++;
			if(' '<c&&c<0177) {
				if(!token) {
					wordct++;
					token++;
				}
				continue;
			}
			if(c=='\n')
				linect++;
			else if(c!=' '&&c!='\t')
				continue;
			token = 0;
		}
		if (ferror(fp)) { fprintf(stderr, "wc: read error\n"); errors = 1; }
		/* print lines, words, chars */
		wcp(wd, charct, wordct, linect);
		if(argc>1) {
			printf(" %s\n", argv[i]);
		} else
			printf("\n");
		fclose(fp);
		tlinect += linect;
		twordct += wordct;
		tcharct += charct;
	} while(++i<argc);
	if(argc > 2) {
		wcp(wd, tcharct, twordct, tlinect);
		printf(" total\n");
	}
	if (fflush(stdout)==EOF || ferror(stdout)) errors = 1;
	exit(errors);
}

wcp(wd, charct, wordct, linect)
register char *wd;
long charct; long wordct; long linect;
{
	while (*wd) switch (*wd++) {
	case 'l':
		printf("%7ld", linect);
		break;

	case 'w':
		printf("%7ld ", wordct);
		break;

	case 'c':
		printf("%7ld", charct);
		break;
	}
}
