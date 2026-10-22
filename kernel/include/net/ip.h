#ifndef NEXIX_NET_IP_H
#define NEXIX_NET_IP_H
#include "net/route.h"

/* IPv4 input, output, fragmentation and reassembly adapted from 4.4BSD
 * netinet/ip_input.c and ip_output.c (task 420). Forwarding is disabled:
 * datagrams not addressed to this host are dropped. Options are length
 * checked and passed up but not interpreted. Times are milliseconds from the
 * clock given at init; nx_net_ip_timer expires reassembly and should run every
 * NX_NET_IP_TIMER_MS. Worker context only; every function consumes its chain.
 */
#define NX_NET_IP_MAXPACKET 65535u
#define NX_NET_IP_DEFTTL 64u
#define NX_NET_IP_PROTOCOLS 4u
#define NX_NET_IP_QUEUES 8u            /* datagrams being reassembled */
#define NX_NET_IP_QUEUE_FRAGS 16u      /* fragments per datagram */
#define NX_NET_IP_FRAGS 64u            /* fragments held in total */
#define NX_NET_IP_REASS_MS 30000u      /* BSD IPFRAGTTL, 60 half-seconds */
#define NX_NET_IP_TIMER_MS 500u

/* Transport input: m holds the whole datagram, its header_length header
 * bytes contiguous, already validated and reassembled. */
typedef void (*nx_net_ip_input_fn)(struct nx_net_if *, struct nx_net_mbuf *, size_t header_length);
enum nx_net_result nx_net_ip_register(uint8_t protocol, nx_net_ip_input_fn);
void nx_net_ip_unregister(uint8_t protocol);
/* Called with a datagram for an unregistered protocol instead of dropping it
 * (ICMP protocol-unreachable, task 421); NULL restores dropping. */
void nx_net_ip_set_unknown_hook(nx_net_ip_input_fn);

struct nx_net_ip_stats {
    uint64_t received, delivered, too_short, bad_version, bad_header, bad_checksum, bad_length;
    uint64_t bad_source, not_for_us, no_protocol;
    uint64_t fragments, reassembled, frag_dropped, frag_overlaps, frag_timeouts;
    uint64_t sent, fragmented, fragments_sent, cant_fragment, output_errors;
};

/* Clears reassembly; fragment headers are allocated from pool. */
void nx_net_ip_init(struct nx_net_mbuf_pool *, uint64_t (*now_ms)(void));
/* Free every partial datagram, or those last fed by one interface (call
 * from the interface change hook). */
void nx_net_ip_flush(void);
void nx_net_ip_if_changed(struct nx_net_if *);
void nx_net_ip_timer(void);

/* Ethertype 0x0800 input. */
void nx_net_ip_input(struct nx_net_if *, struct nx_net_mbuf *);

struct nx_net_ip_send {
    nx_net_addr_t src;         /* 0: the route's preferred source */
    nx_net_addr_t dst;
    uint8_t protocol, ttl, tos; /* ttl 0: NX_NET_IP_DEFTTL */
    bool dont_fragment;
};
/* Prepend a 20-byte header to payload (a packet-header chain) and send it,
 * fragmenting at the route's MTU unless dont_fragment (then TOO_BIG). A src
 * that is not one of our addresses is INVALID. When a fragment cannot be
 * built nothing is sent (NO_BUFFERS). */
enum nx_net_result nx_net_ip_output(struct nx_net_mbuf *payload, const struct nx_net_ip_send *);

struct nx_net_ip_stats nx_net_ip_stats(void);
/* Datagrams currently being reassembled. */
size_t nx_net_ip_reassembling(void);
#endif
