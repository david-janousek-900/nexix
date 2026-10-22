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

/* Live stack (task 418). net_start runs in process context after proc_init:
 * it creates the packet pool, starts the worker (whose parent is the caller),
 * registers ARP and IPv4 with their 250 ms and 500 ms timers (tasks 419-420),
 * ICMP and UDP (tasks 421-422) and TCP with its 100 ms timer (task 423), and
 * attaches lo0 (up, unaddressed) and, when an e1000 exists, em0 (down).
 * net_stop detaches every interface, flushes raw, UDP and TCP sockets, stops
 * and reaps nothing (the parent reaps the worker) and destroys the pool. */
bool net_start(void);
void net_stop(void);
struct nx_net_mbuf_pool *net_pool(void);
void net_stack_selftest(void);
#endif
