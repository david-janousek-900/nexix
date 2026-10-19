#ifndef NEXIX_UTSNAME_H
#define NEXIX_UTSNAME_H
#include <stdint.h>
#include <stddef.h>
int64_t uname_request(uintptr_t);
int64_t hostname_request(uintptr_t,size_t);
#endif
