#ifndef NEXIX_NET_SOCKET_H
#define NEXIX_NET_SOCKET_H
#include "net/tcp.h"
#include "net/udp.h"

/* Socket objects behind file descriptors (task 425). Each wraps one TCP, UDP
 * or raw ICMP control block and lives in the 16-page socket-object budget.
 * Every function below runs in process context and performs its protocol work
 * on the network worker; none sleeps except for that call. Results are 0 or
 * a byte count, else a negative native errno; -NX_EAGAIN means "wait for
 * events to change and retry". The first create starts the stack, detached
 * from the caller, with lo0 at 127.0.0.1/8; nothing else is configured.
 */
struct nx_socket {
    uint8_t type;               /* NX_SOCK_STREAM, NX_SOCK_DGRAM or NX_SOCK_RAW */
    bool connected_once;        /* a stream that reached ESTABLISHED */
    bool connecting;
    bool shut_read, shut_write;
    union { struct nx_net_tcp *tcp; struct nx_net_udp *udp; struct nx_net_raw *raw; } pcb;
    volatile uint32_t events;   /* bumped, with a wakeup on this object, by protocol events */
    uint64_t send_timeout, receive_timeout;   /* ticks, 0 = none (SO_SNDTIMEO/SO_RCVTIMEO) */
    uint32_t send_size;         /* SO_SNDBUF of a datagram/raw socket, which has no send buffer */
};
_Static_assert(sizeof(struct nx_socket) <= 512, "socket object budget");

int net_socket_create(int type, bool privileged, struct nx_socket **);
/* Final close: graceful for streams (unread data resets the peer). */
void net_socket_release(struct nx_socket *);
size_t net_socket_count(void);

/* Addresses and ports are host order. */
int net_socket_bind(struct nx_socket *, nx_net_addr_t, nx_net_port_t, bool privileged);
int net_socket_listen(struct nx_socket *, int backlog);
/* -NX_EAGAIN while no connection is queued. */
int net_socket_accept(struct nx_socket *, struct nx_socket **, nx_net_addr_t *, nx_net_port_t *);
/* Start (or, for datagram/raw, set) a connection; addr 0 disconnects a
 * datagram socket. Streams then poll net_socket_connected. */
int net_socket_connect(struct nx_socket *, nx_net_addr_t, nx_net_port_t);
/* 0 once established, -NX_EAGAIN while in progress, else the failure. */
int net_socket_connected(struct nx_socket *);
/* Stream data: bytes accepted or read (0 = end of stream), -NX_EAGAIN when
 * the buffer is full or empty. A send that can never succeed returns
 * -NX_EPIPE; the caller raises SIGPIPE. */
int64_t net_socket_send(struct nx_socket *, const void *, size_t);
int64_t net_socket_receive(struct nx_socket *, void *, size_t, bool peek);
/* Datagram/raw sockets (task 426). stage appends caller bytes to *chain
 * (created on first use), discard frees one; send consumes the chain and
 * sends it to addr:port when addressed, else to the connected peer.
 * receive hands over the oldest datagram (a copy with peek): UDP payload,
 * or the whole IP datagram for raw sockets; *chain is NULL after SHUT_RD.
 * The caller may read the chain directly and must discard it. */
int net_socket_stage(struct nx_net_mbuf **chain, const void *, size_t);
void net_socket_discard(struct nx_net_mbuf *);
int net_socket_send_datagram(struct nx_socket *, struct nx_net_mbuf *, bool addressed, nx_net_addr_t, nx_net_port_t);
int net_socket_receive_datagram(struct nx_socket *, bool peek, struct nx_net_mbuf **, nx_net_addr_t *, nx_net_port_t *);
/* Options (task 426): value is an int32 or, for the timeouts, an
 * nx_timeval, already validated by the syscall layer. */
int net_socket_set_option(struct nx_socket *, int level, int name, const void *value);
int net_socket_get_option(struct nx_socket *, int level, int name, void *value);
int net_socket_shutdown(struct nx_socket *, int how);
int net_socket_name(struct nx_socket *, bool peer, nx_net_addr_t *, nx_net_port_t *);
#endif
