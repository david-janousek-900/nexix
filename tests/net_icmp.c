#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/arp.h"
#include "net/compat.h"
#include "net/icmp.h"
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
#define LOCALHOST IP(127, 0, 0, 1)

/* Fake Ethernet driver: records IPv4 packets (without the link header). */
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

static uint16_t sum(const uint8_t *bytes, size_t length)
{
    struct nx_net_checksum c;
    nx_net_checksum_init(&c);
    nx_net_checksum_update(&c, bytes, length);
    return nx_net_checksum_finish(&c);
}
static uint8_t pattern(size_t i) { return (uint8_t)(i * 7 + 3); }

/* A datagram: header with options_bytes of NOPs, then the body. */
struct raw { uint8_t bytes[2000]; size_t length, hlen; };
static void build(struct raw *r, nx_net_addr_t src, nx_net_addr_t dst, uint8_t protocol, size_t options_bytes,
                  const uint8_t *body, size_t length)
{
    r->hlen = 20 + options_bytes;
    r->length = r->hlen + length;
    memset(r->bytes, 0, r->hlen);
    r->bytes[0] = (uint8_t)(0x40 | r->hlen / 4);
    r->bytes[1] = 0x10;
    nx_net_put16(r->bytes + 2, (uint16_t)r->length);
    nx_net_put16(r->bytes + 4, 77);
    r->bytes[8] = 64;
    r->bytes[9] = protocol;
    nx_net_put32(r->bytes + 12, src);
    nx_net_put32(r->bytes + 16, dst);
    memset(r->bytes + 20, 1, options_bytes);
    memcpy(r->bytes + r->hlen, body, length);
    nx_net_put16(r->bytes + 10, sum(r->bytes, r->hlen));
}
/* An ICMP message with a valid checksum: type, code, rest, then data. */
static size_t icmp(uint8_t *out, uint8_t type, uint8_t code, uint32_t rest, const uint8_t *data, size_t length)
{
    out[0] = type; out[1] = code;
    nx_net_put16(out + 2, 0);
    nx_net_put32(out + 4, rest);
    memcpy(out + 8, data, length);
    nx_net_put16(out + 2, sum(out, 8 + length));
    return 8 + length;
}
static struct nx_net_mbuf *chain(const void *bytes, size_t length)
{
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    assert(m && nx_net_mbuf_append(m, bytes, length));
    return m;
}
static void feed(struct nx_net_if *ifp, const struct raw *r) { nx_net_ip_input(ifp, chain(r->bytes, r->length)); }

/* An error message quoting an IPv4 datagram we sent (from ME) with protocol. */
static void error_message(struct raw *r, uint8_t type, uint8_t code, uint32_t rest, uint8_t protocol,
                          nx_net_addr_t quoted_src, size_t quoted_bytes)
{
    static uint8_t body[200], quoted[100];
    static struct raw inner;
    uint8_t udp[8] = {0x12, 0x34, 0x00, 0x35, 0x00, 0x10, 0, 0};
    build(&inner, quoted_src, PEER, protocol, 0, udp, 8);
    memcpy(quoted, inner.bytes, inner.length);
    build(r, PEER, ME, NX_NET_IPPROTO_ICMP, 0, body, icmp(body, type, code, rest, quoted, quoted_bytes));
}

static unsigned notices;
static struct nx_net_icmp_notice notice;
static void notified(const struct nx_net_icmp_notice *n)
{
    notice = *n;
    assert(n->quoted_length == n->quoted_header + 8 && n->quoted[0] >> 4 == 4);
    ++notices;
}
static unsigned readable;
static void wake(void *a) { ++*(unsigned *)a; }

/* Check the checksums of a received datagram copied to bytes. */
static size_t take(struct nx_net_raw *r, uint8_t *bytes)
{
    struct nx_net_mbuf *m = nx_net_raw_receive(r);
    if (!m) return 0;
    size_t length = nx_net_mbuf_length(m);
    assert(length <= 2000 && nx_net_mbuf_copydata(m, 0, length, bytes));
    nx_net_mbuf_freem(m);
    size_t hlen = (size_t)(bytes[0] & 0xf) * 4;
    assert(!sum(bytes, hlen) && !sum(bytes + hlen, length - hlen));
    return length;
}

