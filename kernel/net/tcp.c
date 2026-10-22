#include "net/tcp.h"
#include "net/compat.h"
#include "net/wire.h"

#define TH_FIN 0x01u
#define TH_SYN 0x02u
#define TH_RST 0x04u
#define TH_PSH 0x08u
#define TH_ACK 0x10u
#define HEADER 20u
#define MAX_WINDOW 65535u

static struct nx_net_tcp pcbs[NX_NET_TCP_SOCKETS];
static struct nx_net_tcp_stats stats;
static struct nx_net_mbuf_pool *pool;
static uint64_t (*now_ms)(void);
static uint64_t secret;
static nx_net_port_t next_ephemeral = NX_NET_TCP_EPHEMERAL_FIRST;

static void release(struct nx_net_tcp *);
static void output(struct nx_net_tcp *, bool force);

static uint32_t min32(uint32_t a, uint32_t b) { return a < b ? a : b; }
static size_t min_size(size_t a, size_t b) { return a < b ? a : b; }
static bool synchronized(const struct nx_net_tcp *t) { return t->state >= NX_NET_TCP_SYN_RECEIVED; }

void nx_net_tcp_init(struct nx_net_mbuf_pool *packets, uint64_t (*clock)(void), uint64_t key)
{
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) {
        pcbs[i].attached = false;
        if (pcbs[i].used) release(&pcbs[i]);
    }
    pool = packets;
    now_ms = clock;
    secret = key;
    stats = (struct nx_net_tcp_stats){0};
    next_ephemeral = NX_NET_TCP_EPHEMERAL_FIRST;
}
struct nx_net_tcp_stats nx_net_tcp_stats(void) { return stats; }
size_t nx_net_tcp_used(void)
{
    size_t count = 0;
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) count += pcbs[i].used;
    return count;
}

/* Tell the owner something changed; a child before accept tells its listener
 * once it is queued. */
static void event(struct nx_net_tcp *t)
{
    if (t->attached) { if (t->event) t->event(t->argument); }
    else if (t->listener && t->queued && t->listener->event) t->listener->event(t->listener->argument);
}

/* RFC 6528: a 4-microsecond clock plus a keyed hash of the connection. Not
 * cryptographic, but unpredictable without the secret. */
static uint64_t mix(uint64_t x)
{
    x ^= x >> 30; x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27; x *= UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}
static nx_net_seq_t initial_sequence(const struct nx_net_tcp *t)
{
    uint64_t tuple = (uint64_t)t->local_addr << 32 | t->foreign_addr;
    uint64_t ports = (uint64_t)t->local_port << 16 | t->foreign_port;
    return (nx_net_seq_t)(now_ms() * 250) + (nx_net_seq_t)mix(secret ^ mix(tuple ^ mix(ports)));
}

/* The MSS our route allows, and the one we send: the lesser of that and the
 * peer's (536 when it sent none). */
static size_t route_mss(nx_net_addr_t dst)
{
    struct nx_net_route route;
    if (nx_net_route_lookup(dst, &route) != NX_NET_OK || route.ifp->mtu <= 40 + NX_NET_TCP_MIN_MSS)
        return NX_NET_TCP_DEFAULT_MSS;
    return route.ifp->mtu - 40;
}
static void set_mss(struct nx_net_tcp *t, size_t peer)
{
    size_t mss = min_size(peer ? peer : NX_NET_TCP_DEFAULT_MSS, route_mss(t->foreign_addr));
    t->mss = mss < NX_NET_TCP_MIN_MSS ? NX_NET_TCP_MIN_MSS : mss;
    /* RFC 5681 initial window. */
    size_t two = 2 * t->mss > 4380 ? 2 * t->mss : 4380;
    t->cwnd = (uint32_t)min_size(4 * t->mss, two);
}
static void reset_timing(struct nx_net_tcp *t)
{
    t->rto = NX_NET_TCP_RTO_INITIAL_MS;
    t->srtt = t->rttvar = 0;
    t->ssthresh = UINT32_MAX;
}
/* The retransmission timeout with exponential backoff. */
static uint64_t backed_off(const struct nx_net_tcp *t)
{
    uint64_t rto = (uint64_t)t->rto << t->rxtshift;
    return rto > NX_NET_TCP_RTO_MAX_MS ? NX_NET_TCP_RTO_MAX_MS : rto;
}
/* RFC 6298 estimator from one sample. */
static void rtt_sample(struct nx_net_tcp *t, uint32_t r)
{
    if (!r) r = 1;
    if (!t->srtt) {
        t->srtt = r;
        t->rttvar = r / 2;
    } else {
        uint32_t delta = t->srtt > r ? t->srtt - r : r - t->srtt;
        t->rttvar = (3 * t->rttvar + delta) / 4;
        t->srtt = (7 * t->srtt + r) / 8;
    }
    uint32_t rto = t->srtt + (4 * t->rttvar > NX_NET_TCP_TIMER_MS ? 4 * t->rttvar : NX_NET_TCP_TIMER_MS);
    t->rto = rto < NX_NET_TCP_RTO_MIN_MS ? NX_NET_TCP_RTO_MIN_MS : rto > NX_NET_TCP_RTO_MAX_MS ? NX_NET_TCP_RTO_MAX_MS : rto;
}

static void clear_reass(struct nx_net_tcp *t)
{
    for (size_t i = 0; i < t->reass_count; ++i) nx_net_mbuf_freem(t->reass[i].m);
    t->reass_count = t->reass_bytes = 0;
}
/* Free a connection. An attached block stays allocated, CLOSED, for its
 * owner; any other is cleared. */
static void release(struct nx_net_tcp *t)
{
    nx_net_sockbuf_flush(&t->send);
    nx_net_sockbuf_flush(&t->receive);
    clear_reass(t);
    if (!t->attached) {
        *t = (struct nx_net_tcp){0};
        return;
    }
    t->state = NX_NET_TCP_CLOSED;
    t->rexmt_at = t->persist_at = t->wait_at = 0;
    t->ack_now = t->timing = false;
    event(t);
}

