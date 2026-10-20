#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "mkfont1.c"

/*
  this program takes 102 width values
  (one per line) in the order provided bu Graphic
  Systems and prepares a C-compileable width table.
*/
char ibuf[512];
int id;
int width[102];
int ascii[102];
int zero;
int emw, hyw;
int xxx;

main(argc,argv)
int argc;
char **argv;
{
	register i, j;
	register char *p;

	while((--argc > 0) && ((++argv)[0][0]=='-')){
		switch(argv[0][1]){
			default:
				continue;
		}
	}
	if(argc){
		if((id=open(argv[0],0)) < 0){
			printf("Cannot open: %s.\n",argv[0]);
			exit(1);
		}
	}
    { FILE *input=fdopen(id,"r"); char line[80],*end; long value;
      if(!input)return 1;
      for(i=0;i<102;i++){
        if(!fgets(line,sizeof(line),input)){fputs("mkfont: expected 102 widths\n",stderr);return 1;}
        value=strtol(line,&end,10);
        if(end==line || (*end!='\n' && *end) || value<0 || value>63){fputs("mkfont: invalid width\n",stderr);return 1;}
        width[i]=value;
      }
      fclose(input);
    }

	for(i=0; i<102; i++){
		if(font[i].name < 0177){
			ascii[i] = font[i].name;
		}else{
			for(j=0; chtab[j] != 0; j += 2){
				if(font[i].name == chtab[j])break;
			}
			ascii[i] = chtab[j+1] & 0377;
			if(chtab[j] == (104|(121<<8)))hyw = width[i];
			if(chtab[j] == (101|(109<<8)))emw = width[i];
		}
	}
	printf("char XXw[256-32] = {\t/*XX*/\n");
	for(i=040; i<256; i++){
		if(i == 0377){
			printf("0};\n");
			break;
		}
		if(i == 0177){
			printf("6,\t %s\n",nametab[i-040]);
			continue;
		}
		if(i == 0226){
			printf("3,\t %s\n",nametab[i-040]);
			continue;
		}
		if(i == ' '){
			printf("12,\t %s\n",nametab[i-040]);
			continue;
		}
		if(i == '-'){
			printf("%d,\t %s\n",hyw,nametab[i-040]);
			continue;
		}
		for(j=0; j<102; j++){
			if(ascii[j] == i)break;
		}
		if(j == 102){
			printf("0,");
			zero++;
			if(nametab[i-040]){
				printf("\t %s\n",nametab[i-040]);
				zero = 0;
			}else if(i < 0177){
				printf("\t /*%c*/\n",i);
				zero = 0;
			}
			if(zero && !((i+1)%8)){
				printf("\n");
				zero = 0;
			}
		}else{
			if(zero){
				zero = 0;
				printf("\n");
			}
			printf("%d",width[j]);
			if(font[j].ctval)printf("+0%d00, ",font[j].ctval);
			else printf(",\t ");
			printf("%s\n",nametab[i-040]);
		}
	}
	return ferror(stdout) || fflush(stdout)==EOF;
}
