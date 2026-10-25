/* Native V7 random.c line-selection/exit-value algorithm, with checked buffers.
 * The reference uses MAXINT=32768 and the V7 15-bit rand sequence. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#define MAXINT 32768.0
#ifndef RANDOM_CLOCK
#define RANDOM_CLOCK time
#endif
time_t RANDOM_CLOCK(time_t *);
static float fract=2;
static char ibuf[BUFSIZ],obuf[BUFSIZ];
int main(int argc,char **argv)
{
    int i,rflag=0,eflag=0,c;
    char *end,*line;
    size_t length,capacity=128;
    time_t now;
    for(i=1;i<argc;++i) {
        if(!strcmp(argv[i],"-e"))eflag=1;
        else if(!strcmp(argv[i],"-r"))rflag=1;
        else if(argv[i][0]=='-'){fputs("random: invalid option\n",stderr);return 1;}
        else {
            double value=strtod(argv[i],&end);
            /* Largest float below INT32_MAX avoids an undefined exit-value cast. */
            if(end==argv[i] || *end || !(value>=0) || value>2147483520.0){fputs("random: invalid divisor\n",stderr);return 1;}
            fract=(float)value;
        }
    }
    if(RANDOM_CLOCK(&now)==(time_t)-1){perror("random: time");return 1;}
    srand((unsigned)now);
    if(!rflag&&!eflag){setbuf(stdin,ibuf);setbuf(stdout,obuf);}
    if(!eflag){
        line=malloc(capacity);
        if(!line){perror("random");return 1;}
        for(;;){
            length=0;
            while((c=getchar())!=EOF && c!='\n'){
                if(length==capacity){
                    char *grown;
                    if(capacity>SIZE_MAX/2){free(line);fputs("random: line too long\n",stderr);return 1;}
                    grown=realloc(line,capacity*2);
                    if(!grown){free(line);perror("random");return 1;}
                    line=grown;capacity*=2;
                }
                line[length++]=(char)c;
            }
            if(c==EOF && ferror(stdin)){free(line);fputs("random: read error\n",stderr);return 1;}
            if(c==EOF && !length)break;
            if(rand()/MAXINT*fract<1){
                if(fwrite(line,1,length,stdout)!=length || putchar('\n')==EOF){free(line);return 1;}
            }
            if(c==EOF)break;
        }
        free(line);
    }
    if(fflush(stdout)==EOF)return 1;
    return (int)(rand()/MAXINT*fract);
}
