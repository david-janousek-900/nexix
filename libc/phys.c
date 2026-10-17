#include <sys/phys.h>
#include <errno.h>
#include <nexix/libc.h>
int phys(unsigned slot,size_t pages,uintptr_t physical)
{
    long r=nx_syscall6(NX_SYS_PHYS,slot,pages,physical,0,0,0);
    if(r<0){errno=(int)-r;return -1;}
    return 0;
}
