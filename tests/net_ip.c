#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/arp.h"
#include "net/compat.h"
#include "net/ip.h"
#include "net/wire.h"

static size_t live_pages;
static void *page_alloc(void *c) { (void)c; ++live_pages; void *p = aligned_alloc(4096, 4096); assert(p); return p; }
static void page_free(void *c, void *p) { (void)c; --live_pages; free(p); }
static const struct nx_net_page_ops ops = { page_alloc, page_free, NULL };
static struct nx_net_budget budget;
static struct nx_net_mbuf_pool pool;
static uint64_t now;
static uint64_t clock_ms(void) { return now; }

#define IP(a, b, c, d) ((nx_net_addr_t)(a) << 24 | (nx_net_addr_t)(b) << 16 | (nx_net_addr_t)(c) << 8 | (d))
#define ME IP(10, 0, 2, 15)
#define PEER IP(10, 0, 2, 2)

/* Fake Ethernet driver: records IPv4 frames (without the link header). */
#define FRAMES 16
static uint8_t frames[FRAMES][1600];
static size_t frame_lengths[FRAMES], sent;
static bool fake_transmit(struct nx_net_if *i, struct nx_net_mbuf *m)
{
    (void)i;
    size_t length = nx_net_mbuf_length(m);
    uint8_t type[2];
    assert(nx_net_mbuf_copydata(m, 12, 2, type));
    if (nx_net_get16(type) == NX_NET_ETHERTYPE_IP) {
        assert(sent < FRAMES && length - 14 <= sizeof(frames[0]));
        assert(nx_net_mbuf_copydata(m, 14, length - 14, frames[sent]));
        frame_lengths[sent++] = length - 14;
    }
    nx_net_mbuf_freem(m);
    return true;
}
static const struct nx_net_if_ops fake = { NULL, NULL, fake_transmit };
static const uint8_t mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static const uint8_t peer_mac[6] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};
static struct nx_net_if lo, em;
static void changed(struct nx_net_if *ifp) { nx_net_arp_if_changed(ifp); nx_net_ip_if_changed(ifp); }
static void kick(void *a) { (void)a; }

/* Protocol 17 sink: keeps a copy of the last datagram. */
static unsigned delivered;
static uint8_t last[70000];
static size_t last_length, last_header;
static struct nx_net_if *last_ifp;
static void sink(struct nx_net_if *ifp, struct nx_net_mbuf *m, size_t header_length)
{
    last_length = nx_net_mbuf_length(m);
    assert(m->len >= header_length && last_length <= sizeof(last) && nx_net_mbuf_copydata(m, 0, last_length, last));
    last_header = header_length;
    last_ifp = ifp;
    nx_net_mbuf_freem(m);
    ++delivered;
}

static uint8_t pattern(size_t i) { return (uint8_t)(i * 7 + 3); }
static struct nx_net_mbuf *payload(size_t length)
{
    static uint8_t bytes[70000];
    for (size_t i = 0; i < length; ++i) bytes[i] = pattern(i);
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    assert(m && nx_net_mbuf_append(m, bytes, length));
    return m;
}
static uint16_t sum(const uint8_t *bytes, size_t length)
{
    struct nx_net_checksum c;
    nx_net_checksum_init(&c);
    nx_net_checksum_update(&c, bytes, length);
    return nx_net_checksum_finish(&c);
}
/* A raw datagram: header (with options_bytes of NOPs), then data bytes
 * pattern(data_offset + i). Callers may corrupt bytes before sealing. */
