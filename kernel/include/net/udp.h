#ifndef NEXIX_NET_UDP_H
#define NEXIX_NET_UDP_H
#include "net/icmp.h"

/* UDP adapted from 4.4BSD netinet/udp_usrreq.c and the UDP parts of in_pcb.c
 * (task 422). A fixed table of protocol control blocks, each with a bounded
 * receive queue of datagrams. Unmatched unicast datagrams provoke ICMP port
 * unreachable; ICMP errors are reported to the connected sockets they concern.
 * Worker context only; every function that takes an mbuf chain consumes it.
 */
#define NX_NET_IPPROTO_UDP 17u
#define NX_NET_UDP_HEADER 8u
#define NX_NET_UDP_SOCKETS 32u
#define NX_NET_UDP_SNDMAX 9216u                       /* BSD udp_sendspace: largest datagram */
#define NX_NET_UDP_RCVQ (40u * (1024u + 16u))         /* BSD udp_recvspace */
#define NX_NET_UDP_RESERVED 1024u                     /* BSD IPPORT_RESERVED */
#define NX_NET_UDP_EPHEMERAL_FIRST 49152u             /* IANA dynamic ports */
#define NX_NET_UDP_EPHEMERAL_LAST 65535u

struct nx_net_udp {
    bool open, reuse;
    nx_net_addr_t local_addr, foreign_addr;   /* host order; 0: any / unconnected */
    nx_net_port_t local_port, foreign_port;   /* host order; 0: unbound / unconnected */
    struct nx_net_sockbuf receive;
    enum nx_net_result error;                 /* pending asynchronous error (BSD so_error) */
    void (*readable)(void *);                 /* after a datagram or error is queued */
    void *argument;
};

struct nx_net_udp_stats {
    uint64_t received, delivered, too_short, bad_length, bad_checksum, no_port, broadcast_drops;
    uint64_t queue_drops, sent, output_errors, errors_notified;
};

/* Clears statistics and closes every socket; registers nothing. Call
 * nx_net_ip_register(NX_NET_IPPROTO_UDP, nx_net_udp_input) and
 * nx_net_icmp_register_notify(NX_NET_IPPROTO_UDP, nx_net_udp_notify). */
void nx_net_udp_init(void);
struct nx_net_udp_stats nx_net_udp_stats(void);
void nx_net_udp_input(struct nx_net_if *, struct nx_net_mbuf *, size_t header_length);
void nx_net_udp_notify(const struct nx_net_icmp_notice *);

/* FULL when every socket is open. */
enum nx_net_result nx_net_udp_open(void (*readable)(void *), void *argument, struct nx_net_udp **);
/* Frees anything queued. */
void nx_net_udp_close(struct nx_net_udp *);
/* SO_REUSEADDR: sockets that all set it may share a local address and port. */
void nx_net_udp_set_reuse(struct nx_net_udp *, bool);
/* Bind once (INVALID when already bound). addr is 0 or one of ours (else
 * INVALID); port 0 picks a free ephemeral port (FULL when none); ports below
 * 1024 need privilege (DENIED); a port in use by an overlapping address is
 * EXISTS unless both sockets allow reuse. */
enum nx_net_result nx_net_udp_bind(struct nx_net_udp *, nx_net_addr_t addr, nx_net_port_t port, bool privileged);
/* Connect to a unicast address and nonzero port (else INVALID), binding an
 * ephemeral port first when unbound. addr 0 disconnects. Clears any pending
 * error. */
enum nx_net_result nx_net_udp_connect(struct nx_net_udp *, nx_net_addr_t addr, nx_net_port_t port);
/* Send payload (packet-header chain, 0..9216 bytes; TOO_BIG above) to
 * addr:port, or to the connected peer when addr is 0 (EXISTS when both,
 * INVALID when neither or port 0). Binds an ephemeral port when unbound. */
enum nx_net_result nx_net_udp_send(struct nx_net_udp *, struct nx_net_mbuf *payload, nx_net_addr_t addr,
                                   nx_net_port_t port);
/* The oldest queued datagram's payload (possibly empty) and its source, or
 * NULL when none is queued. */
struct nx_net_mbuf *nx_net_udp_receive(struct nx_net_udp *, nx_net_addr_t *from, nx_net_port_t *from_port);
/* Return and clear the pending error (OK when none). */
enum nx_net_result nx_net_udp_take_error(struct nx_net_udp *);
size_t nx_net_udp_open_count(void);
/* Free every queued datagram (network stop); sockets stay open. */
void nx_net_udp_flush(void);
#endif
