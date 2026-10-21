#ifndef NX_AS_H
#define NX_AS_H
#include <stddef.h>
#include <stdint.h>
#define AS_NONE ((size_t)-1)
#define AS_DOT ((size_t)-2)
#define AS_LIMIT (8UL*1024*1024)
struct as_expr {uint64_t value;size_t plus,minus;};
struct as_symbol {
    char *name;
    unsigned defined,binding,type,common,binding_explicit;
    size_t section,record;
    uint64_t value,size,alignment;
    struct as_expr expression;
};
struct as_section {char *name;uint64_t flags,alignment;unsigned type;};
enum as_kind {AS_LABEL,AS_INSTRUCTION,AS_DATA,AS_BYTES,AS_SPACE,AS_ALIGN,AS_EQUATE,AS_SIZE};
enum as_operand_kind {AS_REGISTER,AS_IMMEDIATE,AS_ADDRESS,AS_MEMORY};
struct as_operand {
    enum as_operand_kind kind;
    char reg[16],base[16],index[16];
    unsigned scale,indirect;
    struct as_expr expression;
};
struct as_record {
    enum as_kind kind;size_t section,symbol;unsigned line,width,count;
    char mnemonic[32];struct as_operand operands[3];
    struct as_expr expression;
    unsigned char *bytes;size_t length;
};
struct as_program {
    struct as_record *records;size_t count,capacity;
    struct as_symbol *symbols;size_t symbol_count,symbol_capacity;
    struct as_section *sections;size_t section_count;
    unsigned errors;
};
struct as_expr as_absolute(uint64_t);
size_t as_symbol(struct as_program *,const char *);
int as_parse(const char *,struct as_program *);
int as_object(struct as_program *,unsigned char **,size_t *);
void as_free(struct as_program *);
#endif