struct raw { uint8_t bytes[70000]; size_t length, hlen; };
static void build(struct raw *r, nx_net_addr_t src, nx_net_addr_t dst, uint16_t id, uint16_t field,
                  size_t options_bytes, size_t data_offset, size_t data)
{
    r->hlen = 20 + options_bytes;
    r->length = r->hlen + data;
    memset(r->bytes, 0, r->hlen);
    r->bytes[0] = (uint8_t)(0x40 | r->hlen / 4);
    nx_net_put16(r->bytes + 2, (uint16_t)r->length);
    nx_net_put16(r->bytes + 4, id);
    nx_net_put16(r->bytes + 6, field);
    r->bytes[8] = 64;
    r->bytes[9] = 17;
    nx_net_put32(r->bytes + 12, src);
    nx_net_put32(r->bytes + 16, dst);
    memset(r->bytes + 20, 1, options_bytes);
    for (size_t i = 0; i < data; ++i) r->bytes[r->hlen + i] = pattern(data_offset + i);
    nx_net_put16(r->bytes + 10, sum(r->bytes, r->hlen));
}
static void reseal(struct raw *r) { nx_net_put16(r->bytes + 10, 0); nx_net_put16(r->bytes + 10, sum(r->bytes, r->hlen)); }
static void feed(struct nx_net_if *ifp, const void *bytes, size_t length)
{
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    assert(m && nx_net_mbuf_append(m, bytes, length));
    nx_net_ip_input(ifp, m);
}
static void feed_raw(const struct raw *r) { feed(&em, r->bytes, r->length); }
static void fragment(nx_net_addr_t src, uint16_t id, size_t offset, size_t length, bool more, size_t options)
{
    static struct raw r;
    build(&r, src, ME, id, (uint16_t)(offset / 8 | (more ? 0x2000 : 0)), options, offset, length);
    feed_raw(&r);
}
static bool delivered_payload(size_t length, size_t header)
{
    if (last_header != header || last_length != header + length || nx_net_get16(last + 2) != last_length) return false;
    if (nx_net_get16(last + 6) != 0 || sum(last, header)) return false;
    for (size_t i = 0; i < length; ++i) if (last[header + i] != pattern(i)) return false;
    return true;
}

static void output(void)
{
    struct nx_net_ip_send send = { .dst = PEER, .protocol = 17 };
    sent = 0;
    assert(nx_net_ip_output(payload(100), &send) == NX_NET_OK && sent == 1 && frame_lengths[0] == 120);
    const uint8_t *h = frames[0];
    assert(h[0] == 0x45 && h[1] == 0 && nx_net_get16(h + 2) == 120 && nx_net_get16(h + 6) == 0 && h[8] == 64 && h[9] == 17);
    assert(nx_net_get32(h + 12) == ME && nx_net_get32(h + 16) == PEER && !sum(h, 20));
    for (size_t i = 0; i < 100; ++i) assert(h[20 + i] == pattern(i));
    uint16_t id = nx_net_get16(h + 4);
    send = (struct nx_net_ip_send){ .src = ME, .dst = IP(8, 8, 8, 8), .protocol = 6, .ttl = 9, .tos = 0x10, .dont_fragment = true };
    assert(nx_net_ip_output(payload(0), &send) == NX_NET_OK && sent == 2);
    h = frames[1];
    assert(nx_net_get16(h + 4) == (uint16_t)(id + 1) && nx_net_get16(h + 6) == 0x4000 && h[8] == 9 && h[1] == 0x10 && h[9] == 6 && !sum(h, 20));

    /* Refusals free the payload. */
    send = (struct nx_net_ip_send){ .src = IP(10, 0, 2, 99), .dst = PEER, .protocol = 17 };
    assert(nx_net_ip_output(payload(10), &send) == NX_NET_INVALID);
    send.src = 0;
    assert(nx_net_ip_output(nx_net_mbuf_get(&pool, false), &send) == NX_NET_INVALID);
    assert(nx_net_ip_output(payload(65516), &send) == NX_NET_TOO_BIG);
    send.dst = IP(172, 16, 0, 1);
    nx_net_route_clear_default();
    assert(nx_net_ip_output(payload(10), &send) == NX_NET_UNREACHABLE);
    assert(nx_net_route_set_default(PEER) == NX_NET_OK);
    send.dst = IP(224, 0, 0, 1);
    assert(nx_net_ip_output(payload(10), &send) == NX_NET_INVALID && nx_net_ip_stats().output_errors == 5);

    /* Fragmentation at the 1500-byte MTU: 1480-byte pieces, MF on all but last. */
    sent = 0;
    send = (struct nx_net_ip_send){ .dst = PEER, .protocol = 17 };
    assert(nx_net_ip_output(payload(4000), &send) == NX_NET_OK && sent == 3);
    static const size_t sizes[] = {1480, 1480, 1040};
    for (unsigned i = 0; i < 3; ++i) {
        h = frames[i];
        assert(frame_lengths[i] == 20 + sizes[i] && nx_net_get16(h + 2) == 20 + sizes[i] && !sum(h, 20));
        assert(nx_net_get16(h + 4) == nx_net_get16(frames[0] + 4));
        assert(nx_net_get16(h + 6) == (uint16_t)(i * 185 | (i < 2 ? 0x2000 : 0)));
        for (size_t j = 0; j < sizes[i]; ++j) assert(h[20 + j] == pattern(i * 1480 + j));
    }
    /* Exactly the MTU is not fragmented; don't-fragment above it is refused. */
    assert(nx_net_ip_output(payload(1480), &send) == NX_NET_OK && sent == 4 && frame_lengths[3] == 1500);
    send.dont_fragment = true;
    assert(nx_net_ip_output(payload(1481), &send) == NX_NET_TOO_BIG && sent == 4 && nx_net_ip_stats().cant_fragment == 1);

    /* Allocation failure while fragmenting sends nothing and leaks nothing. */
    send.dont_fragment = false;
    struct nx_net_mbuf *big = payload(4000), *all = NULL;
    size_t spare = 0;
    for (struct nx_net_mbuf *m; (m = nx_net_mbuf_get(&pool, false)); ++spare) { m->next = all; all = m; }
    /* Leave room for exactly one fragment's copy and header. */
    struct nx_net_mbuf *keep = all;
    all = all->next->next;
    keep->next->next = NULL;
    nx_net_mbuf_freem(keep);
    assert(nx_net_ip_output(big, &send) == NX_NET_NO_BUFFERS && sent == 4);
    nx_net_mbuf_freem(all);
    assert(spare > 2 && pool.stats.mbufs == 0);
}

