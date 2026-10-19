#include <sys/timeb.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>
#include <nexix/timeb.h>
#include <unistd.h>
#include <sys/times.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#define CHECK(c) do{if(!(c)){printf("ftime failure line %d errno %d\n",__LINE__,errno);return 1;}}while(0)
int main(void)
{
    struct timeb a,b;struct nx_timeb raw;
    CHECK(ftime(&a)==0 && a.millitm<1000);
    struct tms t;clock_t deadline=times(&t)+2*NX_CLK_TCK;
    int rollover=0,fraction=0;
    do {
        CHECK(ftime(&b)==0 && b.millitm<1000);
        CHECK(b.time>a.time || (b.time==a.time && b.millitm>=a.millitm));
        fraction|=b.millitm!=0;rollover|=b.time>a.time;
        a=b;
    }while(times(&t)<deadline);
    CHECK(rollover && fraction);
    CHECK(nx_syscall6(NX_SYS_FTIME,(uintptr_t)&raw,0,0,0,0,0)==0 && !raw.reserved && !raw.timezone && !raw.dstflag);
    CHECK(ftime(NULL)==-1 && errno==EFAULT);
    CHECK(ftime((void *)1)==-1 && errno==EFAULT);
    CHECK(nx_syscall6(NX_SYS_FTIME,(uintptr_t)main,0,0,0,0,0)==-EFAULT);
    puts("FTIME_API_OK");return 0;
}
