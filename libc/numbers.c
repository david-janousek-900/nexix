#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>
static int digit(int c)
{
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='z')return c-'a'+10;
    if(c>='A'&&c<='Z')return c-'A'+10;
    return -1;
}
static uint64_t parse(const char *text,char **end,int base,int signed_value,int *negative,int *overflow)
{
    const char *p=text;
    *negative=*overflow=0;
    if(base!=0&&(base<2||base>36)){errno=EINVAL;if(end)*end=(char *)text;return 0;}
    while(isspace((unsigned char)*p))++p;
    if(*p=='+'||*p=='-')*negative=*p++=='-';
    if((base==0||base==16)&&p[0]=='0'&&(p[1]=='x'||p[1]=='X')&&digit((unsigned char)p[2])>=0&&digit((unsigned char)p[2])<16){p+=2;base=16;}
    if(!base)base=*p=='0'?8:10;
    const char *start=p;
    uint64_t value=0,limit=signed_value?(uint64_t)INT64_MAX+(unsigned)*negative:UINT64_MAX;
    int d;
    while((d=digit((unsigned char)*p))>=0&&d<base){
        if(value>(limit-(unsigned)d)/(unsigned)base){*overflow=1;value=limit;}
        else value=value*(unsigned)base+(unsigned)d;
        ++p;
    }
    if(end)*end=(char *)(p==start?text:p);
    if(*overflow)errno=ERANGE;
    return value;
}
long strtol(const char *text,char **end,int base)
{
    int negative,overflow;
    uint64_t value=parse(text,end,base,1,&negative,&overflow);
    (void)overflow;
    return negative?(value==(1ULL<<63)?INT64_MIN:-(long)value):(long)value;
}
unsigned long strtoul(const char *text,char **end,int base)
{
    int negative,overflow;
    uint64_t value=parse(text,end,base,0,&negative,&overflow);
    return negative&&!overflow?0-value:value;
}
long atol(const char *text){return strtol(text,NULL,10);}
int atoi(const char *text)
{
    long value=strtol(text,NULL,10);
    if(value>INT32_MAX){errno=ERANGE;return INT32_MAX;}
    if(value<INT32_MIN){errno=ERANGE;return INT32_MIN;}
    return (int)value;
}
int abs(int value){return value<0?(int)(0-(unsigned)value):value;}
long labs(long value){return value<0?(long)(0-(unsigned long)value):value;}
/* V7 rand.c sequence, with explicit 32-bit unsigned wrap replacing PDP-11
 * long overflow; output remains the historical 15-bit value. */
static uint32_t seed=1;
void srand(unsigned value){seed=value;}
int rand(void){seed=seed*1103515245U+12345;return (seed>>16)&077777;}