static void input(void)
{
    static struct raw r;
    struct nx_net_ip_stats before = nx_net_ip_stats();
    delivered = 0;
    build(&r, PEER, ME, 1, 0, 0, 0, 30);
    feed_raw(&r);
    assert(delivered == 1 && delivered_payload(30, 20) && last_ifp == &em);
    /* Link padding beyond the total length is trimmed. */
    memset(r.bytes + r.length, 0xee, 16);
    feed(&em, r.bytes, r.length + 16);
    assert(delivered == 2 && delivered_payload(30, 20));
    /* Options are passed up with the header. */
    build(&r, PEER, ME, 1, 0, 8, 0, 30);
    feed_raw(&r);
    assert(delivered == 3 && delivered_payload(30, 28));
    /* Destinations: subnet and limited broadcast are local; others are not forwarded. */
    build(&r, PEER, IP(10, 0, 2, 255), 1, 0, 0, 0, 5); feed_raw(&r);
    build(&r, PEER, UINT32_MAX, 1, 0, 0, 0, 5); feed_raw(&r);
    build(&r, PEER, IP(127, 0, 0, 1), 1, 0, 0, 0, 5); feed_raw(&r);    /* own loopback address */
    assert(delivered == 6);
    build(&r, PEER, IP(10, 0, 2, 99), 1, 0, 0, 0, 5); feed_raw(&r);
    build(&r, PEER, IP(8, 8, 8, 8), 1, 0, 0, 0, 5); feed_raw(&r);
    build(&r, PEER, IP(127, 0, 0, 9), 1, 0, 0, 0, 5); feed_raw(&r);    /* 127/8 only arrives on loopback */
    assert(delivered == 6 && nx_net_ip_stats().not_for_us == before.not_for_us + 3);
    feed(&lo, r.bytes, r.length);
    assert(delivered == 7 && last_ifp == &lo);

    /* Malformed headers. */
    feed(&em, r.bytes, 19);
    build(&r, PEER, ME, 1, 0, 0, 0, 5); r.bytes[0] = 0x65; reseal(&r); feed_raw(&r);
    build(&r, PEER, ME, 1, 0, 0, 0, 5); r.bytes[0] = 0x44; feed_raw(&r);           /* ihl 16 bytes */
    build(&r, PEER, ME, 1, 0, 0, 0, 5); r.bytes[0] = 0x4f; feed_raw(&r);           /* 60 > 25 bytes */
    build(&r, PEER, ME, 1, 0, 0, 0, 5); r.bytes[8] ^= 1; feed_raw(&r);
    build(&r, PEER, ME, 1, 0, 0, 0, 5); nx_net_put16(r.bytes + 2, 19); reseal(&r); feed_raw(&r);
    build(&r, PEER, ME, 1, 0, 0, 0, 5); nx_net_put16(r.bytes + 2, 26); reseal(&r); feed_raw(&r);
    build(&r, IP(224, 0, 0, 1), ME, 1, 0, 0, 0, 5); feed_raw(&r);
    build(&r, UINT32_MAX, ME, 1, 0, 0, 0, 5); feed_raw(&r);
    build(&r, PEER, ME, 1, 0x8000, 0, 0, 5); feed_raw(&r);                          /* reserved flag */
    build(&r, PEER, ME, 1, 0, 0, 0, 5); r.bytes[9] = 99; reseal(&r); feed_raw(&r);
    struct nx_net_mbuf *bare = nx_net_mbuf_get(&pool, false);
    assert(nx_net_mbuf_append(bare, r.bytes, r.length));
    nx_net_ip_input(&em, bare);
    struct nx_net_ip_stats s = nx_net_ip_stats();
    assert(delivered == 7);
    assert(s.too_short == before.too_short + 2 && s.bad_version == before.bad_version + 1);
    assert(s.bad_header == before.bad_header + 3 && s.bad_checksum == before.bad_checksum + 1);
    assert(s.bad_length == before.bad_length + 2 && s.bad_source == before.bad_source + 2);
    assert(s.no_protocol == before.no_protocol + 1 && s.delivered == before.delivered + 7);
    /* A header split across buffers is pulled up. */
    build(&r, PEER, ME, 1, 0, 8, 0, 30);
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true), *rest = nx_net_mbuf_get(&pool, false);
    assert(nx_net_mbuf_append(m, r.bytes, 7) && nx_net_mbuf_append(rest, r.bytes + 7, r.length - 7));
    nx_net_mbuf_cat(m, rest);
    nx_net_ip_input(&em, m);
    assert(delivered == 8 && delivered_payload(30, 28));
}

