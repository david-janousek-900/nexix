#include "net/icmp.h"
#include "net/compat.h"
#include "net/wire.h"

#define IP_HEADER sizeof(struct nx_net_ip_wire)
#define IP_OFFMASK 0x1fffu

static struct { uint8_t protocol; nx_net_icmp_notify_fn notify; } notifies[NX_NET_ICMP_NOTIFY];
static struct nx_net_icmp_stats stats;
static struct nx_net_raw raws[NX_NET_RAW_SOCKETS];

enum nx_net_result nx_net_icmp_register_notify(uint8_t protocol, nx_net_icmp_notify_fn notify)
{
    if (!notify) return NX_NET_INVALID;
    for (unsigned i = 0; i < NX_NET_ICMP_NOTIFY; ++i)
        if (notifies[i].notify && notifies[i].protocol == protocol) return NX_NET_EXISTS;
    for (unsigned i = 0; i < NX_NET_ICMP_NOTIFY; ++i)
        if (!notifies[i].notify) { notifies[i].protocol = protocol; notifies[i].notify = notify; return NX_NET_OK; }
    return NX_NET_FULL;
}
void nx_net_icmp_unregister_notify(uint8_t protocol)
{
    for (unsigned i = 0; i < NX_NET_ICMP_NOTIFY; ++i)
        if (notifies[i].notify && notifies[i].protocol == protocol) notifies[i].notify = NULL;
}
void nx_net_icmp_init(void)
{
    for (unsigned i = 0; i < NX_NET_ICMP_NOTIFY; ++i) notifies[i].notify = NULL;
    stats = (struct nx_net_icmp_stats){0};
}
struct nx_net_icmp_stats nx_net_icmp_stats(void) { return stats; }

/* Checksum of the chain from offset to its end. */
static uint16_t chain_checksum(const struct nx_net_mbuf *m, size_t offset)
{
    struct nx_net_checksum sum;
    nx_net_checksum_init(&sum);
    for (; m; m = m->next) {
        if (offset >= m->len) { offset -= m->len; continue; }
        nx_net_checksum_update(&sum, m->data + offset, m->len - offset);
        offset = 0;
    }
    return nx_net_checksum_finish(&sum);
}
/* Fill the checksum of the ICMP message at the head of m (8 bytes contiguous). */
static void seal(struct nx_net_mbuf *m)
{
    nx_net_put16(m->data + 2, 0);
    nx_net_put16(m->data + 2, chain_checksum(m, 0));
}
static void drop(struct nx_net_mbuf *m, uint64_t *counter)
{
    ++*counter;
    nx_net_mbuf_drop(m);
}

static bool broadcast(const struct nx_net_if *ifp, nx_net_addr_t dst)
{
    if (dst == UINT32_MAX || (dst >> 28) >= 0xe) return true;
    return ifp && ifp->inet_configured && ~ifp->inet_mask > 1 &&
           (dst == (ifp->inet_addr | ~ifp->inet_mask) || dst == (ifp->inet_addr & ifp->inet_mask));
}
/* Error types never provoke another error (BSD ICMP_INFOTYPE inverted). */
static bool info_type(uint8_t type)
{
    return type == NX_NET_ICMP_ECHOREPLY || type == NX_NET_ICMP_ECHO || (type >= 13 && type <= 18);
}

/* Send an ICMP message (m starts at its header) from our address local, or
 * the route's source when local is not one of ours. */
static void send(struct nx_net_mbuf *m, nx_net_addr_t local, nx_net_addr_t dst, uint8_t tos, uint64_t *counter)
{
    seal(m);
    struct nx_net_ip_send p = { .src = nx_net_inet_local(local) ? local : 0, .dst = dst,
                                .protocol = NX_NET_IPPROTO_ICMP, .tos = tos };
    if (nx_net_ip_output(m, &p) == NX_NET_OK) ++*counter;
    else ++stats.output_errors;
}

