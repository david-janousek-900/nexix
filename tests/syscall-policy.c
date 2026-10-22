#include "syscall.h"
#include <nexix/syscall.h>
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(syscall_fallback(0)==0 && syscall_fallback(38)==0);
    unsigned invalid[]={40,45,49,50,55,57,58,62,63};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;i++)assert(syscall_fallback(invalid[i])==-NX_EINVAL);
    /* Fallback for a known call is distinct from an unassigned ABI number. */
    for(unsigned i=1;i<=NX_SYS_LAST;i++){
        if(i==38)continue;
        int reserved=0;for(unsigned j=0;j<sizeof invalid/sizeof *invalid;j++)reserved|=i==invalid[j];
        if(!reserved)assert(syscall_fallback(i)==-NX_ENOSYS);
    }
    assert(syscall_fallback(NX_SYS_LAST+1)==-NX_EINVAL);
    assert(syscall_fallback(UINT64_MAX)==-NX_EINVAL);
    assert(syscall_fallback(UINT64_C(0x100000002))==-NX_EINVAL);
    puts("PASS: historical nullsys/nosys, native assigned-but-unavailable calls and full-width invalid numbers");
}
