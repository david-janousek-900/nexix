#include <sys/ptrace.h>
#include <errno.h>
#include <nexix/libc.h>
long ptrace(int request,pid_t pid,void *address,long data)
{
    /* A successful read may return any signed word, including -1. The native
     * syscall uses a fifth output pointer so errors cannot collide with data. */
    uint64_t word=0;
    errno=0;
    long status=nx_syscall6(NX_SYS_PTRACE,request,pid,(long)(uintptr_t)address,data,
                           (long)(uintptr_t)&word,0);
    if(status<0){errno=-status;return -1;}
    return request>=NX_PT_READ_I&&request<=NX_PT_READ_U?(long)word:0;
}
