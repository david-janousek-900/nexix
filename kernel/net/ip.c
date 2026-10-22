#include "net/ip.h"
#include "net/compat.h"
#include "net/wire.h"

#define HEADER sizeof(struct nx_net_ip_wire)
#define IP_RF 0x8000u
#define IP_DF 0x4000u
#define IP_MF 0x2000u
#define IP_OFFMASK 0x1fffu

static struct { uint8_t protocol; nx_net_ip_input_fn input; } protocols[NX_NET_IP_PROTOCOLS];
static nx_net_ip_input_fn unknown;
static struct nx_net_mbuf_pool *pool;
static uint64_t (*now_ms)(void);
static struct nx_net_ip_stats stats;
static uint16_t next_id;

/* One datagram under reassembly: payloads with their headers removed, in
 * arrival order, plus the header of the offset-zero fragment. */
struct fragment { struct nx_net_mbuf *m; size_t offset, length; };
struct reassembly {
    bool used, have_first, have_last;
    nx_net_addr_t src, dst;
    uint16_t id;
    uint8_t protocol;
    struct nx_net_if *ifp;   /* of the latest fragment */
    uint64_t deadline;
    size_t end, count, header_length;
    uint8_t header[60];
    struct fragment fragments[NX_NET_IP_QUEUE_FRAGS];
};
static struct reassembly queues[NX_NET_IP_QUEUES];
static size_t held;

enum nx_net_result nx_net_ip_register(uint8_t protocol, nx_net_ip_input_fn input)
{
    if (!input) return NX_NET_INVALID;
    for (unsigned i = 0; i < NX_NET_IP_PROTOCOLS; ++i)
        if (protocols[i].input && protocols[i].protocol == protocol) return NX_NET_EXISTS;
    for (unsigned i = 0; i < NX_NET_IP_PROTOCOLS; ++i)
        if (!protocols[i].input) { protocols[i].protocol = protocol; protocols[i].input = input; return NX_NET_OK; }
    return NX_NET_FULL;
}
void nx_net_ip_unregister(uint8_t protocol)
{
    for (unsigned i = 0; i < NX_NET_IP_PROTOCOLS; ++i)
        if (protocols[i].input && protocols[i].protocol == protocol) protocols[i].input = NULL;
}
void nx_net_ip_set_unknown_hook(nx_net_ip_input_fn hook) { unknown = hook; }

/* Drop a partial datagram and everything it holds. */
static void release(struct reassembly *q)
{
    for (size_t i = 0; i < q->count; ++i) nx_net_mbuf_drop(q->fragments[i].m);
    held -= q->count;
    *q = (struct reassembly){0};
}
void nx_net_ip_flush(void)
{
    for (unsigned i = 0; i < NX_NET_IP_QUEUES; ++i) if (queues[i].used) release(&queues[i]);
}
void nx_net_ip_if_changed(struct nx_net_if *ifp)
{
    for (unsigned i = 0; i < NX_NET_IP_QUEUES; ++i)
        if (queues[i].used && queues[i].ifp == ifp) release(&queues[i]);
}
void nx_net_ip_init(struct nx_net_mbuf_pool *packets, uint64_t (*clock)(void))
{
    nx_net_ip_flush();
    pool = packets;
    now_ms = clock;
    stats = (struct nx_net_ip_stats){0};
}
void nx_net_ip_timer(void)
{
    uint64_t now = now_ms();
    for (unsigned i = 0; i < NX_NET_IP_QUEUES; ++i)
        if (queues[i].used && now >= queues[i].deadline) {
            ++stats.frag_timeouts;
            release(&queues[i]);
        }
}
size_t nx_net_ip_reassembling(void)
{
    size_t count = 0;
    for (unsigned i = 0; i < NX_NET_IP_QUEUES; ++i) count += queues[i].used;
    return count;
}
struct nx_net_ip_stats nx_net_ip_stats(void) { return stats; }