static void reassembly(void)
{
    struct nx_net_ip_stats before = nx_net_ip_stats();
    delivered = 0;
    /* In order; header options come from the first fragment. */
    fragment(PEER, 10, 0, 1000, true, 4);
    fragment(PEER, 10, 1000, 1000, true, 0);
    assert(!delivered && nx_net_ip_reassembling() == 1);
    fragment(PEER, 10, 2000, 3, false, 0);
    assert(delivered == 1 && delivered_payload(2003, 24) && nx_net_ip_reassembling() == 0);
    /* Reverse order, a duplicate, and a zero-length final fragment. */
    fragment(PEER, 11, 16, 0, false, 0);
    fragment(PEER, 11, 8, 8, true, 0);
    fragment(PEER, 11, 8, 8, true, 0);
    fragment(PEER, 11, 0, 8, true, 0);
    assert(delivered == 2 && delivered_payload(16, 20));
    /* Different source, protocol or id: separate datagrams. */
    fragment(PEER, 12, 0, 8, true, 0);
    fragment(IP(10, 0, 2, 3), 12, 8, 8, false, 0);
    assert(delivered == 2 && nx_net_ip_reassembling() == 2);
    fragment(PEER, 12, 8, 8, false, 0);
    fragment(IP(10, 0, 2, 3), 12, 0, 8, true, 0);
    assert(delivered == 4 && nx_net_ip_reassembling() == 0);

    /* Partial overlap discards the whole datagram, either direction. */
    fragment(PEER, 13, 0, 16, true, 0);
    fragment(PEER, 13, 8, 16, true, 0);
    assert(nx_net_ip_reassembling() == 0);
    fragment(PEER, 13, 16, 8, false, 0);
    assert(delivered == 4 && nx_net_ip_reassembling() == 1);   /* a fresh, incomplete datagram */
    fragment(PEER, 13, 0, 24, true, 0);
    assert(nx_net_ip_reassembling() == 0);
    /* Inconsistent ends: a second last fragment elsewhere, or data past the end. */
    fragment(PEER, 14, 16, 8, false, 0);
    fragment(PEER, 14, 32, 8, false, 0);
    assert(nx_net_ip_reassembling() == 0);
    fragment(PEER, 14, 16, 8, false, 0);
    fragment(PEER, 14, 24, 8, true, 0);
    assert(nx_net_ip_reassembling() == 0);
    fragment(PEER, 14, 32, 8, true, 0);
    fragment(PEER, 14, 8, 8, false, 0);
    assert(nx_net_ip_reassembling() == 0);
    struct nx_net_ip_stats s = nx_net_ip_stats();
    assert(s.frag_overlaps == before.frag_overlaps + 5 && delivered == 4);
    /* Bad lengths: non-final pieces must be nonzero multiples of 8, and
     * nothing may end past 65535. */
    fragment(PEER, 15, 0, 12, true, 0);
    fragment(PEER, 15, 0, 0, true, 0);
    fragment(PEER, 15, 65512, 8, false, 0);
    assert(nx_net_ip_stats().bad_length == s.bad_length + 3 && nx_net_ip_reassembling() == 0);
    /* The largest datagram reassembles exactly. */
    for (size_t offset = 0; offset < 65515; offset += 4096)
        fragment(PEER, 16, offset, 65515 - offset < 4096 ? 65515 - offset : 4096, offset + 4096 < 65515, 0);
    assert(delivered == 5 && delivered_payload(65515, 20));

    /* Bounds: 16 fragments per datagram, then it is discarded. */
    s = nx_net_ip_stats();
    for (size_t i = 0; i < NX_NET_IP_QUEUE_FRAGS; ++i) fragment(PEER, 20, i * 8, 8, true, 0);
    assert(nx_net_ip_reassembling() == 1);
    fragment(PEER, 20, NX_NET_IP_QUEUE_FRAGS * 8, 8, false, 0);
    assert(nx_net_ip_reassembling() == 0 && nx_net_ip_stats().frag_dropped == s.frag_dropped + 1);
    /* 8 datagrams at once; a ninth is refused. */
    for (uint16_t id = 30; id < 30 + NX_NET_IP_QUEUES; ++id) fragment(PEER, id, 0, 8, true, 0);
    fragment(PEER, 99, 0, 8, true, 0);
    assert(nx_net_ip_reassembling() == NX_NET_IP_QUEUES && nx_net_ip_stats().frag_dropped == s.frag_dropped + 2);
    nx_net_ip_flush();
    /* 64 fragments in total: four full datagrams, then nothing more is held. */
    for (uint16_t id = 40; id < 44; ++id)
        for (size_t i = 0; i < NX_NET_IP_QUEUE_FRAGS; ++i) fragment(PEER, id, i * 8, 8, true, 0);
    fragment(PEER, 50, 0, 8, true, 0);
    assert(nx_net_ip_reassembling() == 4 && nx_net_ip_stats().frag_dropped == s.frag_dropped + 3);
    assert(pool.stats.mbufs >= NX_NET_IP_FRAGS);

    /* Expiry after 30 seconds frees everything. */
    now += NX_NET_IP_REASS_MS - 1;
    nx_net_ip_timer();
    assert(nx_net_ip_reassembling() == 4);
    now += 1;
    nx_net_ip_timer();
    assert(nx_net_ip_reassembling() == 0 && nx_net_ip_stats().frag_timeouts == 4 && pool.stats.mbufs == 0);
    /* An interface going down discards what it fed. */
    fragment(PEER, 60, 0, 8, true, 0);
    nx_net_if_down(&em);
    assert(nx_net_ip_reassembling() == 0 && pool.stats.mbufs == 0);
    assert(nx_net_if_up(&em) == NX_NET_OK);
}

