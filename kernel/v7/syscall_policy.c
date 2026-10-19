/* V7 sysent.c nosys/nullsys dispositions for the native direct-call ABI. */
#include "syscall.h"
#include <nexix/syscall.h>
#include <nexix/errno.h>
int64_t syscall_fallback(uint64_t number)
{
    switch(number){
    case 0:case 38:return 0;
    case 40:case 45:case 49:case 50:case 55:case 57:case 58:case 62:case 63:
        return -NX_EINVAL;
    default:
        /* Recognized slots without a dispatcher implementation are explicitly
         * unavailable. Completely unknown numbers retain V7's nosys error. */
        return number<=NX_SYS_TIMEDPAUSE?-NX_ENOSYS:-NX_EINVAL;
    }
}
