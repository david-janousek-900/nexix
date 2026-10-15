#include <unistd.h>
#include <errno.h>
#include <nexix/libc.h>
int profil(unsigned short *buffer,size_t size,uintptr_t offset,unsigned int scale)
{
    long result=nx_syscall6(NX_SYS_PROFIL,(long)(uintptr_t)buffer,size,offset,scale,0,0);
    if(result<0){errno=-result;return -1;}
    return 0;
}
