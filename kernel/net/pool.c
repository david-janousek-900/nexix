#include "net/pool.h"

#define SLAB_MAGIC UINT64_C(0x4e58534c41425047)

/* Lives in slot zero; 64 bytes at most so every object size can host it. */
struct nx_net_slab_page {
    struct nx_net_slab_page *next;
    struct nx_net_slab *owner;
    uint64_t magic, used;   /* used: bit per slot, slot zero never set. */
};
_Static_assert(sizeof(struct nx_net_slab_page) <= 64, "slab page header");

void nx_net_budget_init(struct nx_net_budget *budget, size_t limit_pages)
{
    *budget = (struct nx_net_budget){ .limit = limit_pages };
}
void *nx_net_budget_page(struct nx_net_budget *budget, const struct nx_net_page_ops *ops)
{
    void *page = budget->pages < budget->limit ? ops->alloc(ops->context) : NULL;
    if (!page || ((uintptr_t)page & (NX_NET_PAGE_SIZE - 1))) {
        if (page) ops->free(ops->context, page);
        ++budget->failures;
        return NULL;
    }
    if (++budget->pages > budget->peak) budget->peak = budget->pages;
    return page;
}
void nx_net_budget_release(struct nx_net_budget *budget, const struct nx_net_page_ops *ops, void *page)
{
    --budget->pages;
    ops->free(ops->context, page);
}

bool nx_net_slab_init(struct nx_net_slab *slab, const struct nx_net_page_ops *ops,
                      struct nx_net_budget *budget, size_t object_size, size_t max_objects)
{
    if (!ops || !ops->alloc || !ops->free || !budget || !object_size ||
        object_size > NX_NET_PAGE_SIZE / 2 || !max_objects) return false;
    size_t size = (object_size + 63) & ~(size_t)63;
    *slab = (struct nx_net_slab){ .ops = ops, .budget = budget, .size = size,
        .per_page = NX_NET_PAGE_SIZE / size - 1, .max_objects = max_objects };
    return true;
}
void *nx_net_slab_alloc(struct nx_net_slab *slab)
{
    if (slab->in_use >= slab->max_objects) { ++slab->failures; return NULL; }
    if (!slab->free) {
        struct nx_net_slab_page *page = nx_net_budget_page(slab->budget, slab->ops);
        if (!page) { ++slab->failures; return NULL; }
        *page = (struct nx_net_slab_page){ .next = slab->pages, .owner = slab, .magic = SLAB_MAGIC };
        slab->pages = page;
        for (size_t slot = slab->per_page; slot; --slot) {
            void **object = (void **)((uint8_t *)page + slot * slab->size);
            *object = slab->free;
            slab->free = object;
        }
    }
    void **object = slab->free;
    slab->free = *object;
    struct nx_net_slab_page *page = (void *)((uintptr_t)object & ~(uintptr_t)(NX_NET_PAGE_SIZE - 1));
    page->used |= UINT64_C(1) << (((uintptr_t)object & (NX_NET_PAGE_SIZE - 1)) / slab->size);
    if (++slab->in_use > slab->peak) slab->peak = slab->in_use;
    ++slab->allocations;
    return object;
}
bool nx_net_slab_free(struct nx_net_slab *slab, void *object)
{
    uintptr_t offset = (uintptr_t)object & (NX_NET_PAGE_SIZE - 1);
    struct nx_net_slab_page *page = (void *)((uintptr_t)object - offset);
    if (!object || offset % slab->size || offset / slab->size == 0) return false;
    struct nx_net_slab_page *known = slab->pages;
    while (known && known != page) known = known->next;
    uint64_t bit = UINT64_C(1) << (offset / slab->size);
    if (!known || page->owner != slab || page->magic != SLAB_MAGIC || !(page->used & bit))
        return false;
    page->used &= ~bit;
    *(void **)object = slab->free;
    slab->free = object;
    --slab->in_use;
    ++slab->frees;
    return true;
}
bool nx_net_slab_destroy(struct nx_net_slab *slab)
{
    if (slab->in_use) return false;
    while (slab->pages) {
        struct nx_net_slab_page *page = slab->pages;
        slab->pages = page->next;
        page->magic = 0;
        nx_net_budget_release(slab->budget, slab->ops, page);
    }
    slab->free = NULL;
    return true;
}
