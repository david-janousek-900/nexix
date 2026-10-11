#ifndef NEXIX_LIBC_SYS_DIR_H
#define NEXIX_LIBC_SYS_DIR_H
#include <stdint.h>
#define DIRSIZ 14
struct direct { uint16_t d_ino; char d_name[DIRSIZ]; };
_Static_assert(sizeof(struct direct)==16,"V7 directory record");
#endif