static uint16_t segment_checksum(const struct nx_net_mbuf *m, size_t offset, nx_net_addr_t src, nx_net_addr_t dst,
                                 size_t length)
{
    struct nx_net_checksum sum;
    nx_net_checksum_init(&sum);
    nx_net_checksum_pseudo(&sum, src, dst, NX_NET_IPPROTO_TCP, (uint16_t)length);
    for (; m; m = m->next) {
        if (offset >= m->len) { offset -= m->len; continue; }
        nx_net_checksum_update(&sum, m->data + offset, m->len - offset);
        offset = 0;
    }
    return nx_net_checksum_finish(&sum);
}
/* Send one segment with header fields given; payload from the send buffer. */
static void send_raw(nx_net_addr_t src, nx_net_port_t sport, nx_net_addr_t dst, nx_net_port_t dport,
                     nx_net_seq_t seq, nx_net_seq_t ack, uint8_t flags, uint16_t window, size_t mss_option,
                     struct nx_net_mbuf *data)
{
    uint8_t h[HEADER + 4] = {0};
    size_t hl = mss_option ? HEADER + 4 : HEADER;
    nx_net_put16(h, sport);
    nx_net_put16(h + 2, dport);
    nx_net_put32(h + 4, seq);
    nx_net_put32(h + 8, ack);
    h[12] = (uint8_t)(hl / 4 << 4);
    h[13] = flags;
    nx_net_put16(h + 14, window);
    if (mss_option) { h[20] = 2; h[21] = 4; nx_net_put16(h + 22, (uint16_t)min_size(mss_option, 65535)); }
    struct nx_net_mbuf *m = nx_net_mbuf_get(pool, true);
    if (!m || !nx_net_mbuf_append(m, h, hl)) {
        if (m) nx_net_mbuf_freem(m);
        if (data) nx_net_mbuf_freem(data);
        ++stats.output_errors;
        return;
    }
    if (data) nx_net_mbuf_cat(m, data);
    size_t length = nx_net_mbuf_length(m);
    nx_net_put16(m->data + 16, segment_checksum(m, 0, src, dst, length));
    struct nx_net_ip_send p = { .src = src, .dst = dst, .protocol = NX_NET_IPPROTO_TCP };
    if (nx_net_ip_output(m, &p) == NX_NET_OK) ++stats.sent;
    else ++stats.output_errors;
}
/* Receive space, less what out-of-order segments hold. */
static uint32_t receive_space(const struct nx_net_tcp *t)
{
    size_t space = nx_net_sockbuf_space(&t->receive);
    space = space > t->reass_bytes ? space - t->reass_bytes : 0;
    return (uint32_t)min_size(space, MAX_WINDOW);
}
static void emit(struct nx_net_tcp *t, uint8_t flags, nx_net_seq_t seq, size_t offset, size_t length)
{
    uint32_t window = receive_space(t);
    if (nx_net_seq_lt(t->rcv_nxt + window, t->rcv_adv)) window = t->rcv_adv - t->rcv_nxt;   /* never shrink */
    if (nx_net_seq_gt(t->rcv_nxt + window, t->rcv_adv)) t->rcv_adv = t->rcv_nxt + window;
    struct nx_net_mbuf *data = NULL;
    if (length && !(data = nx_net_mbuf_copym(t->send.head, offset, length))) { ++stats.output_errors; return; }
    if (length) ++stats.data_sent;
    send_raw(t->local_addr, t->local_port, t->foreign_addr, t->foreign_port, seq,
             (flags & TH_ACK) ? t->rcv_nxt : 0, flags, (uint16_t)window,
             (flags & TH_SYN) ? route_mss(t->foreign_addr) : 0, data);
    if (flags & TH_ACK) t->ack_now = false;
}
/* Answer a segment that has no connection (RFC 793 reset generation). */
static void reply_reset(nx_net_addr_t src, nx_net_port_t sport, nx_net_addr_t dst, nx_net_port_t dport,
                        nx_net_seq_t seq, nx_net_seq_t ack, uint8_t flags, size_t length)
{
    if (flags & TH_RST) return;
    ++stats.resets_sent;
    if (flags & TH_ACK) send_raw(dst, dport, src, sport, ack, 0, TH_RST, 0, 0, NULL);
    else send_raw(dst, dport, src, sport, 0, seq + (nx_net_seq_t)length + !!(flags & TH_SYN) + !!(flags & TH_FIN),
                  TH_RST | TH_ACK, 0, 0, NULL);
}
/* BSD tcp_drop: reset a synchronized peer and report error to the owner. */
static void drop(struct nx_net_tcp *t, enum nx_net_result error)
{
    if (synchronized(t) && t->state != NX_NET_TCP_TIME_WAIT) {
        ++stats.resets_sent;
        emit(t, TH_RST | TH_ACK, t->snd_max, 0, 0);
    }
    ++stats.dropped;
    if (t->attached) t->error = error == NX_NET_TIMED_OUT && t->soft_error != NX_NET_OK ? t->soft_error : error;
    release(t);
}

static void arm_rexmt(struct nx_net_tcp *t) { t->rexmt_at = now_ms() + backed_off(t); }
static void enter_time_wait(struct nx_net_tcp *t)
{
    t->state = NX_NET_TCP_TIME_WAIT;
    t->rexmt_at = t->persist_at = 0;
    t->wait_at = now_ms() + 2 * NX_NET_TCP_MSL_MS;
    nx_net_sockbuf_flush(&t->send);
}

/* BSD tcp_output, without urgent data or options other than MSS. force
 * sends a one-byte window probe. */