static uint16_t header_checksum(const uint8_t *header, size_t length)
{
    struct nx_net_checksum sum;
    nx_net_checksum_init(&sum);
    nx_net_checksum_update(&sum, header, length);
    return nx_net_checksum_finish(&sum);
}
static void seal(uint8_t *header, size_t length)
{
    nx_net_put16(header + 10, 0);
    nx_net_put16(header + 10, header_checksum(header, length));
}

static struct reassembly *find_queue(nx_net_addr_t src, nx_net_addr_t dst, uint16_t id, uint8_t protocol)
{
    struct reassembly *free_slot = NULL;
    for (unsigned i = 0; i < NX_NET_IP_QUEUES; ++i) {
        struct reassembly *q = &queues[i];
        if (q->used && q->src == src && q->dst == dst && q->id == id && q->protocol == protocol) return q;
        if (!q->used && !free_slot) free_slot = q;
    }
    if (free_slot)
        *free_slot = (struct reassembly){ .used = true, .src = src, .dst = dst, .id = id, .protocol = protocol,
                                          .deadline = now_ms() + NX_NET_IP_REASS_MS };
    return free_slot;
}
static struct nx_net_mbuf *fail(struct nx_net_mbuf *m, uint64_t *counter)
{
    ++*counter;
    nx_net_mbuf_drop(m);
    return NULL;
}

/* Add one fragment (header validated, length trimmed). Returns the complete
 * datagram, its header contiguous, or NULL when the fragment was kept or
 * dropped. Any partial overlap or inconsistent end discards the datagram,
 * as RFC 5722 does for IPv6, rather than BSD's trimming. */
static struct nx_net_mbuf *reassemble(struct nx_net_if *ifp, struct nx_net_mbuf *m, size_t *header_length)
{
    const uint8_t *h = m->data;
    size_t hlen = *header_length, length = nx_net_get16(h + 2) - hlen;
    uint16_t field = nx_net_get16(h + 6);
    size_t offset = (size_t)(field & IP_OFFMASK) * 8;
    bool more = field & IP_MF;
    ++stats.fragments;
    if ((more && (length == 0 || length % 8)) || offset + length > NX_NET_IP_MAXPACKET - hlen)
        return fail(m, &stats.bad_length);
    struct reassembly *q = find_queue(nx_net_get32(h + 12), nx_net_get32(h + 16), nx_net_get16(h + 4), h[9]);
    if (!q) return fail(m, &stats.frag_dropped);
    for (size_t i = 0; i < q->count; ++i) {
        const struct fragment *f = &q->fragments[i];
        if (f->offset == offset && f->length == length) return fail(m, &stats.frag_dropped);   /* duplicate */
        if (offset < f->offset + f->length && f->offset < offset + length) {
            release(q);
            return fail(m, &stats.frag_overlaps);
        }
    }
    size_t end = offset + length;
    bool inconsistent = q->have_last && (more ? end > q->end : end != q->end);
    for (size_t i = 0; !more && i < q->count; ++i)
        inconsistent |= q->fragments[i].offset + q->fragments[i].length > end;
    if (inconsistent) {
        release(q);
        return fail(m, &stats.frag_overlaps);
    }
    if (q->count == NX_NET_IP_QUEUE_FRAGS) {
        release(q);
        return fail(m, &stats.frag_dropped);
    }
    if (held == NX_NET_IP_FRAGS) {
        if (!q->count) *q = (struct reassembly){0};
        return fail(m, &stats.frag_dropped);
    }
    if (!offset) {
        for (size_t i = 0; i < hlen; ++i) q->header[i] = h[i];
        q->header_length = hlen;
        q->have_first = true;
    }
    if (!more) { q->have_last = true; q->end = end; }
    nx_net_mbuf_adj_head(m, hlen);
    q->fragments[q->count++] = (struct fragment){ m, offset, length };
    ++held;
    q->ifp = ifp;

