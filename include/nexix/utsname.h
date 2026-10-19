#ifndef NEXIX_ABI_UTSNAME_H
#define NEXIX_ABI_UTSNAME_H
#define NX_UTS_FIELD_SIZE 65
/* Native uname syscall record: five fixed, NUL-terminated byte strings. */
struct nx_utsname {
    char sysname[NX_UTS_FIELD_SIZE];
    char nodename[NX_UTS_FIELD_SIZE];
    char release[NX_UTS_FIELD_SIZE];
    char version[NX_UTS_FIELD_SIZE];
    char machine[NX_UTS_FIELD_SIZE];
};
_Static_assert(sizeof(struct nx_utsname)==325,"uname ABI layout");
#endif