void nx_net_icmp_error(struct nx_net_if *ifp, struct nx_net_mbuf *m, size_t hlen, uint8_t type, uint8_t code,
                       uint16_t mtu)
{
    const uint8_t *h = m->data;
    size_t length = nx_net_mbuf_length(m);
    nx_net_addr_t src = nx_net_get32(h + 12), dst = nx_net_get32(h + 16);
    bool suppress = (nx_net_get16(h + 6) & IP_OFFMASK) || broadcast(ifp, dst) || !nx_net_inet_unicast(src);
    if (!suppress && h[9] == NX_NET_IPPROTO_ICMP) {
        uint8_t quoted_type;
        suppress = length < hlen + NX_NET_ICMP_MINLEN || !nx_net_mbuf_copydata(m, hlen, 1, &quoted_type) ||
                   !info_type(quoted_type);
    }
    if (suppress) { drop(m, &stats.errors_suppressed); return; }
    uint8_t message[NX_NET_ICMP_MINLEN + 60 + NX_NET_ICMP_QUOTE] = { type, code };
    if (type == NX_NET_ICMP_UNREACH && code == NX_NET_ICMP_UNREACH_NEEDFRAG) nx_net_put16(message + 6, mtu);
    size_t quote = hlen + (length - hlen < NX_NET_ICMP_QUOTE ? length - hlen : NX_NET_ICMP_QUOTE);
    struct nx_net_mbuf *e = nx_net_mbuf_get(m->pool, true);
    bool built = e && nx_net_mbuf_copydata(m, 0, quote, message + NX_NET_ICMP_MINLEN) &&
                 nx_net_mbuf_append(e, message, NX_NET_ICMP_MINLEN + quote);
    nx_net_mbuf_freem(m);
    if (!built) {
        if (e) nx_net_mbuf_freem(e);
        ++stats.output_errors;
        return;
    }
    send(e, dst, src, 0, &stats.errors_sent);
}
void nx_net_icmp_unknown(struct nx_net_if *ifp, struct nx_net_mbuf *m, size_t hlen)
{
    nx_net_icmp_error(ifp, m, hlen, NX_NET_ICMP_UNREACH, NX_NET_ICMP_UNREACH_PROTOCOL, 0);
}

/* Map an error to its result as BSD inetctlerrmap does; false when the code
 * is unknown. */
static bool classify(uint8_t type, uint8_t code, enum nx_net_result *error)
{
    switch (type) {
    case NX_NET_ICMP_UNREACH:
        switch (code) {
        case 0: case 6: case 9: case 11: *error = NX_NET_UNREACHABLE; return true;
        case 1: case 5: case 7: case 10: case 12: *error = NX_NET_HOST_UNREACHABLE; return true;
        case 2: case 3: *error = NX_NET_REFUSED; return true;
        case 4: *error = NX_NET_TOO_BIG; return true;
        default: return false;
        }
    case NX_NET_ICMP_TIMXCEED: *error = NX_NET_HOST_UNREACHABLE; return code <= 1;
    case NX_NET_ICMP_PARAMPROB: *error = NX_NET_INVALID; return code <= 2;
    default: return false;
    }
}
/* Validate the quoted datagram and tell its transport. Errors about
 * datagrams we could not have sent are dropped. */
static bool notify(struct nx_net_mbuf **m, size_t hlen, uint8_t type, uint8_t code)
{
    enum nx_net_result error;
    size_t length = nx_net_mbuf_length(*m), at = hlen + NX_NET_ICMP_MINLEN;
    if (!classify(type, code, &error) || length < at + IP_HEADER + NX_NET_ICMP_QUOTE) return false;
    uint8_t first;
    if (!nx_net_mbuf_copydata(*m, at, 1, &first) || first >> 4 != 4) return false;
    size_t quoted_header = (size_t)(first & 0xf) * 4, quoted = length - at;
    if (quoted_header < IP_HEADER || quoted < quoted_header + NX_NET_ICMP_QUOTE) return false;
    size_t need = at + quoted_header + NX_NET_ICMP_QUOTE;
    if (need > NX_NET_MLEN || !nx_net_mbuf_pullup(m, need)) return false;
    const uint8_t *q = (*m)->data + at;
    if (!nx_net_inet_local(nx_net_get32(q + 12))) return false;
    struct nx_net_icmp_notice notice = {
        .error = error, .type = type, .code = code, .reporter = nx_net_get32((*m)->data + 12),
        .mtu = error == NX_NET_TOO_BIG ? nx_net_get16((*m)->data + hlen + 6) : 0,
        .quoted = q, .quoted_length = quoted_header + NX_NET_ICMP_QUOTE, .quoted_header = quoted_header,
    };
    for (unsigned i = 0; i < NX_NET_ICMP_NOTIFY; ++i)
        if (notifies[i].notify && notifies[i].protocol == q[9]) {
            notifies[i].notify(&notice);
            ++stats.notified;
            break;
        }
    return true;
}

/* BSD rip_input: a copy for every matching socket. */
static void raw_input(struct nx_net_mbuf *m)
{
    nx_net_addr_t src = nx_net_get32(m->data + 12), dst = nx_net_get32(m->data + 16);
    size_t length = nx_net_mbuf_length(m);
    for (unsigned i = 0; i < NX_NET_RAW_SOCKETS; ++i) {
        struct nx_net_raw *r = &raws[i];
        if (!r->open || (r->local && r->local != dst) || (r->foreign && r->foreign != src)) continue;
        struct nx_net_mbuf *copy = nx_net_mbuf_copym(m, 0, length);
        if (!copy) { ++r->receive.drops; continue; }
        if (!nx_net_sockbuf_append_record(&r->receive, copy)) continue;
        ++stats.raw_delivered;
        if (r->readable) r->readable(r->argument);
    }
    nx_net_mbuf_freem(m);
}