static void output(struct nx_net_tcp *t, bool force)
{
    if (t->state == NX_NET_TCP_SYN_SENT || t->state == NX_NET_TCP_SYN_RECEIVED) {
        if (t->snd_nxt == t->iss) {
            emit(t, t->state == NX_NET_TCP_SYN_SENT ? TH_SYN : TH_SYN | TH_ACK, t->iss, 0, 0);
            t->snd_nxt = t->iss + 1;
            if (nx_net_seq_gt(t->snd_nxt, t->snd_max)) t->snd_max = t->snd_nxt;
            if (!t->timing && !t->rxtshift) { t->timing = true; t->rtseq = t->iss; t->rtstart = now_ms(); }
            if (!t->rexmt_at) arm_rexmt(t);
        } else if (t->ack_now) {
            emit(t, TH_ACK, t->snd_nxt, 0, 0);
        }
        return;
    }
    if (t->state < NX_NET_TCP_ESTABLISHED) return;
    bool can_send = t->state != NX_NET_TCP_FIN_WAIT_2 && t->state != NX_NET_TCP_TIME_WAIT;
    for (;;) {
        size_t cc = t->send.cc, off = t->snd_nxt - t->snd_una, len = 0;
        uint32_t window = min32(t->snd_wnd, t->cwnd);
        if (force && !window) window = 1;
        if (can_send && off < cc) {
            size_t room = window > off ? window - off : 0;
            len = min_size(min_size(cc - off, room), t->mss);
        }
        bool fin = can_send && t->fin_wanted && !t->fin_acked && off + len == cc &&
                   (t->state == NX_NET_TCP_FIN_WAIT_1 || t->state == NX_NET_TCP_CLOSING || t->state == NX_NET_TCP_LAST_ACK);
        bool go = len == t->mss || (len && off + len == cc && (t->nodelay || t->snd_nxt == t->snd_una)) ||
                  (len && force) || (len && t->max_sndwnd && len >= t->max_sndwnd / 2) || fin || t->ack_now;
        if (!go) {
            if (can_send && cc > off && !t->snd_wnd && !t->rexmt_at && !t->persist_at)
                t->persist_at = now_ms() + NX_NET_TCP_PERSIST_MIN_MS;
            return;
        }
        nx_net_seq_t seq = t->snd_nxt;
        if (len && seq != t->snd_max) ++stats.retransmits;
        emit(t, (uint8_t)(TH_ACK | (fin ? TH_FIN : 0) | (len && off + len == cc ? TH_PSH : 0)), seq, off, len);
        if (!len && !fin) return;
        t->snd_nxt = seq + (nx_net_seq_t)len + fin;
        if (nx_net_seq_gt(t->snd_nxt, t->snd_max)) {
            if (!t->timing) { t->timing = true; t->rtseq = seq; t->rtstart = now_ms(); }
            t->snd_max = t->snd_nxt;
        }
        if (!t->rexmt_at && !force) {
            arm_rexmt(t);
            t->persist_at = 0;
        }
        if (force || len != t->mss) return;
    }
}

/* Retransmission timeout (BSD TCPT_REXMT). */
static void retransmit(struct nx_net_tcp *t)
{
    t->rexmt_at = 0;
    unsigned limit = synchronized(t) && t->state != NX_NET_TCP_SYN_RECEIVED ? NX_NET_TCP_MAXRXT : NX_NET_TCP_SYN_MAXRXT;
    if (++t->rxtshift > limit) {
        ++stats.timeouts;
        drop(t, NX_NET_TIMED_OUT);
        return;
    }
    if (!synchronized(t) || t->state == NX_NET_TCP_SYN_RECEIVED) ++stats.retransmits;   /* output counts data */
    uint32_t flight = min32(t->snd_wnd, t->cwnd) / 2;
    t->ssthresh = flight > 2 * t->mss ? flight : (uint32_t)(2 * t->mss);
    t->cwnd = (uint32_t)t->mss;
    t->dupacks = 0;
    t->recovering = t->timing = false;
    t->snd_nxt = t->snd_una;
    arm_rexmt(t);
    output(t, false);
}
static void persist(struct nx_net_tcp *t)
{
    ++stats.persist_probes;
    t->snd_nxt = t->snd_una;   /* probe with the first unacknowledged byte */
    output(t, true);
    uint64_t wait = (uint64_t)NX_NET_TCP_PERSIST_MIN_MS << (t->persist_shift < 4 ? ++t->persist_shift : 4);
    t->persist_at = t->snd_wnd ? 0 : now_ms() + (wait > NX_NET_TCP_PERSIST_MAX_MS ? NX_NET_TCP_PERSIST_MAX_MS : wait);
}
void nx_net_tcp_timer(void)
{
    uint64_t now = now_ms();
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) {
        struct nx_net_tcp *t = &pcbs[i];
        if (t->used && t->state != NX_NET_TCP_CLOSED && t->rexmt_at && now >= t->rexmt_at) retransmit(t);
        if (t->used && t->state != NX_NET_TCP_CLOSED && t->persist_at && now >= t->persist_at) persist(t);
        if (t->used && t->state != NX_NET_TCP_CLOSED && t->wait_at && now >= t->wait_at) release(t);
    }
}

