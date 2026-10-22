#include "net/udp.h"
#include "net/compat.h"
#include "net/wire.h"

#define META 8u   /* queued before each payload: source address, port, padding */

static struct nx_net_udp sockets[NX_NET_UDP_SOCKETS];
static struct nx_net_udp_stats stats;
static nx_net_port_t next_ephemeral = NX_NET_UDP_EPHEMERAL_FIRST;

void nx_net_udp_init(void)
{
    for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i) if (sockets[i].open) nx_net_udp_close(&sockets[i]);
    stats = (struct nx_net_udp_stats){0};
    next_ephemeral = NX_NET_UDP_EPHEMERAL_FIRST;
}
struct nx_net_udp_stats nx_net_udp_stats(void) { return stats; }
size_t nx_net_udp_open_count(void)
{
    size_t count = 0;
    for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i) count += sockets[i].open;
    return count;
}
void nx_net_udp_flush(void)
{
    for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i)
        if (sockets[i].open) nx_net_sockbuf_flush(&sockets[i].receive);
}

enum nx_net_result nx_net_udp_open(void (*readable)(void *), void *argument, struct nx_net_udp **out)
{
    for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i)
        if (!sockets[i].open) {
            sockets[i] = (struct nx_net_udp){ .open = true, .readable = readable, .argument = argument };
            nx_net_sockbuf_init(&sockets[i].receive, NX_NET_UDP_RCVQ, NX_NET_UDP_RCVQ * 4);
            *out = &sockets[i];
            return NX_NET_OK;
        }
    return NX_NET_FULL;
}
void nx_net_udp_close(struct nx_net_udp *u)
{
    nx_net_sockbuf_flush(&u->receive);
    *u = (struct nx_net_udp){0};
}
void nx_net_udp_set_reuse(struct nx_net_udp *u, bool reuse) { u->reuse = reuse; }

/* BSD in_pcbbind's conflict rule: same port and overlapping addresses. */
static bool conflicts(const struct nx_net_udp *self, nx_net_addr_t addr, nx_net_port_t port)
{
    for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i) {
        const struct nx_net_udp *u = &sockets[i];
        if (u == self || !u->open || u->local_port != port) continue;
        if ((!u->local_addr || !addr || u->local_addr == addr) && !(u->reuse && self->reuse)) return true;
    }
    return false;
}
/* Ephemeral ports are never shared, whatever the address. */
static bool port_used(nx_net_port_t port)
{
    for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i)
        if (sockets[i].open && sockets[i].local_port == port) return true;
    return false;
}
enum nx_net_result nx_net_udp_bind(struct nx_net_udp *u, nx_net_addr_t addr, nx_net_port_t port, bool privileged)
{
    if (u->local_port || (addr && !nx_net_inet_local(addr))) return NX_NET_INVALID;
    if (!port) {
        for (unsigned tries = 0; tries <= NX_NET_UDP_EPHEMERAL_LAST - NX_NET_UDP_EPHEMERAL_FIRST; ++tries) {
            nx_net_port_t candidate = next_ephemeral;
            next_ephemeral = candidate == NX_NET_UDP_EPHEMERAL_LAST ? NX_NET_UDP_EPHEMERAL_FIRST
                                                                    : (nx_net_port_t)(candidate + 1);
            if (!port_used(candidate)) { port = candidate; break; }
        }
        if (!port) return NX_NET_FULL;
    } else if (port < NX_NET_UDP_RESERVED && !privileged) {
        return NX_NET_DENIED;
    } else if (conflicts(u, addr, port)) {
        return NX_NET_EXISTS;
    }
    u->local_addr = addr;
    u->local_port = port;
    return NX_NET_OK;
}
enum nx_net_result nx_net_udp_connect(struct nx_net_udp *u, nx_net_addr_t addr, nx_net_port_t port)
{
    if (addr && (!nx_net_inet_unicast(addr) || !port)) return NX_NET_INVALID;
    if (addr && !u->local_port) {
        enum nx_net_result result = nx_net_udp_bind(u, 0, 0, false);
        if (result != NX_NET_OK) return result;
    }
    u->foreign_addr = addr;
    u->foreign_port = addr ? port : 0;
    u->error = NX_NET_OK;
    return NX_NET_OK;
}
enum nx_net_result nx_net_udp_take_error(struct nx_net_udp *u)
{
    enum nx_net_result error = u->error;
    u->error = NX_NET_OK;
    return error;
}

