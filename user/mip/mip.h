/* Shared native contracts for the supplied portable compiler infrastructure. */
#ifndef NX_MIP_H
#define NX_MIP_H
#include <stddef.h>
#include <stdint.h>
#include "manifest.h"

typedef uint64_t mip_type;
typedef int64_t mip_offset; /* byte offsets; never an int holding a pointer */
typedef uint64_t mip_constant; /* two's-complement bits, including ULONG */
#define MIP_NO_NODE UINT32_MAX
#define MIP_NAME_MAX 255
#define MIP_MAX_MODIFIERS 29

/* The original sixteen basic types retain their manifest numbers. */
struct mip_layout { uint64_t size; unsigned alignment; };
int mip_type_valid(mip_type type);
int mip_type_add(mip_type type, unsigned modifier, mip_type *out);
int mip_type_remove(mip_type type, mip_type *out);
/* Arrays/aggregates require front-end dimensions/member layout; return -1. */
int mip_scalar_layout(mip_type type, struct mip_layout *out);
int mip_align(uint64_t offset, unsigned alignment, uint64_t *out);
unsigned mip_op_flags(unsigned op);
const char *mip_op_name(unsigned op);

/* Explicit node members replace the PDP-11 anonymous union overlays.
 * Child indices refer only to earlier nodes: construction is a bounded DAG.
 * Names are exact, case-sensitive and never silently truncated to eight bytes.
 */
struct mip_node {
    unsigned op;
    mip_type type;
    uint32_t left, right;
    mip_constant value;
    mip_offset offset;
    double floating;
    unsigned char extended[10]; /* canonical little-endian x87 significand/sign-exponent */
    uint64_t aggregate_size;
    unsigned aggregate_alignment;
    unsigned reg, label, field_size, field_shift, source_line;
    char name[MIP_NAME_MAX+1];
};
struct mip_arena {
    struct mip_node *nodes;
    uint32_t count, capacity;
};
void mip_arena_init(struct mip_arena *, struct mip_node *, uint32_t capacity);
/* Returns -1 without changing the arena or output on malformed/full input. */
int mip_node_valid(const struct mip_arena *, const struct mip_node *);
int mip_node_add(struct mip_arena *, const struct mip_node *, uint32_t *out);
#define MIP_NODE_LIMIT 65536U
#define MIP_WIRE_NODE_SIZE 88U
#define MIP_WIRE_LIMIT (32UL*1024*1024)
typedef int (*mip_visitor)(const struct mip_node *, uint32_t, void *);
int mip_walk(const struct mip_arena *, uint32_t root, mip_visitor, void *);
int mip_copy(const struct mip_arena *, uint32_t root, struct mip_arena *, uint32_t *);
int mip_encode(const struct mip_arena *, uint32_t root, unsigned char **, size_t *);
int mip_decode(const unsigned char *, size_t, struct mip_arena *, uint32_t *root);
#endif
