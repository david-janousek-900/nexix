/* Native equivalent of V7 factor.s: decimal input and repeated prime factors.
 * PDP-11 floating instructions are replaced by exact bounded integer arithmetic.
 * Historical source/license: reference/v7/usr/src/cmd/factor.s. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define LIMIT (UINT64_C(1)<<56)

/* The retained converter ignores spaces and ends a number at another character. */
static int number(FILE *input,const char *text,uint64_t *result)
{
    uint64_t n=0;
    int c,range=0;
    for (;;) {
        c=text ? (unsigned char)*text++ : getc(input);
        if(c==' ')continue;
        if(c<'0'||c>'9')break;
        if(n>(LIMIT-1-(unsigned)(c-'0'))/10)range=1;
        else if(!range)n=n*10+(unsigned)(c-'0');
    }
    if(!text && ferror(input))return -1;
    *result=n;
    return range?-1:n?1:0;
}
static int factors(uint64_t n)
{
    uint64_t d=2;
    unsigned step=2;
    if(putchar('\n')==EOF)return 1;
    while(d<=n/d) {
        while(n%d==0) {
            if(printf("     %lu\n",(unsigned long)d)<0)return 1;
            n/=d;
        }
        if(d==2)d=3;
        else if(d==3)d=5;
        else {d+=step;step=6-step;}
    }
    if(n>1 && printf("     %lu\n",(unsigned long)n)<0)return 1;
    return 0;
}
int main(int argc,char **argv)
{
    uint64_t n;
    int rc,status=0;
    if(argc>2){fputs("usage: factor [number]\n",stderr);return 1;}
    do {
        rc=number(stdin,argc==2?argv[1]:NULL,&n);
        if(rc<0){fputs("Ouch.\n",stderr);status=1;}
        else if(!rc)break;
        else if(factors(n))return 1;
    }while(argc==1);
    return fflush(stdout)==EOF?1:status;
}
