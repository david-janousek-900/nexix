#include <unistd.h>
#include <errno.h>
#include <nexix/libc.h>
int lock(int flag)
{
    long result=nx_syscall6(NX_SYS_LOCK,flag,0,0,0,0,0);
    if(result<0){errno=(int)-result;return -1;}
    return 0;
}
