#ifndef NEXIX_NET_POOL_H
#define NEXIX_NET_POOL_H
#include "net/types.h"

/* Bounded page-backed object pools for networking (task 414). Callers serialize
 * every operation; the network worker (task 415) owns that serialization. Pools
 * never sleep and never fall back to another allocator.
 */
#define NX_NET_PAGE_SIZE 4096u

/* Page source. alloc returns one page-aligned NX_NET_PAGE_SIZE page or NULL;
 * free receives only pages returned by alloc. Pages need not be cleared.
 */
struct nx_net_page_ops {
    void *(*alloc)(void *context);
    void (*free)(void *context, void *page);
    void *context;
};

/* Shared page limit. Several pools may draw from one budget. */
struct nx_net_budget {
    size_t limit, pages, peak;
    uint64_t failures;   /* Refused by limit or by the page source. */
};
void nx_net_budget_init(struct nx_net_budget *, size_t limit_pages);
void *nx_net_budget_page(struct nx_net_budget *, const struct nx_net_page_ops *);
void nx_net_budget_release(struct nx_net_budget *, const struct nx_net_page_ops *, void *page);

/* Fixed-size object slab. Slot zero of every page holds the page header, so
 * an object size of 64..2048 bytes (rounded to 64) gives 4096/size - 1 objects
 * per page. Pages are retained until destroy; freed objects are reused first.
 */
struct nx_net_slab_page;
struct nx_net_slab {
    const struct nx_net_page_ops *ops;
    struct nx_net_budget *budget;
    struct nx_net_slab_page *pages;
    void *free;
    size_t size, per_page, max_objects, in_use, peak;
    uint64_t allocations, frees, failures;
};
bool nx_net_slab_init(struct nx_net_slab *, const struct nx_net_page_ops *,
                      struct nx_net_budget *, size_t object_size, size_t max_objects);
void *nx_net_slab_alloc(struct nx_net_slab *);
/* Rejects foreign, misaligned, header and already-free addresses. */
bool nx_net_slab_free(struct nx_net_slab *, void *object);
/* Fails without freeing anything while objects remain allocated. */
bool nx_net_slab_destroy(struct nx_net_slab *);
#endif