    size_t covered = 0;
    for (size_t i = 0; i < q->count; ++i) covered += q->fragments[i].length;
    if (!q->have_first || !q->have_last || covered != q->end) return NULL;
    /* Without overlaps, full coverage means contiguous: chain in order. */
    if (q->header_length + q->end > NX_NET_IP_MAXPACKET) {
        release(q);
        ++stats.bad_length;
        return NULL;
    }
    for (size_t i = 1; i < q->count; ++i)
        for (size_t j = i; j && q->fragments[j - 1].offset > q->fragments[j].offset; --j) {
            struct fragment t = q->fragments[j];
            q->fragments[j] = q->fragments[j - 1];
            q->fragments[j - 1] = t;
        }
    struct nx_net_mbuf *chain = q->fragments[0].m;
    for (size_t i = 1; i < q->count; ++i) nx_net_mbuf_cat(chain, q->fragments[i].m);
    held -= q->count;
    q->count = 0;
    hlen = q->header_length;
    if (!nx_net_mbuf_prepend(&chain, hlen)) {
        *q = (struct reassembly){0};
        return fail(chain, &stats.frag_dropped);
    }
    for (size_t i = 0; i < hlen; ++i) chain->data[i] = q->header[i];
    nx_net_put16(chain->data + 2, (uint16_t)(hlen + q->end));
    nx_net_put16(chain->data + 6, 0);
    seal(chain->data, hlen);
    *q = (struct reassembly){0};
    ++stats.reassembled;
    *header_length = hlen;
    return chain;
}

/* Forwarding is disabled: accept only our addresses and broadcasts. */
static bool for_us(const struct nx_net_if *ifp, nx_net_addr_t dst)
{
    if (dst == UINT32_MAX || nx_net_inet_local(dst)) return true;
    if ((ifp->flags & NX_NET_IFF_LOOPBACK) && (dst >> 24) == 127) return true;
    return ifp->inet_configured && ~ifp->inet_mask > 1 && dst == (ifp->inet_addr | ~ifp->inet_mask);
}

void nx_net_ip_input(struct nx_net_if *ifp, struct nx_net_mbuf *m)
{
    ++stats.received;
    size_t length = nx_net_mbuf_length(m);
    if (!(m->flags & NX_NET_M_PKTHDR) || length < HEADER || !nx_net_mbuf_pullup(&m, HEADER)) {
        fail(m, &stats.too_short);
        return;
    }
    size_t hlen = (size_t)(m->data[0] & 0xf) * 4;
    if (m->data[0] >> 4 != 4) { fail(m, &stats.bad_version); return; }
    if (hlen < HEADER || hlen > length || !nx_net_mbuf_pullup(&m, hlen)) { fail(m, &stats.bad_header); return; }
    const uint8_t *h = m->data;
    if (header_checksum(h, hlen)) { fail(m, &stats.bad_checksum); return; }
    size_t total = nx_net_get16(h + 2);
    if (total < hlen || total > length) { fail(m, &stats.bad_length); return; }
    if (total < length) nx_net_mbuf_adj_tail(m, length - total);   /* link padding */
    nx_net_addr_t src = nx_net_get32(h + 12), dst = nx_net_get32(h + 16);
    if ((src >> 28) >= 0xe) { fail(m, &stats.bad_source); return; }   /* multicast, reserved, broadcast */
    if (!for_us(ifp, dst)) { fail(m, &stats.not_for_us); return; }
    uint16_t field = nx_net_get16(h + 6);
    if (field & IP_RF) { fail(m, &stats.bad_header); return; }
    if (field & (IP_MF | IP_OFFMASK)) {
        if (!(m = reassemble(ifp, m, &hlen))) return;
    }
    uint8_t protocol = m->data[9];
    for (unsigned i = 0; i < NX_NET_IP_PROTOCOLS; ++i)
        if (protocols[i].input && protocols[i].protocol == protocol) {
            ++stats.delivered;
            protocols[i].input(ifp, m, hlen);
            return;
        }
    ++stats.no_protocol;
    if (unknown) unknown(ifp, m, hlen);
    else nx_net_mbuf_drop(m);
}

