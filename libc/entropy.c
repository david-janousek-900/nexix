#include <sys/random.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>
#include <errno.h>
#include <stdint.h>
int getentropy(void *buffer,size_t length)
{
    long result=nx_syscall6(NX_SYS_GETENTROPY,(long)(uintptr_t)buffer,(long)length,0,0,0,0);
    if(result<0){errno=(int)-result;return -1;}
    return 0;
}
