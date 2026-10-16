#include <sys/kinfo.h>
#include <errno.h>
#include <nexix/libc.h>
ssize_t nx_kinfo(unsigned version,unsigned kind,void *buffer,size_t bytes)
{
    long result=nx_syscall6(NX_SYS_KINFO,version,kind,(long)(uintptr_t)buffer,bytes,0,0);
    if(result<0){errno=(int)-result;return -1;}
    return result;
}