static void echo(void)
{
    static struct raw r;
    uint8_t body[100], data[56];
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = pattern(i);
    struct nx_net_icmp_stats before = nx_net_icmp_stats();
    /* A request with options is answered from our address, without them. */
    sent = 0;
    build(&r, PEER, ME, NX_NET_IPPROTO_ICMP, 4, body, icmp(body, NX_NET_ICMP_ECHO, 0, 0x12340001, data, 56));
    feed(&em, &r);
    assert(sent == 1 && frame_lengths[0] == 20 + 64);
    const uint8_t *h = frames[0];
    assert(h[0] == 0x45 && h[1] == 0x10 && h[9] == 1 && !sum(h, 20));
    assert(nx_net_get32(h + 12) == ME && nx_net_get32(h + 16) == PEER);
    assert(h[20] == NX_NET_ICMP_ECHOREPLY && h[21] == 0 && nx_net_get32(h + 24) == 0x12340001 && !sum(h + 20, 64));
    assert(!memcmp(h + 28, data, 56));
    /* An odd length and a request split across buffers. */
    build(&r, PEER, ME, NX_NET_IPPROTO_ICMP, 0, body, icmp(body, NX_NET_ICMP_ECHO, 0, 7, data, 13));
    struct nx_net_mbuf *m = chain(r.bytes, 24), *rest = nx_net_mbuf_get(&pool, false);
    assert(nx_net_mbuf_append(rest, r.bytes + 24, r.length - 24));
    nx_net_mbuf_cat(m, rest);
    nx_net_ip_input(&em, m);
    assert(sent == 2 && frame_lengths[1] == 20 + 21 && !sum(frames[1] + 20, 21) && !memcmp(frames[1] + 28, data, 13));

    /* Malformed: short, bad checksum; broadcast requests are ignored. */
    build(&r, PEER, ME, NX_NET_IPPROTO_ICMP, 0, body, 7);
    feed(&em, &r);
    build(&r, PEER, ME, NX_NET_IPPROTO_ICMP, 0, body, icmp(body, NX_NET_ICMP_ECHO, 0, 0, data, 8));
    r.bytes[30] ^= 0x40;
    feed(&em, &r);
    build(&r, PEER, IP(10, 0, 2, 255), NX_NET_IPPROTO_ICMP, 0, body, icmp(body, NX_NET_ICMP_ECHO, 0, 0, data, 8));
    feed(&em, &r);
    build(&r, PEER, UINT32_MAX, NX_NET_IPPROTO_ICMP, 0, body, icmp(body, NX_NET_ICMP_ECHO, 0, 0, data, 8));
    feed(&em, &r);
    struct nx_net_icmp_stats s = nx_net_icmp_stats();
    assert(sent == 2 && s.too_short == before.too_short + 1 && s.bad_checksum == before.bad_checksum + 1);
    assert(s.broadcast_echoes == before.broadcast_echoes + 2 && s.echoes == before.echoes + 2);
    assert(s.replies_sent == before.replies_sent + 2 && s.received == before.received + 6);
    assert(pool.stats.mbufs == 0);
}

