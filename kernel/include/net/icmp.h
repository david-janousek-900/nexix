#ifndef NEXIX_NET_ICMP_H
#define NEXIX_NET_ICMP_H
#include "net/ip.h"

/* ICMP and raw ICMP sockets adapted from 4.4BSD netinet/ip_icmp.c and
 * raw_ip.c (task 421). The kernel answers echo requests, sends unreachable
 * errors for undeliverable datagrams and passes received errors to the
 * transport that sent the quoted datagram. Redirects, source quench,
 * timestamp and mask requests are not acted on. Raw sockets carry ICMP only
 * and may be opened only by privileged callers. Worker context only; every
 * function that takes an mbuf chain consumes it.
 */
#define NX_NET_IPPROTO_ICMP 1u
#define NX_NET_ICMP_ECHOREPLY 0u
#define NX_NET_ICMP_UNREACH 3u
#define NX_NET_ICMP_UNREACH_NET 0u
#define NX_NET_ICMP_UNREACH_HOST 1u
#define NX_NET_ICMP_UNREACH_PROTOCOL 2u
#define NX_NET_ICMP_UNREACH_PORT 3u
#define NX_NET_ICMP_UNREACH_NEEDFRAG 4u
#define NX_NET_ICMP_SOURCEQUENCH 4u
#define NX_NET_ICMP_REDIRECT 5u
#define NX_NET_ICMP_ECHO 8u
#define NX_NET_ICMP_TIMXCEED 11u
#define NX_NET_ICMP_PARAMPROB 12u
#define NX_NET_ICMP_MINLEN 8u
#define NX_NET_ICMP_QUOTE 8u           /* payload bytes quoted after the header */
#define NX_NET_ICMP_NOTIFY 4u          /* transports taking error notices */

/* A received error about one of our datagrams. quoted holds its IP header
 * and at least 8 payload bytes, contiguous; mtu is the next-hop MTU of a
 * needfrag error (0 when absent). */
struct nx_net_icmp_notice {
    enum nx_net_result error;   /* UNREACHABLE, HOST_UNREACHABLE, REFUSED, TOO_BIG or INVALID */
    uint8_t type, code;
    uint16_t mtu;
    nx_net_addr_t reporter;
    const uint8_t *quoted;
    size_t quoted_length, quoted_header;
};
typedef void (*nx_net_icmp_notify_fn)(const struct nx_net_icmp_notice *);
enum nx_net_result nx_net_icmp_register_notify(uint8_t protocol, nx_net_icmp_notify_fn);
void nx_net_icmp_unregister_notify(uint8_t protocol);

struct nx_net_icmp_stats {
    uint64_t received, too_short, bad_checksum, bad_errors, echoes, broadcast_echoes;
    uint64_t errors_received, notified, raw_delivered, replies_sent, errors_sent, errors_suppressed;
    uint64_t output_errors;
};
/* Clears statistics and notify registrations; raw sockets stay open. */
void nx_net_icmp_init(void);
struct nx_net_icmp_stats nx_net_icmp_stats(void);

/* IP protocol 1 input (nx_net_ip_register). */
void nx_net_icmp_input(struct nx_net_if *, struct nx_net_mbuf *, size_t header_length);
/* Send an error quoting m (a received datagram, header_length header bytes
 * contiguous) back to its source, unless the datagram is a non-first
 * fragment, was broadcast, has a non-unicast source or is itself an ICMP
 * error. mtu is used for needfrag. */
void nx_net_icmp_error(struct nx_net_if *, struct nx_net_mbuf *m, size_t header_length,
                       uint8_t type, uint8_t code, uint16_t mtu);
/* Protocol unreachable, for nx_net_ip_set_unknown_hook. */
void nx_net_icmp_unknown(struct nx_net_if *, struct nx_net_mbuf *, size_t header_length);

/* Raw ICMP sockets (BSD rip_*), 8192-byte receive queues (RIPRCVQ). Every
 * valid ICMP message the kernel does not answer itself (replies, errors and
 * unhandled types; not echo requests) is copied, with its IP header, to each
 * open socket whose bound local and connected foreign addresses match. */
#define NX_NET_RAW_SOCKETS 8u
#define NX_NET_RAW_RCVQ 8192u
struct nx_net_raw {
    bool open;
    nx_net_addr_t local, foreign;   /* 0: any */
    struct nx_net_sockbuf receive;
    void (*readable)(void *);       /* after a record is queued */
    void *argument;
};
/* DENIED unless privileged (the socket layer passes effective uid 0); FULL
 * when every socket is open. */
enum nx_net_result nx_net_raw_open(bool privileged, void (*readable)(void *), void *argument,
                                   struct nx_net_raw **);
/* Frees anything queued. */
void nx_net_raw_close(struct nx_net_raw *);
/* local must be 0 or one of our addresses (else INVALID). */
enum nx_net_result nx_net_raw_bind(struct nx_net_raw *, nx_net_addr_t local);
/* foreign 0 disconnects; otherwise it must be unicast (else INVALID). */
enum nx_net_result nx_net_raw_connect(struct nx_net_raw *, nx_net_addr_t foreign);
/* Send one ICMP message (header and data, no IP header) of 8..65515 bytes to
 * dst, or to the connected address when dst is 0 (EXISTS if both are set,
 * INVALID if neither). The kernel computes the checksum; the source is the
 * bound address or the route's. */
enum nx_net_result nx_net_raw_send(struct nx_net_raw *, struct nx_net_mbuf *message, nx_net_addr_t dst);
/* The oldest queued datagram (IP header included), or NULL. */
struct nx_net_mbuf *nx_net_raw_receive(struct nx_net_raw *);
size_t nx_net_raw_open_count(void);
/* Free every queued datagram (network stop). */
void nx_net_raw_flush(void);
#endif
