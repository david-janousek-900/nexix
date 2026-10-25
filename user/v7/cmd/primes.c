/* Native equivalent of V7 primes.s. Decimal starts retain the 0 < n < 2^56
 * domain. Exact bounded arithmetic replaces PDP-11 floating instructions.
 * First twelve prime witnesses suffice throughout this domain; see
 * Sorenson/Webster, https://arxiv.org/abs/1509.00864, Theorem 1.1. */
#include <stdio.h>
#include <stdint.h>
#define LIMIT (UINT64_C(1)<<56)
static uint64_t mulmod(uint64_t a,uint64_t b,uint64_t modulus)
{
    uint64_t result=0;
    if(!b)return 0;
    if(a<=UINT64_MAX/b)return a*b%modulus;
    /* All operands are below 2^56: these additions cannot overflow uint64_t. */
    while(b){if(b&1)result=(result+a)%modulus;a=(a+a)%modulus;b>>=1;}
    return result;
}
static uint64_t power(uint64_t a,uint64_t exponent,uint64_t modulus)
{
    uint64_t result=1;
    while(exponent){if(exponent&1)result=mulmod(result,a,modulus);a=mulmod(a,a,modulus);exponent>>=1;}
    return result;
}
static int prime(uint64_t n)
{
    static const unsigned bases[]={2,3,5,7,11,13,17,19,23,29,31,37};
    uint64_t d,x;
    unsigned s=0,i,j;
    if(n<2||n>=LIMIT)return 0;
    for(i=0;i<sizeof(bases)/sizeof(bases[0]);++i){if(n==bases[i])return 1;if(n%bases[i]==0)return 0;}
    d=n-1;while(!(d&1)){d>>=1;++s;}
    for(i=0;i<sizeof(bases)/sizeof(bases[0]);++i){
        x=power(bases[i],d,n);
        if(x==1||x==n-1)continue;
        for(j=1;j<s;++j){x=mulmod(x,x,n);if(x==n-1)break;}
        if(j==s)return 0;
    }
    return 1;
}
static int start(FILE *input,const char *text,uint64_t *out)
{
    uint64_t n=0;
    int c,range=0;
    for(;;){
        c=text?(unsigned char)*text++:getc(input);
        if(c==' ')continue;
        if(c<'0'||c>'9')break;
        if(n>(LIMIT-1-(unsigned)(c-'0'))/10)range=1;
        else if(!range)n=n*10+(unsigned)(c-'0');
    }
    *out=n;
    return range||(!text&&ferror(input))?-1:n?1:0;
}
int main(int argc,char **argv)
{
    uint64_t n;
    int rc;
    if(argc>2){fputs("usage: primes [number]\n",stderr);return 1;}
    rc=start(stdin,argc==2?argv[1]:NULL,&n);
    if(rc<0){fputs("Ouch.\n",stderr);return 1;}
    if(!rc)return 0;
    setbuf(stdout,NULL);
    if(n<=2){if(puts("2")==EOF)return 1;n=3;}
    if(!(n&1))++n;
    for(;n<LIMIT;n+=2)if(prime(n)&&printf("%lu\n",(unsigned long)n)<0)return 1;
    return ferror(stdout)?1:0;
}