static void errors_out(void)
{
    static struct raw r;
    uint8_t data[40];
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = pattern(i);
    struct nx_net_icmp_stats before = nx_net_icmp_stats();
    /* An unregistered protocol: protocol unreachable quoting header + 8 bytes. */
    sent = 0;
    build(&r, PEER, ME, 99, 8, data, 40);
    feed(&em, &r);
    assert(sent == 1 && frame_lengths[0] == 20 + 8 + 28 + 8);
    const uint8_t *h = frames[0];
    assert(nx_net_get32(h + 12) == ME && nx_net_get32(h + 16) == PEER && !sum(h, 20) && !sum(h + 20, 44));
    assert(h[20] == NX_NET_ICMP_UNREACH && h[21] == NX_NET_ICMP_UNREACH_PROTOCOL && nx_net_get32(h + 24) == 0);
    assert(!memcmp(h + 28, r.bytes, 28 + 8));
    /* Shorter than 8 payload bytes: quote what there is. */
    build(&r, PEER, ME, 99, 0, data, 3);
    feed(&em, &r);
    assert(sent == 2 && frame_lengths[1] == 20 + 8 + 23 && !memcmp(frames[1] + 28, r.bytes, 23));
    /* Needfrag carries the next-hop MTU. */
    build(&r, PEER, ME, 17, 0, data, 40);
    nx_net_icmp_error(&em, chain(r.bytes, r.length), 20, NX_NET_ICMP_UNREACH, NX_NET_ICMP_UNREACH_NEEDFRAG, 1280);
    assert(sent == 3 && frames[2][21] == 4 && nx_net_get16(frames[2] + 26) == 1280 && !sum(frames[2] + 20, 36));
    /* Errors about ICMP queries are sent; about ICMP errors they are not. */
    uint8_t body[60];
    build(&r, PEER, ME, NX_NET_IPPROTO_ICMP, 0, body, icmp(body, NX_NET_ICMP_ECHO, 0, 0, data, 8));
    nx_net_icmp_error(&em, chain(r.bytes, r.length), 20, NX_NET_ICMP_UNREACH, 3, 0);
    assert(sent == 4);

    /* Suppressed: about an error, a non-first fragment, a broadcast, a
     * non-unicast source, or ICMP too short to tell. */
    build(&r, PEER, ME, NX_NET_IPPROTO_ICMP, 0, body, icmp(body, NX_NET_ICMP_UNREACH, 3, 0, data, 28));
    nx_net_icmp_error(&em, chain(r.bytes, r.length), 20, NX_NET_ICMP_UNREACH, 3, 0);
    build(&r, PEER, ME, 17, 0, data, 16);
    nx_net_put16(r.bytes + 6, 0x2001);
    nx_net_icmp_error(&em, chain(r.bytes, r.length), 20, NX_NET_ICMP_UNREACH, 3, 0);
    build(&r, PEER, IP(10, 0, 2, 255), 99, 0, data, 16); feed(&em, &r);
    build(&r, PEER, UINT32_MAX, 99, 0, data, 16); feed(&em, &r);
    build(&r, IP(0, 0, 0, 0), ME, 99, 0, data, 16); feed(&em, &r);
    build(&r, PEER, ME, NX_NET_IPPROTO_ICMP, 0, data, 4);
    nx_net_icmp_error(&em, chain(r.bytes, r.length), 20, NX_NET_ICMP_UNREACH, 3, 0);
    struct nx_net_icmp_stats s = nx_net_icmp_stats();
    assert(sent == 4 && s.errors_suppressed == before.errors_suppressed + 6 && s.errors_sent == before.errors_sent + 4);

    /* Allocation failure sends nothing and leaks nothing. */
    struct nx_net_mbuf *all = NULL;
    build(&r, PEER, ME, 99, 0, data, 16);
    struct nx_net_mbuf *victim = chain(r.bytes, r.length);
    for (struct nx_net_mbuf *m; (m = nx_net_mbuf_get(&pool, false));) { m->next = all; all = m; }
    nx_net_ip_input(&em, victim);
    nx_net_mbuf_freem(all);
    assert(sent == 4 && nx_net_icmp_stats().output_errors == s.output_errors + 1 && pool.stats.mbufs == 0);
}