/* Process our FIN being acknowledged or the peer's FIN arriving. */
static void received_fin(struct nx_net_tcp *t)
{
    t->rcv_nxt += 1;
    t->fin_received = t->ack_now = true;
    switch (t->state) {
    case NX_NET_TCP_SYN_RECEIVED: case NX_NET_TCP_ESTABLISHED: t->state = NX_NET_TCP_CLOSE_WAIT; break;
    case NX_NET_TCP_FIN_WAIT_1: t->state = NX_NET_TCP_CLOSING; break;
    case NX_NET_TCP_FIN_WAIT_2: enter_time_wait(t); break;
    default: break;
    }
    event(t);
}
/* Append in-order data; false when the buffer refused it. */
static bool accept_data(struct nx_net_tcp *t, struct nx_net_mbuf *m, size_t length)
{
    if (!length) { nx_net_mbuf_freem(m); return true; }
    if (!nx_net_sockbuf_append_stream(&t->receive, m)) return false;
    t->rcv_nxt += (nx_net_seq_t)length;
    return true;
}
/* Deliver queued segments that are now in order. */
static void pull_reass(struct nx_net_tcp *t)
{
    while (t->reass_count && !t->fin_received) {
        struct nx_net_tcp_segment *e = &t->reass[0];
        if (nx_net_seq_gt(e->seq, t->rcv_nxt)) return;
        size_t skip = t->rcv_nxt - e->seq;
        struct nx_net_tcp_segment s = *e;
        t->reass_bytes -= s.length;
        for (size_t i = 1; i < t->reass_count; ++i) t->reass[i - 1] = t->reass[i];
        --t->reass_count;
        if (skip > s.length || (skip == s.length && !s.fin)) { nx_net_mbuf_freem(s.m); continue; }
        nx_net_mbuf_adj_head(s.m, skip);
        if (!accept_data(t, s.m, s.length - skip)) return;
        if (s.fin) received_fin(t);
    }
}
/* Queue an out-of-order segment, trimming it against what is held. */
static void insert_reass(struct nx_net_tcp *t, nx_net_seq_t seq, struct nx_net_mbuf *m, size_t length, bool fin)
{
    for (size_t i = 0; i < t->reass_count && (length || fin); ++i) {
        struct nx_net_tcp_segment *e = &t->reass[i];
        nx_net_seq_t end = seq + (nx_net_seq_t)length, e_end = e->seq + (nx_net_seq_t)e->length;
        if (nx_net_seq_le(e->seq, seq) && nx_net_seq_ge(e_end, end) && (!fin || e->fin || nx_net_seq_gt(e_end, end))) {
            length = 0; fin = false;   /* already held */
        } else if (nx_net_seq_le(e->seq, seq) && nx_net_seq_gt(e_end, seq)) {
            size_t cut = e_end - seq;
            nx_net_mbuf_adj_head(m, cut);
            seq += (nx_net_seq_t)cut;
            length -= cut;
        } else if (nx_net_seq_lt(seq, e->seq) && nx_net_seq_gt(end, e->seq)) {
            size_t keep = e->seq - seq;
            nx_net_mbuf_adj_tail(m, length - keep);
            length = keep;
            fin = false;
        }
    }
    if (!length && !fin) { ++stats.duplicates; nx_net_mbuf_freem(m); return; }
    if (t->reass_count == NX_NET_TCP_REASS) { ++stats.reass_drops; nx_net_mbuf_drop(m); return; }
    size_t at = t->reass_count;
    while (at && nx_net_seq_gt(t->reass[at - 1].seq, seq)) { t->reass[at] = t->reass[at - 1]; --at; }
    t->reass[at] = (struct nx_net_tcp_segment){ seq, length, fin, m };
    ++t->reass_count;
    t->reass_bytes += length;
    ++stats.reordered;
}

static struct nx_net_tcp *lookup(nx_net_addr_t src, nx_net_port_t sport, nx_net_addr_t dst, nx_net_port_t dport)
{
    struct nx_net_tcp *listener = NULL;
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) {
        struct nx_net_tcp *t = &pcbs[i];
        if (!t->used || t->state == NX_NET_TCP_CLOSED || t->local_port != dport) continue;
        if (t->state == NX_NET_TCP_LISTEN) {
            if ((!t->local_addr || t->local_addr == dst) && (!listener || t->local_addr)) listener = t;
        } else if (t->foreign_port == sport && t->foreign_addr == src && t->local_addr == dst) {
            return t;
        }
    }
    return listener;
}
static struct nx_net_tcp *allocate(void)
{
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i)
        if (!pcbs[i].used) {
            pcbs[i] = (struct nx_net_tcp){ .used = true };
            nx_net_sockbuf_init(&pcbs[i].send, NX_NET_TCP_SNDBUF, NX_NET_TCP_SNDBUF * 4);
            nx_net_sockbuf_init(&pcbs[i].receive, NX_NET_TCP_RCVBUF, NX_NET_TCP_RCVBUF * 4);
            reset_timing(&pcbs[i]);
            return &pcbs[i];
        }
    return NULL;
}
static size_t children(const struct nx_net_tcp *listener)
{
    size_t count = 0;
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) count += pcbs[i].used && pcbs[i].listener == listener;
    return count;
}

/* A passive open from a listener (BSD tcp_input LISTEN case). */
static void listen_input(struct nx_net_tcp *l, nx_net_addr_t src, nx_net_port_t sport, nx_net_addr_t dst,
                         nx_net_port_t dport, nx_net_seq_t seq, nx_net_seq_t ack, uint8_t flags, uint16_t window,
                         size_t peer_mss, size_t length)
{
    if (flags & TH_RST) return;
    if (flags & TH_ACK) { reply_reset(src, sport, dst, dport, seq, ack, flags, length); return; }
    if (!(flags & TH_SYN)) return;
    struct nx_net_tcp *c;
    if (children(l) >= l->backlog || !(c = allocate())) { ++stats.listen_drops; return; }
    c->state = NX_NET_TCP_SYN_RECEIVED;
    c->local_addr = dst; c->local_port = dport; c->foreign_addr = src; c->foreign_port = sport;
    c->listener = l; c->reuse = l->reuse; c->nodelay = l->nodelay;
    c->irs = seq; c->rcv_nxt = c->rcv_adv = seq + 1;
    c->iss = c->snd_una = c->snd_nxt = c->snd_max = initial_sequence(c);
    c->snd_wnd = c->max_sndwnd = window; c->snd_wl1 = seq; c->snd_wl2 = c->iss;
    set_mss(c, peer_mss);
    output(c, false);
}