/* Checksum over the pseudoheader and the chain from offset (length bytes). */
static uint16_t checksum(const struct nx_net_mbuf *m, size_t offset, nx_net_addr_t src, nx_net_addr_t dst,
                         uint16_t length)
{
    struct nx_net_checksum sum;
    nx_net_checksum_init(&sum);
    nx_net_checksum_pseudo(&sum, src, dst, NX_NET_IPPROTO_UDP, length);
    for (; m; m = m->next) {
        if (offset >= m->len) { offset -= m->len; continue; }
        nx_net_checksum_update(&sum, m->data + offset, m->len - offset);
        offset = 0;
    }
    return nx_net_checksum_finish(&sum);
}

enum nx_net_result nx_net_udp_send(struct nx_net_udp *u, struct nx_net_mbuf *m, nx_net_addr_t addr,
                                   nx_net_port_t port)
{
    enum nx_net_result result = NX_NET_OK;
    size_t length = nx_net_mbuf_length(m);
    if (!(m->flags & NX_NET_M_PKTHDR)) result = NX_NET_INVALID;
    else if (addr && u->foreign_addr) result = NX_NET_EXISTS;
    else if (!addr && !(addr = u->foreign_addr)) result = NX_NET_INVALID;
    else if (!u->foreign_addr && !port) result = NX_NET_INVALID;
    else if (length > NX_NET_UDP_SNDMAX) result = NX_NET_TOO_BIG;
    else if (!u->local_port) result = nx_net_udp_bind(u, 0, 0, false);
    if (u->foreign_addr && addr == u->foreign_addr) port = u->foreign_port;
    nx_net_addr_t src = u->local_addr;
    struct nx_net_route route;
    if (result == NX_NET_OK && !src && (result = nx_net_route_lookup(addr, &route)) == NX_NET_OK) src = route.source;
    if (result == NX_NET_OK && !nx_net_mbuf_prepend(&m, NX_NET_UDP_HEADER)) result = NX_NET_NO_BUFFERS;
    if (result != NX_NET_OK) {
        ++stats.output_errors;
        nx_net_mbuf_drop(m);
        return result;
    }
    uint16_t total = (uint16_t)(length + NX_NET_UDP_HEADER);
    uint8_t *h = m->data;
    nx_net_put16(h, u->local_port);
    nx_net_put16(h + 2, port);
    nx_net_put16(h + 4, total);
    nx_net_put16(h + 6, 0);
    uint16_t sum = checksum(m, 0, src, addr, total);
    nx_net_put16(h + 6, sum ? sum : 0xffff);   /* zero means no checksum */
    struct nx_net_ip_send p = { .src = src, .dst = addr, .protocol = NX_NET_IPPROTO_UDP };
    result = nx_net_ip_output(m, &p);
    if (result == NX_NET_OK) ++stats.sent;
    else ++stats.output_errors;
    return result;
}

struct nx_net_mbuf *nx_net_udp_receive(struct nx_net_udp *u, nx_net_addr_t *from, nx_net_port_t *from_port)
{
    struct nx_net_mbuf *m = nx_net_sockbuf_take_record(&u->receive);
    if (!m) return NULL;
    uint8_t meta[META];
    nx_net_mbuf_copydata(m, 0, META, meta);
    nx_net_mbuf_adj_head(m, META);
    if (from) *from = nx_net_get32(meta);
    if (from_port) *from_port = nx_net_get16(meta + 4);
    return m;
}

/* Queue the payload of m (header_length + 8 bytes contiguous) for u. */
static void deliver(struct nx_net_udp *u, struct nx_net_mbuf *m, size_t hlen, nx_net_addr_t src, nx_net_port_t sport)
{
    nx_net_mbuf_adj_head(m, hlen + NX_NET_UDP_HEADER - META);
    nx_net_put32(m->data, src);
    nx_net_put16(m->data + 4, sport);
    nx_net_put16(m->data + 6, 0);
    if (!nx_net_sockbuf_append_record(&u->receive, m)) { ++stats.queue_drops; return; }
    ++stats.delivered;
    if (u->readable) u->readable(u->argument);
}
static bool broadcast(const struct nx_net_if *ifp, nx_net_addr_t dst)
{
    return dst == UINT32_MAX ||
           (ifp->inet_configured && ~ifp->inet_mask > 1 && dst == (ifp->inet_addr | ~ifp->inet_mask));
}
static bool matches(const struct nx_net_udp *u, nx_net_addr_t src, nx_net_port_t sport, nx_net_addr_t dst,
                    nx_net_port_t dport)
{
    return u->open && u->local_port == dport && (!u->local_addr || u->local_addr == dst) &&
           (!u->foreign_addr || (u->foreign_addr == src && u->foreign_port == sport));
}

