#ifndef NEXIX_NET_TCP_H
#define NEXIX_NET_TCP_H
#include "net/icmp.h"

/* TCP adapted from 4.4BSD netinet/tcp_input.c, tcp_output.c, tcp_timer.c,
 * tcp_subr.c and tcp_usrreq.c (task 423). A fixed table of control blocks
 * covers listeners, embryonic and queued connections and connections still
 * closing after their owner let go. Only the MSS option is used; there is no
 * window scaling, timestamps, SACK, urgent data or keepalive. Times are
 * milliseconds from the clock given at init; nx_net_tcp_timer runs every
 * NX_NET_TCP_TIMER_MS. Worker context only; every function that takes an mbuf
 * chain consumes it.
 */
#define NX_NET_IPPROTO_TCP 6u
#define NX_NET_TCP_SOCKETS 32u
#define NX_NET_TCP_SNDBUF 16384u
#define NX_NET_TCP_RCVBUF 16384u
#define NX_NET_TCP_BACKLOG 8u             /* largest listen queue */
#define NX_NET_TCP_REASS 16u              /* out-of-order segments held per connection */
#define NX_NET_TCP_DEFAULT_MSS 536u       /* RFC 1122 when the peer sends none */
#define NX_NET_TCP_MIN_MSS 64u
#define NX_NET_TCP_TIMER_MS 100u
#define NX_NET_TCP_RTO_INITIAL_MS 1000u   /* RFC 6298 */
#define NX_NET_TCP_RTO_MIN_MS 200u
#define NX_NET_TCP_RTO_MAX_MS 64000u      /* BSD TCPTV_REXMTMAX */
#define NX_NET_TCP_MAXRXT 12u             /* BSD TCP_MAXRXTSHIFT */
#define NX_NET_TCP_SYN_MAXRXT 6u          /* about 63 s for a connect, under BSD's 75 s */
#define NX_NET_TCP_PERSIST_MIN_MS 5000u   /* BSD TCPTV_PERSMIN */
#define NX_NET_TCP_PERSIST_MAX_MS 60000u
#define NX_NET_TCP_MSL_MS 30000u          /* BSD TCPTV_MSL; TIME_WAIT lasts twice this */
#define NX_NET_TCP_FIN_WAIT_2_MS 60000u   /* after close, a silent peer is dropped */
#define NX_NET_TCP_EPHEMERAL_FIRST 49152u
#define NX_NET_TCP_EPHEMERAL_LAST 65535u

enum nx_net_tcp_state {
    NX_NET_TCP_CLOSED, NX_NET_TCP_LISTEN, NX_NET_TCP_SYN_SENT, NX_NET_TCP_SYN_RECEIVED,
    NX_NET_TCP_ESTABLISHED, NX_NET_TCP_CLOSE_WAIT, NX_NET_TCP_FIN_WAIT_1, NX_NET_TCP_CLOSING,
    NX_NET_TCP_LAST_ACK, NX_NET_TCP_FIN_WAIT_2, NX_NET_TCP_TIME_WAIT
};

struct nx_net_tcp_segment { nx_net_seq_t seq; size_t length; bool fin; struct nx_net_mbuf *m; };
struct nx_net_tcp {
    bool used, attached, reuse, nodelay, queued;
    enum nx_net_tcp_state state;
    nx_net_addr_t local_addr, foreign_addr;
    nx_net_port_t local_port, foreign_port;
    struct nx_net_tcp *listener;      /* embryonic or queued child of this listener */
    size_t backlog;
    /* Send sequence space (RFC 793). Buffered byte i is sequence snd_una + i
     * once the SYN is acknowledged. */
    nx_net_seq_t iss, snd_una, snd_nxt, snd_max, snd_wl1, snd_wl2;
    uint32_t snd_wnd, max_sndwnd, cwnd, ssthresh;
    size_t mss;                       /* largest segment we send */
    unsigned dupacks;
    bool fin_wanted, fin_acked, recovering;
    nx_net_seq_t recover;
    /* Receive sequence space. */
    nx_net_seq_t irs, rcv_nxt, rcv_adv;
    bool fin_received, ack_now;
    struct nx_net_sockbuf send, receive;
    struct nx_net_tcp_segment reass[NX_NET_TCP_REASS];
    size_t reass_count, reass_bytes;
    /* Round-trip timing and timers (absolute ms, 0 = off). */
    bool timing;
    nx_net_seq_t rtseq;
    uint64_t rtstart, rexmt_at, persist_at, wait_at;
    uint32_t srtt, rttvar, rto;       /* ms; srtt 0 until the first sample */
    unsigned rxtshift, persist_shift;
    enum nx_net_result error, soft_error;
    void (*event)(void *);            /* state, data, space or error changed */
    void *argument;
};