void nx_net_tcp_input(struct nx_net_if *ifp, struct nx_net_mbuf *m, size_t hlen)
{
    ++stats.received;
    size_t total = nx_net_mbuf_length(m);
    if (total < hlen + HEADER || hlen + HEADER > NX_NET_MLEN || !nx_net_mbuf_pullup(&m, hlen + HEADER)) {
        ++stats.too_short;
        nx_net_mbuf_drop(m);
        return;
    }
    size_t off = (size_t)(m->data[hlen + 12] >> 4) * 4;
    if (off < HEADER || off > total - hlen || hlen + off > NX_NET_MLEN || !nx_net_mbuf_pullup(&m, hlen + off)) {
        ++stats.bad_offset;
        nx_net_mbuf_drop(m);
        return;
    }
    const uint8_t *ip = m->data, *th = ip + hlen;
    nx_net_addr_t src = nx_net_get32(ip + 12), dst = nx_net_get32(ip + 16);
    if (!nx_net_inet_unicast(src) || dst == UINT32_MAX ||
        (ifp->inet_configured && ~ifp->inet_mask > 1 && dst == (ifp->inet_addr | ~ifp->inet_mask))) {
        nx_net_mbuf_drop(m);   /* TCP is unicast only */
        return;
    }
    if (segment_checksum(m, hlen, src, dst, total - hlen)) { ++stats.bad_checksum; nx_net_mbuf_drop(m); return; }
    nx_net_port_t sport = nx_net_get16(th), dport = nx_net_get16(th + 2);
    nx_net_seq_t seq = nx_net_get32(th + 4), ack = nx_net_get32(th + 8);
    uint8_t flags = th[13] & 0x3f;
    uint16_t window = nx_net_get16(th + 14);
    size_t peer_mss = 0;
    for (size_t i = HEADER; (flags & TH_SYN) && i < off;) {
        uint8_t kind = th[i];
        if (kind == 0) break;
        if (kind == 1) { ++i; continue; }
        if (i + 1 >= off || th[i + 1] < 2 || i + th[i + 1] > off) break;
        if (kind == 2 && th[i + 1] == 4) peer_mss = nx_net_get16(th + i + 2);
        i += th[i + 1];
    }
    size_t tlen = total - hlen - off;
    nx_net_mbuf_adj_head(m, hlen + off);
    struct nx_net_tcp *t = lookup(src, sport, dst, dport);
    if (!t) {
        ++stats.no_socket;
        reply_reset(src, sport, dst, dport, seq, ack, flags, tlen);
        nx_net_mbuf_freem(m);
        return;
    }
    if (t->state == NX_NET_TCP_LISTEN) {
        listen_input(t, src, sport, dst, dport, seq, ack, flags, window, peer_mss, tlen);
        nx_net_mbuf_freem(m);
        return;
    }
    if (t->state == NX_NET_TCP_SYN_SENT) {
        nx_net_mbuf_freem(m);   /* data on a SYN is not kept; the peer retransmits it */
        if ((flags & TH_ACK) && (nx_net_seq_le(ack, t->iss) || nx_net_seq_gt(ack, t->snd_max))) {
            reply_reset(src, sport, dst, dport, seq, ack, flags, tlen);
            return;
        }
        if (flags & TH_RST) {
            if (flags & TH_ACK) { ++stats.resets_received; drop(t, NX_NET_REFUSED); }
            return;
        }
        if (!(flags & TH_SYN)) return;
        t->irs = seq;
        t->rcv_nxt = t->rcv_adv = seq + 1;
        set_mss(t, peer_mss);
        t->snd_wnd = t->max_sndwnd = window; t->snd_wl1 = seq; t->snd_wl2 = ack;
        t->ack_now = true;
        if (!(flags & TH_ACK)) {   /* simultaneous open */
            t->state = NX_NET_TCP_SYN_RECEIVED;
            t->snd_nxt = t->iss;
            t->rexmt_at = 0;
            output(t, false);
            return;
        }
        if (t->timing && nx_net_seq_gt(ack, t->rtseq)) rtt_sample(t, (uint32_t)(now_ms() - t->rtstart));
        t->timing = false;
        t->snd_una = ack;
        t->rxtshift = 0;
        t->rexmt_at = 0;
        t->state = NX_NET_TCP_ESTABLISHED;
        output(t, false);
        event(t);
        return;
    }

    /* Synchronized states. RFC 5961: a reset must hit rcv_nxt exactly; one
     * elsewhere in the window, or any SYN, draws a challenge ACK. */
    uint32_t rcv_wnd = receive_space(t);
    bool in_window = nx_net_seq_ge(seq, t->rcv_nxt) && nx_net_seq_lt(seq, t->rcv_nxt + (rcv_wnd ? rcv_wnd : 1));
    if (flags & TH_RST) {
        nx_net_mbuf_freem(m);
        if (seq == t->rcv_nxt) {
            ++stats.resets_received;
            enum nx_net_result error = t->state == NX_NET_TCP_SYN_RECEIVED ? NX_NET_REFUSED : NX_NET_RESET;
            if (t->state >= NX_NET_TCP_CLOSING && t->state != NX_NET_TCP_FIN_WAIT_2) error = NX_NET_OK;
            ++stats.dropped;
            if (t->attached && error != NX_NET_OK) t->error = error;
            release(t);
        } else if (in_window) {
            ++stats.challenge_acks;
            emit(t, TH_ACK, t->snd_nxt, 0, 0);
        }
        return;
    }
    if (flags & TH_SYN) {
        nx_net_mbuf_freem(m);
        if (t->state == NX_NET_TCP_SYN_RECEIVED && seq == t->irs) {   /* our SYN-ACK was lost */
            t->snd_nxt = t->snd_una;
            output(t, false);
        } else {
            ++stats.challenge_acks;
            emit(t, TH_ACK, t->snd_nxt, 0, 0);
        }
        return;
    }
    if (!(flags & TH_ACK)) { nx_net_mbuf_freem(m); return; }

    /* Trim to the receive window. */
    bool fin = flags & TH_FIN;
    nx_net_seq_t s = seq;
    if (nx_net_seq_lt(s, t->rcv_nxt)) {
        size_t todrop = t->rcv_nxt - s;
        ++stats.duplicates;
        t->ack_now = true;
        if (todrop > tlen || (todrop == tlen && !fin)) {
            nx_net_mbuf_adj_head(m, tlen);
            tlen = 0;
            fin = false;
            s = t->rcv_nxt;
        } else {
            nx_net_mbuf_adj_head(m, todrop);
            tlen -= todrop;
            s = t->rcv_nxt;
        }
    }
    if (tlen || fin) {
        size_t start = s - t->rcv_nxt;
        if (start >= rcv_wnd && (tlen || start > rcv_wnd)) {
            if (start || rcv_wnd) ++stats.out_of_window;   /* else a zero-window probe */
            t->ack_now = true;
            nx_net_mbuf_adj_tail(m, tlen);
            tlen = 0;
            fin = false;
        } else if (start + tlen > rcv_wnd) {
            nx_net_mbuf_adj_tail(m, start + tlen - rcv_wnd);
            tlen = rcv_wnd - start;
            fin = false;
            t->ack_now = true;
        }
    }
    /* After close, new data has nowhere to go (BSD SS_NOFDREF). */
    if (!t->attached && !t->listener && tlen) {
        nx_net_mbuf_freem(m);
        drop(t, NX_NET_RESET);
        return;
    }

    if (t->state == NX_NET_TCP_SYN_RECEIVED) {
        if (nx_net_seq_le(ack, t->snd_una) || nx_net_seq_gt(ack, t->snd_max)) {
            nx_net_mbuf_freem(m);
            reply_reset(src, sport, dst, dport, seq, ack, flags, tlen);
            return;
        }
        t->state = NX_NET_TCP_ESTABLISHED;
        t->snd_wnd = t->max_sndwnd = window; t->snd_wl1 = seq; t->snd_wl2 = ack;
        t->snd_una += 1;   /* the SYN */
        if (t->timing && nx_net_seq_gt(ack, t->rtseq)) rtt_sample(t, (uint32_t)(now_ms() - t->rtstart));
        t->timing = false;
        t->rxtshift = 0;
        if (t->snd_una == t->snd_max) t->rexmt_at = 0;
        if (t->listener) t->queued = true;
        event(t);
    }

    /* Acknowledgment processing (BSD tcp_input, Reno fast retransmit). */
    if (nx_net_seq_le(ack, t->snd_una)) {
        if (!tlen && !fin && window == t->snd_wnd && t->rexmt_at && ack == t->snd_una) {
            if (++t->dupacks == 3) {
                ++stats.fast_retransmits;
                uint32_t flight = min32(t->snd_wnd, t->cwnd) / 2;
                t->ssthresh = flight > 2 * t->mss ? flight : (uint32_t)(2 * t->mss);
                t->recover = t->snd_max;
                t->recovering = true;
                nx_net_seq_t next = t->snd_nxt;
                t->rexmt_at = 0;
                t->timing = false;
                t->snd_nxt = ack;
                t->cwnd = (uint32_t)t->mss;
                output(t, false);
                t->cwnd = t->ssthresh + 3 * (uint32_t)t->mss;
                if (nx_net_seq_gt(next, t->snd_nxt)) t->snd_nxt = next;
                nx_net_mbuf_freem(m);
                return;
            }
            if (t->dupacks > 3 && t->recovering) {
                t->cwnd += (uint32_t)t->mss;
                output(t, false);
                nx_net_mbuf_freem(m);
                return;
            }
        } else if (nx_net_seq_lt(ack, t->snd_una) || tlen || fin) {
            /* data or old acknowledgments leave the count alone */
        } else {
            t->dupacks = 0;
        }
    } else if (nx_net_seq_gt(ack, t->snd_max)) {
        nx_net_mbuf_freem(m);
        t->ack_now = true;
        output(t, false);
        return;
    } else {
        size_t acked = ack - t->snd_una;
        if (t->timing && nx_net_seq_gt(ack, t->rtseq)) {
            rtt_sample(t, (uint32_t)(now_ms() - t->rtstart));
            t->timing = false;
        }
        t->rxtshift = 0;
        if (t->recovering) {
            t->recovering = false;
            t->cwnd = t->ssthresh;
        } else if (t->cwnd < t->ssthresh) {
            t->cwnd += (uint32_t)t->mss;
        } else {
            uint32_t step = (uint32_t)(t->mss * t->mss / t->cwnd);
            t->cwnd += step ? step : 1;
        }
        if (t->cwnd > (1u << 20)) t->cwnd = 1u << 20;
        t->dupacks = 0;
        bool fin_acked = false;
        if (acked > t->send.cc) {
            fin_acked = t->fin_wanted;
            nx_net_sockbuf_consume(&t->send, t->send.cc);
        } else {
            nx_net_sockbuf_consume(&t->send, acked);
        }
        t->snd_una = ack;
        if (nx_net_seq_lt(t->snd_nxt, t->snd_una)) t->snd_nxt = t->snd_una;
        if (ack == t->snd_max) t->rexmt_at = 0;
        else arm_rexmt(t);
        event(t);
        if (fin_acked) {
            t->fin_acked = true;
            switch (t->state) {
            case NX_NET_TCP_FIN_WAIT_1:
                t->state = NX_NET_TCP_FIN_WAIT_2;
                if (!t->attached) t->wait_at = now_ms() + NX_NET_TCP_FIN_WAIT_2_MS;
                break;
            case NX_NET_TCP_CLOSING: enter_time_wait(t); break;
            case NX_NET_TCP_LAST_ACK: nx_net_mbuf_freem(m); release(t); return;
            default: break;
            }
        }
    }

    /* Window update (RFC 793 SND.WL1/WL2), which also lets a window shrink. */
    if (nx_net_seq_lt(t->snd_wl1, seq) || (t->snd_wl1 == seq && nx_net_seq_le(t->snd_wl2, ack))) {
        t->snd_wnd = window;
        t->snd_wl1 = seq;
        t->snd_wl2 = ack;
        if (window > t->max_sndwnd) t->max_sndwnd = window;
        if (window) { t->persist_at = 0; t->persist_shift = 0; }
    }

    /* Data and FIN. */
    bool receiving = t->state == NX_NET_TCP_ESTABLISHED || t->state == NX_NET_TCP_FIN_WAIT_1 ||
                     t->state == NX_NET_TCP_FIN_WAIT_2;
    if ((tlen || fin) && receiving) {
        t->ack_now = true;
        if (s == t->rcv_nxt) {
            if (accept_data(t, m, tlen)) {
                pull_reass(t);
                if (fin && !t->fin_received) {
                    clear_reass(t);   /* nothing can follow the FIN */
                    received_fin(t);
                }
                event(t);
            }
        } else {
            insert_reass(t, s, m, tlen, fin);
        }
    } else {
        if (t->state == NX_NET_TCP_TIME_WAIT && (flags & TH_FIN)) {
            t->wait_at = now_ms() + 2 * NX_NET_TCP_MSL_MS;   /* a retransmitted FIN */
            t->ack_now = true;
        }
        nx_net_mbuf_freem(m);
    }
    if (t->used && t->state != NX_NET_TCP_CLOSED) output(t, false);
}

