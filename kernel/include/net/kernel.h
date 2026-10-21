#ifndef NEXIX_NET_KERNEL_H
#define NEXIX_NET_KERNEL_H
#include "net/mbuf.h"

/* Kernel page source: identity-mapped physical frames from page_alloc. */
extern const struct nx_net_page_ops nx_net_kernel_pages;

/* Fixed limits derived from installed memory: packet pages are one eighth of
 * total pages, clamped to [32, 2048] (128 KiB .. 8 MiB). Socket objects use a
 * separate 16-page budget so packet floods cannot starve socket creation.
 */
#define NX_NET_SOCKET_OBJECT_SIZE 512u
#define NX_NET_SOCKET_PAGES 16u
struct nx_net_kernel_limits {
    size_t packet_pages, socket_objects;
    struct nx_net_mbuf_limits mbuf;
};
struct nx_net_kernel_limits nx_net_kernel_limits(size_t total_pages);

/* Boot self-test of pools against the real page allocator. */
void net_pool_selftest(void);
#endif
