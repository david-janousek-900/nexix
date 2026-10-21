#ifndef NEXIX_NET_IF_H
#define NEXIX_NET_IF_H
#include "net/mbuf.h"

/* Network interfaces adapted from 4.4BSD net/if.c, if_ethersubr.c and
 * if_loop.c (task 418). Every function runs in network-worker context, which
 * serializes interface state; nothing here sleeps. Each function that takes an
 * mbuf chain consumes it, whatever the result.
 */
#define NX_NET_IF_MAX 4u
#define NX_NET_IF_NAME 8u
#define NX_NET_ETHER_ADDR 6u
#define NX_NET_ETHER_HEADER 14u
#define NX_NET_ETHER_MTU 1500u
#define NX_NET_LOOP_MTU 16384u      /* BSD LOMTU */
#define NX_NET_IFQ_MAXLEN 50u       /* BSD IFQ_MAXLEN */
#define NX_NET_ETHERTYPE_IP 0x0800u
#define NX_NET_ETHERTYPE_ARP 0x0806u
#define NX_NET_PROTOCOLS 4u

#define NX_NET_IFF_UP 0x1u
#define NX_NET_IFF_BROADCAST 0x2u
#define NX_NET_IFF_LOOPBACK 0x8u
#define NX_NET_IFF_RUNNING 0x40u

/* Outcomes for callers; task 424 maps them to the socket errno ABI. */
enum nx_net_result {
    NX_NET_OK, NX_NET_DOWN, NX_NET_TOO_BIG, NX_NET_NO_BUFFERS, NX_NET_INVALID, NX_NET_EXISTS,
    NX_NET_FULL, NX_NET_UNSUPPORTED
};

struct nx_net_if;
/* Driver operations. up/down change hardware state; transmit takes a complete
 * link-layer frame and consumes it, returning false when it was dropped. */
struct nx_net_if_ops {
    bool (*up)(struct nx_net_if *);
    void (*down)(struct nx_net_if *);
    bool (*transmit)(struct nx_net_if *, struct nx_net_mbuf *frame);
};
struct nx_net_if_stats {
    uint64_t ipackets, ierrors, opackets, oerrors, ibytes, obytes, iqdrops, noproto;
};
struct nx_net_if {
    char name[NX_NET_IF_NAME];
    uint32_t flags;
    size_t mtu;
    uint8_t lladdr[NX_NET_ETHER_ADDR];
    const struct nx_net_if_ops *ops;
    void *driver;
    struct nx_net_if_stats stats;
    /* Loopback: bounded input queue (nextpkt-linked) and deferred delivery. */
    struct nx_net_mbuf *queue_head, *queue_tail;
    size_t queue_length, queue_limit;
    void (*kick)(void *);
    void *kick_argument;
    bool attached;
};

/* Upper-layer input for one ethertype, receiving the payload (link header
 * removed, packet header length adjusted) and taking ownership. */
typedef void (*nx_net_proto_input)(struct nx_net_if *, struct nx_net_mbuf *);
enum nx_net_result nx_net_proto_register(uint16_t ethertype, nx_net_proto_input);
void nx_net_proto_unregister(uint16_t ethertype);

/* Attach with a unique NUL-terminated name; interfaces start down. Detach
 * brings an interface down and frees anything queued on it. */
enum nx_net_result nx_net_if_attach(struct nx_net_if *);
void nx_net_if_detach(struct nx_net_if *);
struct nx_net_if *nx_net_if_find(const char *name);
size_t nx_net_if_count(void);
struct nx_net_if *nx_net_if_at(size_t index);
enum nx_net_result nx_net_if_up(struct nx_net_if *);
void nx_net_if_down(struct nx_net_if *);

/* Send an upper-layer packet of ethertype to next-hop link address dst
 * (ignored by loopback). Ethernet prepends its header; payloads above the MTU
 * are refused. */
enum nx_net_result nx_net_if_output(struct nx_net_if *, struct nx_net_mbuf *packet,
                                    const uint8_t dst[NX_NET_ETHER_ADDR], uint16_t ethertype);

/* Ethernet: initialize an interface record, and receive one frame from its
 * driver. Frames outside 14..14+MTU bytes, not addressed to this interface or
 * broadcast, or of an unregistered type are dropped and counted. */
void nx_net_ether_init(struct nx_net_if *, const char *name, const uint8_t lladdr[NX_NET_ETHER_ADDR],
                       const struct nx_net_if_ops *, void *driver);
void nx_net_ether_input(struct nx_net_if *, struct nx_net_mbuf *frame);
extern const uint8_t nx_net_ether_broadcast[NX_NET_ETHER_ADDR];

/* Loopback: output queues the packet (dropping when the queue is full) and
 * calls kick, which must arrange a later nx_net_loop_service call; packets
 * are never delivered recursively from output. */
void nx_net_loop_init(struct nx_net_if *, const char *name, void (*kick)(void *), void *argument);
/* Deliver queued packets, at most budget of them; returns the number left. */
size_t nx_net_loop_service(struct nx_net_if *, size_t budget);
#endif
