#ifndef NX_LD_H
#define NX_LD_H
#include "object.h"
#define LD_NONE ((size_t)-1)
#define LD_OBJECT_LIMIT 4096
#define LD_SYMBOL_LIMIT 32768
#define LD_BYTE_LIMIT (32UL*1024*1024)
struct ld_input {
    struct object object;
    unsigned char *data;
    char *path, *member;
    Elf64_Shdr symtab, strings;
    Elf64_Sym *symbols;
    size_t symbol_count;
    int selected;
};
struct ld_symbol {
    const char *name;
    size_t object, symbol;
    uint64_t size, alignment, value;
    unsigned bind;
    int defined, common, required;
};
struct ld_state {
    struct ld_input *inputs;
    size_t input_count, input_capacity, bytes;
    struct ld_symbol *symbols;
    size_t symbol_count, symbol_capacity;
    uint32_t *hash;
    int failed;
    char error[512];
};
void ld_init(struct ld_state *);
void ld_free(struct ld_state *);
/* Object/archive arguments are processed in command-line order. Each archive
 * is searched to a fixed point. Unselected members add no undefined symbols. */
int ld_add_file(struct ld_state *, const char *path);
int ld_require(struct ld_state *, const char *name);
size_t ld_lookup(const struct ld_state *, const char *name);
int ld_check(struct ld_state *);
/* On success returns caller-owned ELF64 bytes; outputs stay unchanged on error. */
int ld_executable(struct ld_state *, const char *entry, unsigned char **, size_t *);
const char *ld_symbol_name(const struct ld_input *, size_t);
#endif
