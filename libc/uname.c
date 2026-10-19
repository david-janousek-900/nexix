#include <sys/utsname.h>
#include <unistd.h>
#include <errno.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>
int uname(struct utsname *buffer)
{
    long result=nx_syscall6(NX_SYS_UNAME,(long)(uintptr_t)buffer,0,0,0,0,0);
    if(result<0) { errno=(int)-result;return -1; }
    return (int)result;
}

int gethostname(char *buffer,size_t size)
{
    struct utsname value;
    size_t length;
    if(uname(&value)<0)return -1;
    for(length=0;value.nodename[length];length++);
    if(size<=length){errno=ERANGE;return -1;}
    if(!buffer){errno=EFAULT;return -1;}
    for(size_t i=0;i<=length;i++)buffer[i]=value.nodename[i];
    return 0;
}
int sethostname(const char *name,size_t size)
{
    long result=nx_syscall6(NX_SYS_SETHOSTNAME,(long)(uintptr_t)name,size,0,0,0,0);
    if(result<0){errno=(int)-result;return -1;}
    return (int)result;
}