struct nx_net_tcp_stats {
    uint64_t received, too_short, bad_checksum, bad_offset, no_socket, resets_sent, resets_received;
    uint64_t listen_drops, out_of_window, duplicates, reordered, reass_drops, challenge_acks;
    uint64_t sent, data_sent, retransmits, timeouts, fast_retransmits, persist_probes, dropped;
    uint64_t connects, accepts, output_errors;
};

/* Clears statistics and frees every connection; headers are allocated from
 * pool. secret keys initial sequence numbers (RFC 6528); the clock must be
 * monotonic. */
void nx_net_tcp_init(struct nx_net_mbuf_pool *, uint64_t (*now_ms)(void), uint64_t secret);
struct nx_net_tcp_stats nx_net_tcp_stats(void);
void nx_net_tcp_input(struct nx_net_if *, struct nx_net_mbuf *, size_t header_length);
void nx_net_tcp_notify(const struct nx_net_icmp_notice *);
void nx_net_tcp_timer(void);
/* Free every connection, attached ones becoming CLOSED (network stop). */
void nx_net_tcp_flush(void);
size_t nx_net_tcp_used(void);

/* FULL when no control block is free. */
enum nx_net_result nx_net_tcp_open(void (*event)(void *), void *argument, struct nx_net_tcp **);
void nx_net_tcp_set_reuse(struct nx_net_tcp *, bool);
void nx_net_tcp_set_nodelay(struct nx_net_tcp *, bool);
/* As nx_net_udp_bind: once, our address or 0, privileged ports below 1024,
 * EXISTS on overlap unless both reuse, port 0 ephemeral. */
enum nx_net_result nx_net_tcp_bind(struct nx_net_tcp *, nx_net_addr_t addr, nx_net_port_t port, bool privileged);
/* backlog is clamped to 1..NX_NET_TCP_BACKLOG; binds an ephemeral port when
 * unbound. INVALID unless CLOSED. */
enum nx_net_result nx_net_tcp_listen(struct nx_net_tcp *, size_t backlog);
/* The oldest established connection, now owned by the caller, or AGAIN. */
enum nx_net_result nx_net_tcp_accept(struct nx_net_tcp *, void (*event)(void *), void *argument,
                                     struct nx_net_tcp **);
/* Start an active open to a unicast address and nonzero port (INVALID), from
 * a CLOSED, never-connected block; EXISTS when the connection already exists.
 * Completion or failure is reported through the event and state. */
enum nx_net_result nx_net_tcp_connect(struct nx_net_tcp *, nx_net_addr_t addr, nx_net_port_t port);
/* Copy up to length bytes into the send buffer (*done set). AGAIN when it is
 * full, NOT_CONNECTED before establishment, INVALID after shutdown, or the
 * pending error. */
enum nx_net_result nx_net_tcp_write(struct nx_net_tcp *, const void *data, size_t length, size_t *done);
/* Copy up to length received bytes out (*done set). OK with *done 0 means
 * end of stream; AGAIN when nothing has arrived yet. */
enum nx_net_result nx_net_tcp_read(struct nx_net_tcp *, void *data, size_t length, size_t *done);
/* Send FIN after buffered data; reading continues. */
enum nx_net_result nx_net_tcp_shutdown(struct nx_net_tcp *);
/* Give the block up. Unread data or a listener's pending children are reset;
 * otherwise the connection closes gracefully in the background. */
void nx_net_tcp_close(struct nx_net_tcp *);
/* Reset the connection at once and give the block up. */
void nx_net_tcp_abort(struct nx_net_tcp *);
/* Return and clear the pending error (OK when none). */
enum nx_net_result nx_net_tcp_take_error(struct nx_net_tcp *);
size_t nx_net_tcp_readable(const struct nx_net_tcp *);
size_t nx_net_tcp_writable(const struct nx_net_tcp *);
#endif
