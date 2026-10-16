#ifndef NEXIX_LIBC_SYS_KINFO_H
#define NEXIX_LIBC_SYS_KINFO_H
#include <sys/types.h>
#include <nexix/kinfo.h>
/* NULL/0 queries bytes. Short buffers fail ERANGE without writes. Root only. */
ssize_t nx_kinfo(unsigned version, unsigned kind, void *buffer, size_t bytes);
#endif
