#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/times.h>
#include <sys/wait.h>
#define FN __attribute__((noinline,noclone))
extern int monitor_abi(void);
extern void profile_work(unsigned long);
FN long mon_integer(long a,long b,long c,long d,long e,long f,long g,long h)
{return a+2*b+3*c+4*d+5*e+6*f+7*g+8*h;}
FN double mon_float(double a,double b,double c,double d,double e,double f,double g,double h)
{return a+b+c+d+e+f+g+h;}
FN double mon_variadic(int n,...)
{
    va_list args;va_start(args,n);double value=va_arg(args,double);
    value+=va_arg(args,int);value+=va_arg(args,double);va_end(args);return value+n;
}
FN long mon_recursive(long n)
{return n?n+mon_recursive(n-1):0;}
FN void mon_callback(void){printf("tail\n");}
int main(int argc,char **argv,char **envp)
{
    if(argc!=2||strcmp(argv[0],"profiled")||!envp||!getenv("MON_TEST")||strcmp(getenv("MON_TEST"),"ok"))return 90;
    printf("head\n");
    if(!strcmp(argv[1],"quick"))_exit(19);
    if(!strcmp(argv[1],"exec")){execl("/bin/echo","echo","replaced",(char *)0);return 91;}
    for(long i=0;i<7;++i){
        if(mon_integer(i,2,3,4,5,6,7,8)!=i+203)return 92;
        if(mon_float((double)i,2,3,4,5,6,7,8)!=(double)i+35)return 93;
    }
    if(mon_variadic(2,1.5,3,4.5)!=11||mon_recursive(4)!=10||monitor_abi())return 94;
    struct tms t;if(times(&t)<0)return 95;clock_t begin=t.tms_utime;
    do{profile_work(100000);if(times(&t)<0)return 95;}while(t.tms_utime-begin<3);
    if(!strcmp(argv[1],"fork")){
        pid_t child=fork();if(child<0)return 96;
        if(!child)_exit(mon_integer(1,2,3,4,5,6,7,8)==204?0:1);
        int status;if(wait(&status)!=child||status)return 97;
    }
    if(atexit(mon_callback))return 98;
    if(!strcmp(argv[1],"exit"))exit(17);
    return 17;
}