static void errors_in(struct nx_net_raw *raw)
{
    static struct raw r;
    static uint8_t got[2000];
    struct nx_net_icmp_stats before = nx_net_icmp_stats();
    notices = 0;
    error_message(&r, NX_NET_ICMP_UNREACH, NX_NET_ICMP_UNREACH_PORT, 0, 17, ME, 28);
    feed(&em, &r);
    assert(notices == 1 && notice.error == NX_NET_REFUSED && notice.type == 3 && notice.code == 3);
    assert(notice.reporter == PEER && notice.mtu == 0 && notice.quoted_header == 20);
    assert(nx_net_get32(notice.quoted + 12) == ME && nx_net_get16(notice.quoted + 22) == 0x35);
    assert(take(raw, got) == r.length && !memcmp(got, r.bytes, r.length));   /* raw sockets see errors too */
    static const struct { uint8_t type, code; enum nx_net_result error; } map[] = {
        {3, 0, NX_NET_UNREACHABLE}, {3, 1, NX_NET_HOST_UNREACHABLE}, {3, 2, NX_NET_REFUSED},
        {3, 4, NX_NET_TOO_BIG}, {3, 13 - 1, NX_NET_HOST_UNREACHABLE}, {11, 0, NX_NET_HOST_UNREACHABLE},
        {11, 1, NX_NET_HOST_UNREACHABLE}, {12, 0, NX_NET_INVALID},
    };
    for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); ++i) {
        error_message(&r, map[i].type, map[i].code, map[i].code == 4 ? 1400 : 0, 17, ME, 28);
        feed(&em, &r);
        assert(notices == 2 + i && notice.error == map[i].error && notice.mtu == (map[i].code == 4 && map[i].type == 3 ? 1400 : 0));
    }
    while (take(raw, got)) {}
    /* Other transports are not told; malformed errors are dropped unseen. */
    unsigned seen = notices;
    error_message(&r, 3, 3, 0, 6, ME, 28); feed(&em, &r);          /* no TCP listener: raw only */
    assert(notices == seen && take(raw, got));
    error_message(&r, 3, 3, 0, 17, ME, 27); feed(&em, &r);         /* quote too short */
    error_message(&r, 3, 3, 0, 17, PEER, 28); feed(&em, &r);       /* not our datagram */
    error_message(&r, 3, 16, 0, 17, ME, 28); feed(&em, &r);        /* unknown code */
    error_message(&r, 11, 2, 0, 17, ME, 28); feed(&em, &r);
    error_message(&r, 3, 3, 0, 17, ME, 28);
    r.bytes[28] = 0x65; nx_net_put16(r.bytes + 22, 0); nx_net_put16(r.bytes + 22, sum(r.bytes + 20, r.length - 20));
    feed(&em, &r);                                                  /* quoted version 6 */
    error_message(&r, 3, 3, 0, 17, ME, 28);
    r.bytes[28] = 0x46; nx_net_put16(r.bytes + 22, 0); nx_net_put16(r.bytes + 22, sum(r.bytes + 20, r.length - 20));
    feed(&em, &r);                                                  /* quoted options missing */
    struct nx_net_icmp_stats s = nx_net_icmp_stats();
    assert(notices == seen && !take(raw, got) && s.bad_errors == before.bad_errors + 6);
    assert(s.notified == before.notified + 9 && s.errors_received == before.errors_received + 16);
    /* Redirects and source quench change nothing but reach raw sockets. */
    error_message(&r, NX_NET_ICMP_REDIRECT, 1, PEER, 17, ME, 28); feed(&em, &r);
    error_message(&r, NX_NET_ICMP_SOURCEQUENCH, 0, 0, 17, ME, 28); feed(&em, &r);
    assert(notices == seen && take(raw, got) && take(raw, got) && !take(raw, got));
    assert(pool.stats.mbufs == 0);
}