static bool add_header(struct nx_net_mbuf **m, const struct nx_net_ip_send *p, nx_net_addr_t src,
                       uint16_t id, uint16_t field, size_t payload)
{
    if (!nx_net_mbuf_prepend(m, HEADER)) return false;
    uint8_t *h = (*m)->data;
    h[0] = 0x45;
    h[1] = p->tos;
    nx_net_put16(h + 2, (uint16_t)(HEADER + payload));
    nx_net_put16(h + 4, id);
    nx_net_put16(h + 6, field);
    h[8] = p->ttl ? p->ttl : NX_NET_IP_DEFTTL;
    h[9] = p->protocol;
    nx_net_put32(h + 12, src);
    nx_net_put32(h + 16, p->dst);
    seal(h, HEADER);
    return true;
}

enum nx_net_result nx_net_ip_output(struct nx_net_mbuf *m, const struct nx_net_ip_send *p)
{
    struct nx_net_route route;
    enum nx_net_result result = NX_NET_OK;
    size_t length = nx_net_mbuf_length(m);
    if (!(m->flags & NX_NET_M_PKTHDR) || (p->src && !nx_net_inet_local(p->src))) result = NX_NET_INVALID;
    else if (length > NX_NET_IP_MAXPACKET - HEADER) result = NX_NET_TOO_BIG;
    else result = nx_net_route_lookup(p->dst, &route);
    if (result != NX_NET_OK) {
        ++stats.output_errors;
        nx_net_mbuf_drop(m);
        return result;
    }
    nx_net_addr_t src = p->src ? p->src : route.source;
    uint16_t id = next_id++;
    if (HEADER + length <= route.ifp->mtu) {
        if (!add_header(&m, p, src, id, p->dont_fragment ? IP_DF : 0, length)) {
            ++stats.output_errors;
            nx_net_mbuf_drop(m);
            return NX_NET_NO_BUFFERS;
        }
        ++stats.sent;
        return nx_net_route_send(&route, m);
    }
    size_t chunk = (route.ifp->mtu - HEADER) & ~(size_t)7;
    if (p->dont_fragment || !chunk) {
        ++stats.cant_fragment;
        nx_net_mbuf_drop(m);
        return NX_NET_TOO_BIG;
    }
    /* Build every fragment before sending any, so a failure sends nothing. */
    struct nx_net_mbuf *list = NULL, **link = &list;
    for (size_t offset = 0; offset < length; offset += chunk) {
        size_t part = length - offset < chunk ? length - offset : chunk;
        uint16_t field = (uint16_t)(offset / 8) | (offset + part < length ? IP_MF : 0);
        struct nx_net_mbuf *f = nx_net_mbuf_copym(m, offset, part);
        if (!f || !add_header(&f, p, src, id, field, part)) {
            if (f) nx_net_mbuf_drop(f);
            while (list) { struct nx_net_mbuf *next = list->nextpkt; list->nextpkt = NULL; nx_net_mbuf_drop(list); list = next; }
            ++stats.output_errors;
            nx_net_mbuf_drop(m);
            return NX_NET_NO_BUFFERS;
        }
        *link = f;
        link = &f->nextpkt;
    }
    nx_net_mbuf_freem(m);
    ++stats.fragmented;
    ++stats.sent;
    while (list) {
        struct nx_net_mbuf *f = list;
        list = f->nextpkt;
        f->nextpkt = NULL;
        if (result == NX_NET_OK) {
            result = nx_net_route_send(&route, f);
            stats.fragments_sent += result == NX_NET_OK;
        } else {
            nx_net_mbuf_drop(f);
        }
    }
    return result;
}
