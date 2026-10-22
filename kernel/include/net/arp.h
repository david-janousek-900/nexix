#ifndef NEXIX_NET_ARP_H
#define NEXIX_NET_ARP_H
#include "net/if.h"

/* Ethernet/IPv4 ARP adapted from 4.4BSD netinet/if_ether.c (task 419). A
 * fixed table replaces BSD's routing-table llinfo; each entry holds a few
 * packets while unresolved, under a global bound. Times are milliseconds from
 * the clock given at init; nx_net_arp_timer runs retries and expiry and should
 * be called every NX_NET_ARP_TIMER_MS. Worker context only.
 */
#define NX_NET_ARP_ENTRIES 16u
#define NX_NET_ARP_HOLD 4u                  /* packets per unresolved entry */
#define NX_NET_ARP_PENDING 16u              /* packets held in total */
#define NX_NET_ARP_MAXTRIES 5u              /* BSD arp_maxtries */
#define NX_NET_ARP_RETRY_MS 1000u           /* between requests */
#define NX_NET_ARP_KEEP_MS (20u * 60 * 1000) /* BSD arpt_keep */
#define NX_NET_ARP_DOWN_MS (20u * 1000)     /* BSD arpt_down: negative entry */
#define NX_NET_ARP_TIMER_MS 250u

struct nx_net_arp_stats {
    uint64_t requests, replies, received, invalid, ignored, conflicts;
    uint64_t held, hold_drops, timeouts, rejected, evictions, table_full;
};

/* Clears the table; requests are allocated from pool. */
void nx_net_arp_init(struct nx_net_mbuf_pool *, uint64_t (*now_ms)(void));
/* Discard entries (and held packets) of one interface, or all for NULL.
 * Install nx_net_arp_if_changed as the interface change hook. */
void nx_net_arp_flush(struct nx_net_if *);
void nx_net_arp_if_changed(struct nx_net_if *);

/* Send an IPv4 packet to on-link next_hop on a running, configured Ethernet
 * interface: directly when resolved, otherwise held while requests are sent.
 * Consumes the packet. HOST_UNREACHABLE while a failed resolution is cached;
 * NO_BUFFERS when the table is full of unresolved entries. */
enum nx_net_result nx_net_arp_output(struct nx_net_if *, nx_net_addr_t next_hop, struct nx_net_mbuf *);
/* Ethertype 0x0806 input: validates, learns on-link senders that are cached
 * or that address us, releases held packets and answers requests for our
 * address in place. Consumes the packet. */
void nx_net_arp_input(struct nx_net_if *, struct nx_net_mbuf *);
void nx_net_arp_timer(void);

/* True with the link address when a valid resolved entry exists. */
bool nx_net_arp_lookup(const struct nx_net_if *, nx_net_addr_t, uint8_t mac[NX_NET_ETHER_ADDR]);
struct nx_net_arp_stats nx_net_arp_stats(void);
#endif