static void loopback(void)
{
    /* Above the 16384-byte loopback MTU: fragmented, queued, reassembled. */
    struct nx_net_ip_send send = { .dst = IP(127, 0, 0, 1), .protocol = 17 };
    delivered = 0;
    assert(nx_net_ip_output(payload(40000), &send) == NX_NET_OK && lo.queue_length == 3 && !delivered);
    assert(nx_net_loop_service(&lo, 10) == 0 && delivered == 1 && delivered_payload(40000, 20) && last_ifp == &lo);
    send.dst = ME;   /* our Ethernet address also loops back */
    assert(nx_net_ip_output(payload(10), &send) == NX_NET_OK && nx_net_loop_service(&lo, 10) == 0 && delivered == 2);
    assert(nx_net_get32(last + 12) == ME);
}

int main(void)
{
    nx_net_budget_init(&budget, 512);
    assert(nx_net_mbuf_pool_init(&pool, &ops, &budget, (struct nx_net_mbuf_limits){ 1000, 200 }));
    nx_net_arp_init(&pool, clock_ms);
    nx_net_ip_init(&pool, clock_ms);
    nx_net_if_set_change_hook(changed);
    assert(nx_net_proto_register(NX_NET_ETHERTYPE_IP, nx_net_ip_input) == NX_NET_OK);
    assert(nx_net_ip_register(17, NULL) == NX_NET_INVALID && nx_net_ip_register(17, sink) == NX_NET_OK);
    assert(nx_net_ip_register(17, sink) == NX_NET_EXISTS && nx_net_ip_register(1, sink) == NX_NET_OK);
    assert(nx_net_ip_register(2, sink) == NX_NET_OK && nx_net_ip_register(6, sink) == NX_NET_OK);
    assert(nx_net_ip_register(7, sink) == NX_NET_FULL);
    nx_net_ip_unregister(1); nx_net_ip_unregister(2); nx_net_ip_unregister(6);
    nx_net_loop_init(&lo, "lo0", kick, NULL);
    nx_net_ether_init(&em, "em0", mac, &fake, NULL);
    assert(nx_net_if_attach(&lo) == NX_NET_OK && nx_net_if_attach(&em) == NX_NET_OK);
    assert(nx_net_if_up(&lo) == NX_NET_OK && nx_net_if_up(&em) == NX_NET_OK);
    assert(nx_net_inet_set(&lo, IP(127, 0, 0, 1), IP(255, 0, 0, 0)) == NX_NET_OK);
    assert(nx_net_inet_set(&em, ME, IP(255, 255, 255, 0)) == NX_NET_OK);
    assert(nx_net_route_set_default(PEER) == NX_NET_OK);
    /* Resolve the gateway so output is sent at once. */
    uint8_t arp[28] = {0, 1, 8, 0, 6, 4, 0, 2};
    memcpy(arp + 8, peer_mac, 6); nx_net_put32(arp + 14, PEER); memcpy(arp + 18, mac, 6); nx_net_put32(arp + 24, ME);
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    assert(m && nx_net_mbuf_append(m, arp, sizeof(arp)));
    nx_net_arp_input(&em, m);

    output();
    input();
    reassembly();
    loopback();
    nx_net_if_detach(&em);
    nx_net_if_detach(&lo);
    nx_net_route_clear_default();
    nx_net_arp_flush(NULL);
    nx_net_ip_flush();
    assert(pool.stats.mbufs == 0 && pool.stats.clusters == 0 && nx_net_mbuf_pool_destroy(&pool) && !live_pages);
    puts("PASS: IPv4 header/checksum/length validation, local-only delivery, fragmentation, bounded reassembly, overlaps, expiry, allocation failure and mbuf ownership");
    return 0;
}
