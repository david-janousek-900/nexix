#include <sys/utsname.h>
#include <errno.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>
int uname(struct utsname *buffer)
{
    long result=nx_syscall6(NX_SYS_UNAME,(long)(uintptr_t)buffer,0,0,0,0,0);
    if(result<0) { errno=(int)-result;return -1; }
    return (int)result;
}