void nx_net_icmp_input(struct nx_net_if *ifp, struct nx_net_mbuf *m, size_t hlen)
{
    ++stats.received;
    size_t length = nx_net_mbuf_length(m);
    if (length < hlen + NX_NET_ICMP_MINLEN || hlen + NX_NET_ICMP_MINLEN > NX_NET_MLEN ||
        !nx_net_mbuf_pullup(&m, hlen + NX_NET_ICMP_MINLEN)) {
        drop(m, &stats.too_short);
        return;
    }
    if (chain_checksum(m, hlen)) { drop(m, &stats.bad_checksum); return; }
    uint8_t type = m->data[hlen], code = m->data[hlen + 1];
    if (type == NX_NET_ICMP_ECHO) {
        nx_net_addr_t src = nx_net_get32(m->data + 12), dst = nx_net_get32(m->data + 16);
        uint8_t tos = m->data[1];
        if (broadcast(ifp, dst)) { drop(m, &stats.broadcast_echoes); return; }   /* no smurf amplification */
        ++stats.echoes;
        nx_net_mbuf_adj_head(m, hlen);   /* options are not reflected */
        m->data[0] = NX_NET_ICMP_ECHOREPLY;
        send(m, dst, src, tos, &stats.replies_sent);
        return;
    }
    if (type == NX_NET_ICMP_UNREACH || type == NX_NET_ICMP_TIMXCEED || type == NX_NET_ICMP_PARAMPROB) {
        ++stats.errors_received;
        if (!notify(&m, hlen, type, code)) { drop(m, &stats.bad_errors); return; }
    }
    raw_input(m);
}

enum nx_net_result nx_net_raw_open(bool privileged, void (*readable)(void *), void *argument, struct nx_net_raw **out)
{
    if (!privileged) return NX_NET_DENIED;
    for (unsigned i = 0; i < NX_NET_RAW_SOCKETS; ++i)
        if (!raws[i].open) {
            raws[i] = (struct nx_net_raw){ .open = true, .readable = readable, .argument = argument };
            nx_net_sockbuf_init(&raws[i].receive, NX_NET_RAW_RCVQ, NX_NET_RAW_RCVQ * 4);
            *out = &raws[i];
            return NX_NET_OK;
        }
    return NX_NET_FULL;
}
void nx_net_raw_close(struct nx_net_raw *r)
{
    nx_net_sockbuf_flush(&r->receive);
    *r = (struct nx_net_raw){0};
}
enum nx_net_result nx_net_raw_bind(struct nx_net_raw *r, nx_net_addr_t local)
{
    if (local && !nx_net_inet_local(local)) return NX_NET_INVALID;
    r->local = local;
    return NX_NET_OK;
}
enum nx_net_result nx_net_raw_connect(struct nx_net_raw *r, nx_net_addr_t foreign)
{
    if (foreign && !nx_net_inet_unicast(foreign)) return NX_NET_INVALID;
    r->foreign = foreign;
    return NX_NET_OK;
}
enum nx_net_result nx_net_raw_send(struct nx_net_raw *r, struct nx_net_mbuf *m, nx_net_addr_t dst)
{
    enum nx_net_result result = NX_NET_OK;
    size_t length = nx_net_mbuf_length(m);
    if (dst && r->foreign) result = NX_NET_EXISTS;
    else if (!dst && !(dst = r->foreign)) result = NX_NET_INVALID;
    else if (!(m->flags & NX_NET_M_PKTHDR) || length < NX_NET_ICMP_MINLEN) result = NX_NET_INVALID;
    else if (length > NX_NET_IP_MAXPACKET - IP_HEADER) result = NX_NET_TOO_BIG;
    else if (r->local && !nx_net_inet_local(r->local)) result = NX_NET_INVALID;   /* address removed */
    else if (!nx_net_mbuf_pullup(&m, NX_NET_ICMP_MINLEN)) result = NX_NET_NO_BUFFERS;
    if (result != NX_NET_OK) {
        if (m) nx_net_mbuf_drop(m);
        return result;
    }
    seal(m);
    struct nx_net_ip_send p = { .src = r->local, .dst = dst, .protocol = NX_NET_IPPROTO_ICMP };
    return nx_net_ip_output(m, &p);
}
struct nx_net_mbuf *nx_net_raw_receive(struct nx_net_raw *r) { return nx_net_sockbuf_take_record(&r->receive); }
size_t nx_net_raw_open_count(void)
{
    size_t count = 0;
    for (unsigned i = 0; i < NX_NET_RAW_SOCKETS; ++i) count += raws[i].open;
    return count;
}
void nx_net_raw_flush(void)
{
    for (unsigned i = 0; i < NX_NET_RAW_SOCKETS; ++i)
        if (raws[i].open) nx_net_sockbuf_flush(&raws[i].receive);
}