/* ICMP errors (BSD tcp_ctlinput/tcp_notify, RFC 5927 sequence check). */
void nx_net_tcp_notify(const struct nx_net_icmp_notice *n)
{
    const uint8_t *q = n->quoted, *th = q + n->quoted_header;
    nx_net_addr_t src = nx_net_get32(q + 12), dst = nx_net_get32(q + 16);
    nx_net_port_t sport = nx_net_get16(th), dport = nx_net_get16(th + 2);
    nx_net_seq_t seq = nx_net_get32(th + 4);
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) {
        struct nx_net_tcp *t = &pcbs[i];
        if (!t->used || t->state <= NX_NET_TCP_LISTEN || t->local_addr != src || t->local_port != sport ||
            t->foreign_addr != dst || t->foreign_port != dport)
            continue;
        if (nx_net_seq_lt(seq, t->snd_una) || nx_net_seq_ge(seq, t->snd_max)) return;
        if (n->error == NX_NET_TOO_BIG) {
            size_t mss = n->mtu > 40 + NX_NET_TCP_MIN_MSS ? n->mtu - 40u : NX_NET_TCP_MIN_MSS;
            if (mss < t->mss) {
                t->mss = mss;
                t->snd_nxt = t->snd_una;
                output(t, false);
            }
        } else if (t->state == NX_NET_TCP_SYN_SENT && n->error == NX_NET_REFUSED) {
            drop(t, NX_NET_REFUSED);
        } else {
            t->soft_error = n->error;   /* reported if the connection times out */
        }
        return;
    }
}

