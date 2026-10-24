#ifndef NEXIX_ENTROPY_H
#define NEXIX_ENTROPY_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
bool entropy_arch_word(uint64_t *);
int entropy_fill(void *,size_t);
#endif