static void sockets(void)
{
    static uint8_t got[2000];
    uint8_t message[64], data[48];
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = pattern(i);
    struct nx_net_raw *r = NULL, *all[NX_NET_RAW_SOCKETS];
    /* Unprivileged callers are denied; at most 8 sockets. */
    assert(nx_net_raw_open(false, NULL, NULL, &r) == NX_NET_DENIED && !r && nx_net_raw_open_count() == 0);
    for (unsigned i = 0; i < NX_NET_RAW_SOCKETS; ++i) assert(nx_net_raw_open(true, NULL, NULL, &all[i]) == NX_NET_OK);
    assert(nx_net_raw_open(true, NULL, NULL, &r) == NX_NET_FULL && !r);
    for (unsigned i = 0; i < NX_NET_RAW_SOCKETS; ++i) nx_net_raw_close(all[i]);
    assert(nx_net_raw_open_count() == 0);

    struct nx_net_raw *a, *b;
    readable = 0;
    assert(nx_net_raw_open(true, wake, &readable, &a) == NX_NET_OK && nx_net_raw_open(true, NULL, NULL, &b) == NX_NET_OK);
    assert(nx_net_raw_bind(a, IP(10, 0, 2, 99)) == NX_NET_INVALID && nx_net_raw_bind(a, LOCALHOST) == NX_NET_OK);
    assert(nx_net_raw_connect(b, IP(224, 0, 0, 1)) == NX_NET_INVALID && nx_net_raw_connect(b, 0) == NX_NET_OK);

    /* Echo over lo0: the kernel answers; both sockets see the reply, the
     * request is not queued. The kernel fills the checksum. */
    size_t length = icmp(message, NX_NET_ICMP_ECHO, 0, 0xbeef0001, data, sizeof(data));
    nx_net_put16(message + 2, 0xdead);
    assert(nx_net_raw_send(a, chain(message, length), LOCALHOST) == NX_NET_OK);
    assert(nx_net_loop_service(&lo, 10) == 0 && nx_net_loop_service(&lo, 10) == 0);
    assert(readable == 1 && take(a, got) == 20 + length && !take(a, got));
    assert(got[20] == NX_NET_ICMP_ECHOREPLY && nx_net_get32(got + 24) == 0xbeef0001 && !memcmp(got + 28, data, 48));
    assert(nx_net_get32(got + 12) == LOCALHOST && nx_net_get32(got + 16) == LOCALHOST);
    assert(take(b, got) == 20 + length && !take(b, got));
    /* A reply to our Ethernet address does not match a socket bound to 127.0.0.1. */
    assert(nx_net_raw_send(b, chain(message, length), ME) == NX_NET_OK);
    assert(nx_net_loop_service(&lo, 10) == 0 && nx_net_loop_service(&lo, 10) == 0);
    assert(take(b, got) && !take(a, got) && readable == 1);
    /* Connected: only that peer, and no other destination may be named. */
    assert(nx_net_raw_connect(b, PEER) == NX_NET_OK);
    assert(nx_net_raw_send(b, chain(message, length), LOCALHOST) == NX_NET_EXISTS);
    sent = 0;
    assert(nx_net_raw_send(b, chain(message, length), 0) == NX_NET_OK && sent == 1);
    assert(nx_net_get32(frames[0] + 12) == ME && nx_net_get32(frames[0] + 16) == PEER && !sum(frames[0] + 20, length));
    assert(nx_net_raw_send(a, chain(message, length), LOCALHOST) == NX_NET_OK && nx_net_loop_service(&lo, 10) == 0 &&
           nx_net_loop_service(&lo, 10) == 0 && take(a, got) && !take(b, got));
    static struct raw reply;
    uint8_t body[64];
    build(&reply, PEER, ME, NX_NET_IPPROTO_ICMP, 0, body, icmp(body, NX_NET_ICMP_ECHOREPLY, 0, 1, data, 8));
    feed(&em, &reply);
    assert(take(b, got) == reply.length && !take(a, got));
    /* Refusals free the message. */
    assert(nx_net_raw_send(a, chain(message, 7), LOCALHOST) == NX_NET_INVALID);
    assert(nx_net_raw_send(a, chain(message, 8), 0) == NX_NET_INVALID);
    assert(nx_net_raw_send(a, nx_net_mbuf_get(&pool, false), LOCALHOST) == NX_NET_INVALID);
    assert(nx_net_raw_send(a, chain(message, 8), IP(224, 0, 0, 1)) == NX_NET_INVALID);
    assert(nx_net_raw_send(a, chain(message, 8), IP(172, 16, 0, 1)) == NX_NET_OK);   /* via the gateway */
    /* A message split before its header ends is pulled up. */
    struct nx_net_mbuf *m = chain(message, 3), *rest = nx_net_mbuf_get(&pool, false);
    assert(nx_net_mbuf_append(rest, message + 3, length - 3));
    nx_net_mbuf_cat(m, rest);
    assert(nx_net_raw_send(a, m, LOCALHOST) == NX_NET_OK && nx_net_loop_service(&lo, 10) == 0 &&
           nx_net_loop_service(&lo, 10) == 0 && take(a, got) == 20 + length);

    /* The 8192-byte receive queue is bounded; overflow is dropped and counted. */
    nx_net_raw_connect(b, 0);
    for (unsigned i = 0; i < 300; ++i) feed(&em, &reply);
    size_t records = 0;
    for (struct nx_net_mbuf *q = b->receive.head; q; q = q->nextpkt) ++records;
    assert(records >= 100 && records < 300 && b->receive.cc == records * reply.length && b->receive.cc <= NX_NET_RAW_RCVQ);
    assert(b->receive.drops == 300 - records && b->receive.mbcnt <= b->receive.mbmax);
    nx_net_raw_flush();
    assert(!take(b, got) && b->receive.cc == 0 && b->open);
    nx_net_raw_close(a);
    nx_net_raw_close(b);
    assert(nx_net_raw_open_count() == 0 && pool.stats.mbufs == 0);
}