void nx_net_tcp_flush(void)
{
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i)
        if (pcbs[i].used && pcbs[i].state != NX_NET_TCP_CLOSED) release(&pcbs[i]);
}

enum nx_net_result nx_net_tcp_open(void (*callback)(void *), void *argument, struct nx_net_tcp **out)
{
    struct nx_net_tcp *t = allocate();
    if (!t) return NX_NET_FULL;
    t->attached = true;
    t->event = callback;
    t->argument = argument;
    *out = t;
    return NX_NET_OK;
}
void nx_net_tcp_set_reuse(struct nx_net_tcp *t, bool reuse) { t->reuse = reuse; }
void nx_net_tcp_set_nodelay(struct nx_net_tcp *t, bool nodelay)
{
    t->nodelay = nodelay;
    if (nodelay && t->state >= NX_NET_TCP_ESTABLISHED) output(t, false);
}

static bool conflicts(const struct nx_net_tcp *self, nx_net_addr_t addr, nx_net_port_t port)
{
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) {
        const struct nx_net_tcp *t = &pcbs[i];
        if (t == self || !t->used || t->local_port != port) continue;
        if ((!t->local_addr || !addr || t->local_addr == addr) && !(t->reuse && self->reuse)) return true;
    }
    return false;
}
static bool port_used(nx_net_port_t port)
{
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i)
        if (pcbs[i].used && pcbs[i].local_port == port) return true;
    return false;
}
enum nx_net_result nx_net_tcp_bind(struct nx_net_tcp *t, nx_net_addr_t addr, nx_net_port_t port, bool privileged)
{
    if (t->local_port || (addr && !nx_net_inet_local(addr))) return NX_NET_INVALID;
    if (!port) {
        for (unsigned tries = 0; tries <= NX_NET_TCP_EPHEMERAL_LAST - NX_NET_TCP_EPHEMERAL_FIRST; ++tries) {
            nx_net_port_t candidate = next_ephemeral;
            next_ephemeral = candidate == NX_NET_TCP_EPHEMERAL_LAST ? NX_NET_TCP_EPHEMERAL_FIRST
                                                                    : (nx_net_port_t)(candidate + 1);
            if (!port_used(candidate)) { port = candidate; break; }
        }
        if (!port) return NX_NET_FULL;
    } else if (port < 1024 && !privileged) {
        return NX_NET_DENIED;
    } else if (conflicts(t, addr, port)) {
        return NX_NET_EXISTS;
    }
    t->local_addr = addr;
    t->local_port = port;
    return NX_NET_OK;
}
enum nx_net_result nx_net_tcp_listen(struct nx_net_tcp *t, size_t backlog)
{
    if (t->state != NX_NET_TCP_CLOSED || t->foreign_port) return NX_NET_INVALID;
    if (!t->local_port) {
        enum nx_net_result result = nx_net_tcp_bind(t, 0, 0, false);
        if (result != NX_NET_OK) return result;
    }
    t->backlog = backlog < 1 ? 1 : backlog > NX_NET_TCP_BACKLOG ? NX_NET_TCP_BACKLOG : backlog;
    t->state = NX_NET_TCP_LISTEN;
    return NX_NET_OK;
}
enum nx_net_result nx_net_tcp_accept(struct nx_net_tcp *l, void (*callback)(void *), void *argument,
                                     struct nx_net_tcp **out)
{
    if (l->state != NX_NET_TCP_LISTEN) return NX_NET_INVALID;
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) {
        struct nx_net_tcp *c = &pcbs[i];
        if (!c->used || c->listener != l || !c->queued) continue;
        c->listener = NULL;
        c->queued = false;
        c->attached = true;
        c->event = callback;
        c->argument = argument;
        ++stats.accepts;
        *out = c;
        return NX_NET_OK;
    }
    return NX_NET_AGAIN;
}
enum nx_net_result nx_net_tcp_connect(struct nx_net_tcp *t, nx_net_addr_t addr, nx_net_port_t port)
{
    if (t->state != NX_NET_TCP_CLOSED || t->foreign_port || !nx_net_inet_unicast(addr) || !port) return NX_NET_INVALID;
    struct nx_net_route route;
    enum nx_net_result result = nx_net_route_lookup(addr, &route);
    if (result != NX_NET_OK) return result;
    nx_net_addr_t local = t->local_addr ? t->local_addr : route.source;
    if (!t->local_port && (result = nx_net_tcp_bind(t, 0, 0, false)) != NX_NET_OK) return result;
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) {
        const struct nx_net_tcp *o = &pcbs[i];
        if (o != t && o->used && o->state != NX_NET_TCP_CLOSED && o->local_port == t->local_port &&
            o->foreign_port == port && o->foreign_addr == addr && o->local_addr == local)
            return NX_NET_EXISTS;
    }
    t->local_addr = local;
    t->foreign_addr = addr;
    t->foreign_port = port;
    t->iss = t->snd_una = t->snd_nxt = t->snd_max = initial_sequence(t);
    set_mss(t, route_mss(addr));
    reset_timing(t);
    t->state = NX_NET_TCP_SYN_SENT;
    ++stats.connects;
    output(t, false);
    return NX_NET_OK;
}

