#ifndef NEXIX_LIBC_SYS_UTSNAME_H
#define NEXIX_LIBC_SYS_UTSNAME_H
#include <nexix/utsname.h>
struct utsname {
    char sysname[NX_UTS_FIELD_SIZE];
    char nodename[NX_UTS_FIELD_SIZE];
    char release[NX_UTS_FIELD_SIZE];
    char version[NX_UTS_FIELD_SIZE];
    char machine[NX_UTS_FIELD_SIZE];
};
int uname(struct utsname *);
_Static_assert(sizeof(struct utsname)==sizeof(struct nx_utsname),"uname public layout");
#endif
