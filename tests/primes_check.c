#define main primes_command
#include "../user/v7/cmd/primes.c"
#undef main
#include <stdlib.h>
int main(int argc,char **argv)
{
    unsigned long n;
    if(argc!=2)return 2;
    n=strtoul(argv[1],NULL,10);
    if(n==0){for(n=0;n<20000;++n)printf("%d",prime(n));return 0;}
    return prime(n)?0:1;
}
