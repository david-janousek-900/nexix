#ifndef NX_TOOLCHAIN_OBJECT_H
#define NX_TOOLCHAIN_OBJECT_H
#include <stddef.h>
#include <stdint.h>
#include <elf.h>
struct object { const unsigned char *bytes; size_t size; Elf64_Ehdr h; };
struct object_symbol { const char *name; uint64_t value; unsigned bind; char type; };
typedef int (*object_visitor)(const char *, const char *, const unsigned char *, size_t, void *);
int object_files(const char *, object_visitor, void *);
int object_open(struct object *, const unsigned char *, size_t);
int object_section(const struct object *, unsigned, Elf64_Shdr *);
int object_symbols(const struct object *, struct object_symbol **, size_t *);
int object_sizes(const struct object *, uint64_t *, uint64_t *, uint64_t *);
#endif