void nx_net_udp_input(struct nx_net_if *ifp, struct nx_net_mbuf *m, size_t hlen)
{
    ++stats.received;
    size_t length = nx_net_mbuf_length(m);
    if (length < hlen + NX_NET_UDP_HEADER || !nx_net_mbuf_pullup(&m, hlen + NX_NET_UDP_HEADER)) {
        ++stats.too_short;
        nx_net_mbuf_drop(m);
        return;
    }
    const uint8_t *h = m->data, *udp = h + hlen;
    size_t udp_length = nx_net_get16(udp + 4);
    if (udp_length < NX_NET_UDP_HEADER || udp_length > length - hlen) {   /* truncated */
        ++stats.bad_length;
        nx_net_mbuf_drop(m);
        return;
    }
    if (udp_length < length - hlen) nx_net_mbuf_adj_tail(m, length - hlen - udp_length);
    nx_net_addr_t src = nx_net_get32(h + 12), dst = nx_net_get32(h + 16);
    nx_net_port_t sport = nx_net_get16(udp), dport = nx_net_get16(udp + 2);
    if (nx_net_get16(udp + 6) && checksum(m, hlen, src, dst, (uint16_t)udp_length)) {
        ++stats.bad_checksum;
        nx_net_mbuf_drop(m);
        return;
    }
    if (broadcast(ifp, dst)) {
        /* Every matching socket gets a copy, as in BSD; no error is sent. */
        struct nx_net_udp *last = NULL;
        for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i) {
            if (!matches(&sockets[i], src, sport, dst, dport)) continue;
            if (last) {
                struct nx_net_mbuf *copy = nx_net_mbuf_copym(m, 0, udp_length + hlen);
                if (copy && nx_net_mbuf_pullup(&copy, hlen + NX_NET_UDP_HEADER)) deliver(last, copy, hlen, src, sport);
                else { ++stats.queue_drops; if (copy) nx_net_mbuf_drop(copy); }
            }
            last = &sockets[i];
        }
        if (last) deliver(last, m, hlen, src, sport);
        else { ++stats.broadcast_drops; nx_net_mbuf_drop(m); }
        return;
    }
    /* Unicast: the match with the fewest wildcards (BSD in_pcblookup). */
    struct nx_net_udp *best = NULL;
    unsigned best_wild = 3;
    for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i) {
        struct nx_net_udp *u = &sockets[i];
        if (!matches(u, src, sport, dst, dport)) continue;
        unsigned wild = !u->local_addr + !u->foreign_addr;
        if (wild < best_wild) { best = u; best_wild = wild; }
    }
    if (best) { deliver(best, m, hlen, src, sport); return; }
    ++stats.no_port;
    nx_net_icmp_error(ifp, m, hlen, NX_NET_ICMP_UNREACH, NX_NET_ICMP_UNREACH_PORT, 0);
}

/* BSD udp_ctlinput/in_pcbnotify: tell connected sockets that sent the quoted
 * datagram. */
void nx_net_udp_notify(const struct nx_net_icmp_notice *n)
{
    const uint8_t *q = n->quoted, *udp = q + n->quoted_header;
    nx_net_addr_t src = nx_net_get32(q + 12), dst = nx_net_get32(q + 16);
    nx_net_port_t sport = nx_net_get16(udp), dport = nx_net_get16(udp + 2);
    for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i) {
        struct nx_net_udp *u = &sockets[i];
        if (!u->open || u->local_port != sport || u->foreign_addr != dst || u->foreign_port != dport ||
            (u->local_addr && u->local_addr != src)) continue;
        u->error = n->error;
        ++stats.errors_notified;
        if (u->readable) u->readable(u->argument);
    }
}
