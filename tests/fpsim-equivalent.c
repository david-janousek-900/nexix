#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#define CHECK(x) do { if (!(x)) { printf("fpsim failure line %d\n", __LINE__); return 1; } } while (0)
int main(void)
{
    volatile double a=7.5,b=2.0,negative=-7.5;
    CHECK(a+b==9.5 && a-b==5.5 && a*b==15.0 && a/b==3.75);
    CHECK(-a==negative && a>b && negative<0.0 && a!=b);
    CHECK((long)a==7 && (long)negative==-7);
    volatile long integer=123456789;
    CHECK((double)integer==123456789.0);
    volatile float single=1.25f;
    CHECK((double)single==1.25 && (float)a==7.5f);
    int exponent;
    CHECK(frexp(a,&exponent)==0.9375 && exponent==3);
    CHECK(ldexp(0.9375,3)==a);
    double integral;
    CHECK(modf(negative,&integral)==-0.5 && integral==-7.0);
    CHECK(fmod(a,b)==1.5 && sqrt(144.0)==12.0);
    /* fakfp forced an illegal-instruction disposition when no simulator existed.
       Native unsupported instructions must still terminate through SIGILL. */
    int child=fork(),status;
    CHECK(child>=0);
    if(!child){ __asm__ volatile("ud2"); _exit(90); }
    CHECK(wait(&status)==child && (status&127)==SIGILL);
    puts("FPSIM_EQUIVALENT_OK");return 0;
}