static bool connected(const struct nx_net_tcp *t)
{
    return t->state == NX_NET_TCP_ESTABLISHED || t->state == NX_NET_TCP_CLOSE_WAIT;
}
enum nx_net_result nx_net_tcp_write(struct nx_net_tcp *t, const void *data, size_t length, size_t *done)
{
    *done = 0;
    if (t->error != NX_NET_OK) return t->error;
    if (t->fin_wanted) return NX_NET_INVALID;
    if (!connected(t)) return NX_NET_NOT_CONNECTED;
    size_t n = min_size(length, nx_net_sockbuf_space(&t->send));
    if (!n) return length ? NX_NET_AGAIN : NX_NET_OK;
    struct nx_net_mbuf *m = nx_net_mbuf_get(pool, true);
    if (!m || !nx_net_mbuf_append(m, data, n)) {
        if (m) nx_net_mbuf_freem(m);
        return NX_NET_NO_BUFFERS;
    }
    if (!nx_net_sockbuf_append_stream(&t->send, m)) return NX_NET_NO_BUFFERS;
    *done = n;
    output(t, false);
    return NX_NET_OK;
}
enum nx_net_result nx_net_tcp_read(struct nx_net_tcp *t, void *data, size_t length, size_t *done)
{
    *done = 0;
    if (t->receive.cc) {
        size_t n = min_size(length, t->receive.cc);
        nx_net_mbuf_copydata(t->receive.head, 0, n, data);
        nx_net_sockbuf_consume(&t->receive, n);
        *done = n;
        /* Advertise a window that opened by two segments or half the buffer. */
        if (synchronized(t)) {
            uint32_t advertised = t->rcv_adv - t->rcv_nxt, now = receive_space(t);
            if (now > advertised && now - advertised >= min_size(2 * t->mss, NX_NET_TCP_RCVBUF / 2)) {
                t->ack_now = true;
                output(t, false);
            }
        }
        return NX_NET_OK;
    }
    if (t->fin_received) return NX_NET_OK;
    if (t->error != NX_NET_OK) return t->error;
    if (t->state == NX_NET_TCP_CLOSED || t->state == NX_NET_TCP_LISTEN) return NX_NET_NOT_CONNECTED;
    return NX_NET_AGAIN;
}
enum nx_net_result nx_net_tcp_shutdown(struct nx_net_tcp *t)
{
    switch (t->state) {
    case NX_NET_TCP_ESTABLISHED: t->state = NX_NET_TCP_FIN_WAIT_1; break;
    case NX_NET_TCP_CLOSE_WAIT: t->state = NX_NET_TCP_LAST_ACK; break;
    case NX_NET_TCP_SYN_SENT: release(t); return NX_NET_OK;
    case NX_NET_TCP_CLOSED: case NX_NET_TCP_LISTEN: return NX_NET_NOT_CONNECTED;
    default: return NX_NET_OK;   /* already shutting down */
    }
    t->fin_wanted = true;
    output(t, false);
    return NX_NET_OK;
}
void nx_net_tcp_abort(struct nx_net_tcp *t)
{
    if (t->state == NX_NET_TCP_LISTEN)
        for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i)
            if (pcbs[i].used && pcbs[i].listener == t) drop(&pcbs[i], NX_NET_RESET);
    t->attached = false;
    if (synchronized(t) && t->state != NX_NET_TCP_TIME_WAIT) drop(t, NX_NET_RESET);
    else release(t);
}
void nx_net_tcp_close(struct nx_net_tcp *t)
{
    if (t->state == NX_NET_TCP_LISTEN || t->state == NX_NET_TCP_CLOSED || t->state == NX_NET_TCP_SYN_SENT ||
        t->receive.cc) {
        nx_net_tcp_abort(t);   /* RFC 2525: unread data resets the peer */
        return;
    }
    nx_net_tcp_shutdown(t);
    t->attached = false;
    t->event = NULL;
    if (t->state == NX_NET_TCP_FIN_WAIT_2) t->wait_at = now_ms() + NX_NET_TCP_FIN_WAIT_2_MS;
}
enum nx_net_result nx_net_tcp_take_error(struct nx_net_tcp *t)
{
    enum nx_net_result error = t->error;
    t->error = NX_NET_OK;
    return error;
}
size_t nx_net_tcp_readable(const struct nx_net_tcp *t) { return t->receive.cc; }
size_t nx_net_tcp_writable(const struct nx_net_tcp *t) { return connected(t) && !t->fin_wanted ? nx_net_sockbuf_space(&t->send) : 0; }
