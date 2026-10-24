#ifndef NEXIX_LIBC_RANDOM_H
#define NEXIX_LIBC_RANDOM_H
#include <stddef.h>
/* Atomic fill of up to 256 bytes, or -1 with errno. No PRNG fallback. */
int getentropy(void *,size_t);
#endif
