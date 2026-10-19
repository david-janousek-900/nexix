#include <sys/utsname.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>

#define CHECK(c) do { if(!(c)) { puts("UNAME_API_ERROR");return 1; } } while(0)
static const struct utsname readonly;
int main(void)
{
    struct { uint64_t before;struct utsname value;uint64_t after; } record;
    memset(&record,0xa5,sizeof record);
    CHECK(uname(&record.value)==0);
    CHECK(record.before==UINT64_C(0xa5a5a5a5a5a5a5a5) && record.after==record.before);
    CHECK(!strcmp(record.value.sysname,"Nexix"));
    CHECK(!strcmp(record.value.nodename,"nexix.localdomain"));
    CHECK(!strcmp(record.value.release,"0.1.0"));
    CHECK(!strcmp(record.value.version,"development"));
    CHECK(!strcmp(record.value.machine,"x86_64"));
    errno=0;CHECK(uname(NULL)==-1 && errno==EFAULT);
    errno=0;CHECK(uname((struct utsname *)(uintptr_t)-1)==-1 && errno==EFAULT);
    CHECK(nx_syscall6(NX_SYS_UNAME,(long)&readonly,0,0,0,0,0)==-EFAULT);
    CHECK(readonly.sysname[0]==0);
    puts("UNAME_API_OK");return 0;
}