int main(void)
{
    nx_net_budget_init(&budget, 512);
    assert(nx_net_mbuf_pool_init(&pool, &ops, &budget, (struct nx_net_mbuf_limits){ 1000, 200 }));
    nx_net_arp_init(&pool, clock_ms);
    nx_net_ip_init(&pool, clock_ms);
    nx_net_icmp_init();
    nx_net_if_set_change_hook(changed);
    assert(nx_net_proto_register(NX_NET_ETHERTYPE_IP, nx_net_ip_input) == NX_NET_OK);
    assert(nx_net_ip_register(NX_NET_IPPROTO_ICMP, nx_net_icmp_input) == NX_NET_OK);
    nx_net_ip_set_unknown_hook(nx_net_icmp_unknown);
    assert(nx_net_icmp_register_notify(17, NULL) == NX_NET_INVALID);
    assert(nx_net_icmp_register_notify(17, notified) == NX_NET_OK && nx_net_icmp_register_notify(17, notified) == NX_NET_EXISTS);
    for (uint8_t p = 30; p < 33; ++p) assert(nx_net_icmp_register_notify(p, notified) == NX_NET_OK);
    assert(nx_net_icmp_register_notify(40, notified) == NX_NET_FULL);
    for (uint8_t p = 30; p < 33; ++p) nx_net_icmp_unregister_notify(p);
    nx_net_loop_init(&lo, "lo0", kick, NULL);
    nx_net_ether_init(&em, "em0", mac, &fake, NULL);
    assert(nx_net_if_attach(&lo) == NX_NET_OK && nx_net_if_attach(&em) == NX_NET_OK);
    assert(nx_net_if_up(&lo) == NX_NET_OK && nx_net_if_up(&em) == NX_NET_OK);
    assert(nx_net_inet_set(&lo, LOCALHOST, IP(255, 0, 0, 0)) == NX_NET_OK);
    assert(nx_net_inet_set(&em, ME, IP(255, 255, 255, 0)) == NX_NET_OK);
    assert(nx_net_route_set_default(PEER) == NX_NET_OK);
    /* Resolve the gateway so output is sent at once. */
    uint8_t arp[28] = {0, 1, 8, 0, 6, 4, 0, 2};
    memcpy(arp + 8, peer_mac, 6); nx_net_put32(arp + 14, PEER); memcpy(arp + 18, mac, 6); nx_net_put32(arp + 24, ME);
    nx_net_arp_input(&em, chain(arp, sizeof(arp)));

    echo();
    errors_out();
    struct nx_net_raw *raw;
    assert(nx_net_raw_open(true, NULL, NULL, &raw) == NX_NET_OK);
    errors_in(raw);
    nx_net_raw_close(raw);
    sockets();
    nx_net_if_detach(&em);
    nx_net_if_detach(&lo);
    nx_net_route_clear_default();
    nx_net_arp_flush(NULL);
    nx_net_ip_flush();
    assert(pool.stats.mbufs == 0 && pool.stats.clusters == 0 && nx_net_mbuf_pool_destroy(&pool) && !live_pages);
    puts("PASS: ICMP echo, checksums, error generation and suppression, error propagation, malformed input, privileged bounded raw sockets and mbuf ownership");
    return 0;
}
