#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/arp.h"
#include "net/compat.h"
#include "net/udp.h"
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
static uint16_t udp_sum(nx_net_addr_t src, nx_net_addr_t dst, const uint8_t *udp, size_t length)
{
    struct nx_net_checksum c;
    nx_net_checksum_init(&c);
    nx_net_checksum_pseudo(&c, src, dst, NX_NET_IPPROTO_UDP, (uint16_t)length);
    nx_net_checksum_update(&c, udp, length);
    return nx_net_checksum_finish(&c);
}
static uint8_t pattern(size_t i) { return (uint8_t)(i * 7 + 3); }
static struct nx_net_mbuf *chain(const void *bytes, size_t length)
{
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    assert(m && nx_net_mbuf_append(m, bytes, length));
    return m;
}
static struct nx_net_mbuf *payload(size_t length)
{
    static uint8_t bytes[10000];
    for (size_t i = 0; i < length; ++i) bytes[i] = pattern(i);
    return chain(bytes, length);
}

/* A UDP datagram with a correct (or, with checksum false, zero) checksum. */
struct raw { uint8_t bytes[2000]; size_t length; };
static void build(struct raw *r, nx_net_addr_t src, nx_net_port_t sport, nx_net_addr_t dst, nx_net_port_t dport,
                  size_t data, bool checksum)
{
    memset(r->bytes, 0, 28);
    r->length = 28 + data;
    r->bytes[0] = 0x45;
    nx_net_put16(r->bytes + 2, (uint16_t)r->length);
    r->bytes[8] = 64;
    r->bytes[9] = NX_NET_IPPROTO_UDP;
    nx_net_put32(r->bytes + 12, src);
    nx_net_put32(r->bytes + 16, dst);
    nx_net_put16(r->bytes + 10, sum(r->bytes, 20));
    uint8_t *u = r->bytes + 20;
    nx_net_put16(u, sport);
    nx_net_put16(u + 2, dport);
    nx_net_put16(u + 4, (uint16_t)(8 + data));
    for (size_t i = 0; i < data; ++i) u[8 + i] = pattern(i);
    if (checksum) {
        uint16_t s = udp_sum(src, dst, u, 8 + data);
        nx_net_put16(u + 6, s ? s : 0xffff);
    }
}
static void feed(struct nx_net_if *ifp, const struct raw *r) { nx_net_ip_input(ifp, chain(r->bytes, r->length)); }

/* Receive one datagram; returns its length or SIZE_MAX when none. */
static uint8_t got[10000];
static nx_net_addr_t got_from;
static nx_net_port_t got_port;
static size_t take(struct nx_net_udp *u)
{
    struct nx_net_mbuf *m = nx_net_udp_receive(u, &got_from, &got_port);
    if (!m) return SIZE_MAX;
    size_t length = nx_net_mbuf_length(m);
    assert(length <= sizeof(got) && nx_net_mbuf_copydata(m, 0, length, got));
    nx_net_mbuf_freem(m);
    return length;
}
static bool patterned(size_t length)
{
    for (size_t i = 0; i < length; ++i) if (got[i] != pattern(i)) return false;
    return true;
}
static unsigned wakes;
static void wake(void *a) { (void)a; ++wakes; }

static void binding(void)
{
    struct nx_net_udp *all[NX_NET_UDP_SOCKETS], *u;
    for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i) assert(nx_net_udp_open(NULL, NULL, &all[i]) == NX_NET_OK);
    assert(nx_net_udp_open(NULL, NULL, &u) == NX_NET_FULL);
    for (unsigned i = 0; i < NX_NET_UDP_SOCKETS; ++i) nx_net_udp_close(all[i]);
    assert(nx_net_udp_open_count() == 0);

    struct nx_net_udp *a, *b, *c;
    assert(nx_net_udp_open(NULL, NULL, &a) == NX_NET_OK && nx_net_udp_open(NULL, NULL, &b) == NX_NET_OK);
    assert(nx_net_udp_open(NULL, NULL, &c) == NX_NET_OK);
    /* Addresses must be ours; reserved ports need privilege; bind once. */
    assert(nx_net_udp_bind(a, IP(10, 0, 2, 99), 5000, false) == NX_NET_INVALID);
    assert(nx_net_udp_bind(a, 0, 53, false) == NX_NET_DENIED && nx_net_udp_bind(a, 0, 1023, false) == NX_NET_DENIED);
    assert(nx_net_udp_bind(a, 0, 53, true) == NX_NET_OK && a->local_port == 53);
    assert(nx_net_udp_bind(a, 0, 54, true) == NX_NET_INVALID);
    /* Conflicts: wildcard against anything, the same address, not different ones. */
    assert(nx_net_udp_bind(b, ME, 53, true) == NX_NET_EXISTS && nx_net_udp_bind(b, 0, 53, true) == NX_NET_EXISTS);
    assert(nx_net_udp_bind(b, ME, 1024, false) == NX_NET_OK);
    assert(nx_net_udp_bind(c, LOCALHOST, 1024, false) == NX_NET_OK);
    nx_net_udp_close(c);
    assert(nx_net_udp_open(NULL, NULL, &c) == NX_NET_OK && nx_net_udp_bind(c, ME, 1024, false) == NX_NET_EXISTS);
    assert(nx_net_udp_bind(c, 0, 1024, false) == NX_NET_EXISTS);
    /* Reuse only when both sockets allow it. */
    nx_net_udp_set_reuse(c, true);
    assert(nx_net_udp_bind(c, 0, 1024, false) == NX_NET_EXISTS);
    nx_net_udp_set_reuse(b, true);
    assert(nx_net_udp_bind(c, 0, 1024, false) == NX_NET_OK);
    nx_net_udp_close(a); nx_net_udp_close(b); nx_net_udp_close(c);

    /* Ephemeral ports: distinct, in range, wrapping, skipping ports in use. */
    nx_net_udp_init();
    assert(nx_net_udp_open(NULL, NULL, &a) == NX_NET_OK && nx_net_udp_open(NULL, NULL, &b) == NX_NET_OK);
    assert(nx_net_udp_open(NULL, NULL, &c) == NX_NET_OK);
    assert(nx_net_udp_bind(a, 0, NX_NET_UDP_EPHEMERAL_FIRST + 1, false) == NX_NET_OK);
    assert(nx_net_udp_bind(b, 0, 0, false) == NX_NET_OK && b->local_port == NX_NET_UDP_EPHEMERAL_FIRST);
    assert(nx_net_udp_bind(c, 0, 0, false) == NX_NET_OK && c->local_port == NX_NET_UDP_EPHEMERAL_FIRST + 2);
    nx_net_udp_close(b);
    nx_net_udp_close(c);
    for (unsigned i = 0; i < NX_NET_UDP_EPHEMERAL_LAST - NX_NET_UDP_EPHEMERAL_FIRST - 2; ++i) {
        assert(nx_net_udp_open(NULL, NULL, &b) == NX_NET_OK && nx_net_udp_bind(b, 0, 0, false) == NX_NET_OK);
        assert(b->local_port >= NX_NET_UDP_EPHEMERAL_FIRST && b->local_port != a->local_port);
        nx_net_udp_close(b);
    }
    assert(nx_net_udp_open(NULL, NULL, &b) == NX_NET_OK && nx_net_udp_bind(b, 0, 0, false) == NX_NET_OK &&
           b->local_port == NX_NET_UDP_EPHEMERAL_FIRST);   /* wrapped, skipping a */
    /* Connect validates and binds; 0 disconnects. */
    assert(nx_net_udp_open(NULL, NULL, &c) == NX_NET_OK);
    assert(nx_net_udp_connect(c, IP(224, 0, 0, 1), 7) == NX_NET_INVALID && nx_net_udp_connect(c, PEER, 0) == NX_NET_INVALID);
    assert(!c->local_port && nx_net_udp_connect(c, PEER, 7) == NX_NET_OK && c->local_port && c->foreign_port == 7);
    assert(nx_net_udp_connect(c, 0, 0) == NX_NET_OK && !c->foreign_addr && !c->foreign_port && c->local_port);
    nx_net_udp_close(a); nx_net_udp_close(b); nx_net_udp_close(c);
    assert(nx_net_udp_open_count() == 0);
}

static void output(void)
{
    struct nx_net_udp *u;
    assert(nx_net_udp_open(NULL, NULL, &u) == NX_NET_OK);
    sent = 0;
    /* Unbound: an ephemeral port is chosen; the checksum covers the pseudoheader. */
    assert(nx_net_udp_send(u, payload(100), PEER, 9999) == NX_NET_OK && sent == 1 && frame_lengths[0] == 128);
    const uint8_t *h = frames[0], *d = h + 20;
    assert(h[9] == 17 && nx_net_get32(h + 12) == ME && nx_net_get32(h + 16) == PEER);
    assert(nx_net_get16(d) == u->local_port && u->local_port >= NX_NET_UDP_EPHEMERAL_FIRST);
    assert(nx_net_get16(d + 2) == 9999 && nx_net_get16(d + 4) == 108 && nx_net_get16(d + 6) && !udp_sum(ME, PEER, d, 108));
    for (size_t i = 0; i < 100; ++i) assert(d[8 + i] == pattern(i));
    /* Zero-length, and the largest datagram (fragmented on Ethernet). */
    assert(nx_net_udp_send(u, payload(0), PEER, 9999) == NX_NET_OK && sent == 2 && frame_lengths[1] == 28);
    assert(nx_net_get16(frames[1] + 24) == 8 && !udp_sum(ME, PEER, frames[1] + 20, 8));
    assert(nx_net_udp_send(u, payload(NX_NET_UDP_SNDMAX), PEER, 9999) == NX_NET_OK && sent == 9);
    assert(nx_net_get16(frames[2] + 6) == 0x2000 && nx_net_get16(frames[2] + 24) == NX_NET_UDP_SNDMAX + 8);
    /* Refusals free the payload. */
    struct nx_net_udp_stats before = nx_net_udp_stats();
    assert(nx_net_udp_send(u, payload(NX_NET_UDP_SNDMAX + 1), PEER, 9999) == NX_NET_TOO_BIG);
    assert(nx_net_udp_send(u, payload(1), 0, 9999) == NX_NET_INVALID);
    assert(nx_net_udp_send(u, payload(1), PEER, 0) == NX_NET_INVALID);
    assert(nx_net_udp_send(u, nx_net_mbuf_get(&pool, false), PEER, 9999) == NX_NET_INVALID);
    assert(nx_net_udp_send(u, payload(1), IP(224, 0, 0, 1), 9999) == NX_NET_INVALID);
    nx_net_route_clear_default();
    assert(nx_net_udp_send(u, payload(1), IP(172, 16, 0, 1), 9999) == NX_NET_UNREACHABLE);
    assert(nx_net_route_set_default(PEER) == NX_NET_OK);
    assert(nx_net_udp_stats().output_errors == before.output_errors + 6 && sent == 9);
    /* Connected: the peer is implied and no other may be named. */
    assert(nx_net_udp_connect(u, PEER, 53) == NX_NET_OK);
    assert(nx_net_udp_send(u, payload(4), PEER, 54) == NX_NET_EXISTS);
    assert(nx_net_udp_send(u, payload(4), 0, 0) == NX_NET_OK && sent == 10 && nx_net_get16(frames[9] + 22) == 53);
    /* A bound address is the source. */
    nx_net_udp_close(u);
    assert(nx_net_udp_open(NULL, NULL, &u) == NX_NET_OK && nx_net_udp_bind(u, LOCALHOST, 7000, false) == NX_NET_OK);
    assert(nx_net_udp_send(u, payload(4), PEER, 53) == NX_NET_OK && sent == 11);
    assert(nx_net_get32(frames[10] + 12) == LOCALHOST && !udp_sum(LOCALHOST, PEER, frames[10] + 20, 12));
    nx_net_udp_close(u);
    assert(pool.stats.mbufs == 0);
}

static void input(void)
{
    static struct raw r;
    struct nx_net_udp *wild, *bound, *conn;
    wakes = 0;
    assert(nx_net_udp_open(wake, NULL, &wild) == NX_NET_OK && nx_net_udp_bind(wild, 0, 7000, false) == NX_NET_OK);
    struct nx_net_udp_stats before = nx_net_udp_stats();
    /* Delivery with the source; checksum optional; zero-length. */
    build(&r, PEER, 4000, ME, 7000, 30, true); feed(&em, &r);
    assert(wakes == 1 && take(wild) == 30 && patterned(30) && got_from == PEER && got_port == 4000);
    build(&r, PEER, 4000, ME, 7000, 31, false); feed(&em, &r);
    assert(take(wild) == 31 && patterned(31));
    build(&r, PEER, 4000, ME, 7000, 0, true); feed(&em, &r);
    assert(take(wild) == 0 && take(wild) == SIZE_MAX);
    /* IP bytes beyond the UDP length are trimmed. */
    build(&r, PEER, 4000, ME, 7000, 20, true);
    nx_net_put16(r.bytes + 24, 8 + 10);
    nx_net_put16(r.bytes + 26, 0);
    uint16_t s = udp_sum(PEER, ME, r.bytes + 20, 18);
    nx_net_put16(r.bytes + 26, s ? s : 0xffff);
    feed(&em, &r);
    assert(take(wild) == 10 && patterned(10));

    /* Malformed: bad checksum, truncated, short length field, no header. */
    sent = 0;
    build(&r, PEER, 4000, ME, 7000, 30, true); r.bytes[40] ^= 1; feed(&em, &r);
    build(&r, PEER, 4000, ME, 7000, 30, true); nx_net_put16(r.bytes + 24, 39); feed(&em, &r);
    build(&r, PEER, 4000, ME, 7000, 30, true); nx_net_put16(r.bytes + 24, 7); feed(&em, &r);
    build(&r, PEER, 4000, ME, 7000, 0, true);
    r.length = 27; nx_net_put16(r.bytes + 2, 27); nx_net_put16(r.bytes + 10, 0); nx_net_put16(r.bytes + 10, sum(r.bytes, 20));
    feed(&em, &r);
    struct nx_net_udp_stats st = nx_net_udp_stats();
    assert(take(wild) == SIZE_MAX && sent == 0 && st.bad_checksum == before.bad_checksum + 1);
    assert(st.bad_length == before.bad_length + 2 && st.too_short == before.too_short + 1);
    assert(st.delivered == before.delivered + 4 && st.received == before.received + 8);

    /* Demultiplexing: fewest wildcards wins; connected sockets filter peers. */
    assert(nx_net_udp_open(NULL, NULL, &bound) == NX_NET_OK && nx_net_udp_open(NULL, NULL, &conn) == NX_NET_OK);
    nx_net_udp_set_reuse(wild, true); nx_net_udp_set_reuse(bound, true); nx_net_udp_set_reuse(conn, true);
    assert(nx_net_udp_bind(bound, ME, 7000, false) == NX_NET_OK && nx_net_udp_bind(conn, ME, 7000, false) == NX_NET_OK);
    assert(nx_net_udp_connect(conn, PEER, 4000) == NX_NET_OK);
    build(&r, PEER, 4000, ME, 7000, 5, true); feed(&em, &r);
    assert(take(conn) == 5 && take(bound) == SIZE_MAX && take(wild) == SIZE_MAX);
    build(&r, PEER, 4001, ME, 7000, 5, true); feed(&em, &r);
    assert(take(bound) == 5 && take(conn) == SIZE_MAX && take(wild) == SIZE_MAX);
    build(&r, PEER, 4001, IP(10, 0, 2, 15), 7000, 5, true); feed(&lo, &r);
    assert(take(bound) == 5);
    build(&r, PEER, 4001, LOCALHOST, 7000, 5, true); feed(&lo, &r);
    assert(take(wild) == 5 && take(bound) == SIZE_MAX);
    /* Broadcasts reach every matching socket and never provoke errors. */
    build(&r, PEER, 4000, IP(10, 0, 2, 255), 7000, 6, true); feed(&em, &r);
    build(&r, PEER, 4000, UINT32_MAX, 7000, 7, true); feed(&em, &r);
    assert(take(wild) == 6 && take(wild) == 7 && take(conn) == SIZE_MAX && take(bound) == SIZE_MAX);
    nx_net_udp_close(bound);
    assert(nx_net_udp_bind(conn, 0, 0, false) == NX_NET_INVALID);
    nx_net_udp_close(conn);
    assert(nx_net_udp_open(NULL, NULL, &conn) == NX_NET_OK && nx_net_udp_bind(conn, 0, 7000, false) == NX_NET_EXISTS);
    nx_net_udp_set_reuse(conn, true);
    assert(nx_net_udp_bind(conn, 0, 7000, false) == NX_NET_OK);
    build(&r, PEER, 4000, UINT32_MAX, 7000, 8, true); feed(&em, &r);
    assert(take(wild) == 8 && patterned(8) && take(conn) == 8 && patterned(8));
    build(&r, PEER, 4000, UINT32_MAX, 7001, 8, true); feed(&em, &r);
    assert(sent == 0 && nx_net_udp_stats().broadcast_drops == before.broadcast_drops + 1);
    nx_net_udp_close(conn);

    /* No listener: ICMP port unreachable quoting the datagram. */
    build(&r, PEER, 4000, ME, 7001, 12, true); feed(&em, &r);
    assert(sent == 1 && frames[0][9] == 1 && frames[0][20] == 3 && frames[0][21] == 3 && !sum(frames[0] + 20, 36));
    assert(!memcmp(frames[0] + 28, r.bytes, 28) && nx_net_udp_stats().no_port == before.no_port + 1);

    /* The receive queue is bounded; overflow is dropped and counted. */
    build(&r, PEER, 4000, ME, 7000, 1000, true);
    unsigned fed = 0;
    for (; fed < 60; ++fed) feed(&em, &r);
    size_t records = 0;
    for (struct nx_net_mbuf *q = wild->receive.head; q; q = q->nextpkt) ++records;
    assert(records == NX_NET_UDP_RCVQ / 1008 && wild->receive.cc <= NX_NET_UDP_RCVQ);
    assert(nx_net_udp_stats().queue_drops == fed - records && wild->receive.drops == fed - records);
    while (take(wild) == 1000) assert(patterned(1000));
    /* Many tiny datagrams hit the mbuf limit instead. */
    build(&r, PEER, 4000, ME, 7000, 0, true);
    for (unsigned i = 0; i < 1000; ++i) feed(&em, &r);
    assert(wild->receive.mbcnt <= wild->receive.mbmax && wild->receive.drops > fed - records);
    nx_net_udp_flush();
    assert(take(wild) == SIZE_MAX && wild->open);
    nx_net_udp_close(wild);
    assert(pool.stats.mbufs == 0);
}

static void errors(void)
{
    static struct raw r, inner;
    struct nx_net_udp *conn, *other, *unconn;
    wakes = 0;
    assert(nx_net_udp_open(wake, NULL, &conn) == NX_NET_OK && nx_net_udp_connect(conn, PEER, 53) == NX_NET_OK);
    assert(nx_net_udp_open(wake, NULL, &other) == NX_NET_OK && nx_net_udp_connect(other, PEER, 54) == NX_NET_OK);
    assert(nx_net_udp_open(wake, NULL, &unconn) == NX_NET_OK && nx_net_udp_bind(unconn, 0, 0, false) == NX_NET_OK);
    /* A port unreachable error about conn's datagram reaches conn only. */
    build(&inner, ME, conn->local_port, PEER, 53, 4, true);
    uint8_t body[64] = { 3, 3 };
    memcpy(body + 8, inner.bytes, 28);
    nx_net_put16(body + 2, sum(body, 36));
    memset(r.bytes, 0, 20);
    r.bytes[0] = 0x45; nx_net_put16(r.bytes + 2, 56); r.bytes[8] = 64; r.bytes[9] = 1;
    nx_net_put32(r.bytes + 12, PEER); nx_net_put32(r.bytes + 16, ME);
    nx_net_put16(r.bytes + 10, sum(r.bytes, 20));
    memcpy(r.bytes + 20, body, 36);
    r.length = 56;
    feed(&em, &r);
    assert(wakes == 1 && nx_net_udp_take_error(conn) == NX_NET_REFUSED && nx_net_udp_take_error(conn) == NX_NET_OK);
    assert(nx_net_udp_take_error(other) == NX_NET_OK && nx_net_udp_take_error(unconn) == NX_NET_OK);
    /* Needfrag is reported too; reconnecting clears a pending error. */
    body[1] = 4; nx_net_put16(body + 6, 576); nx_net_put16(body + 2, 0); nx_net_put16(body + 2, sum(body, 36));
    memcpy(r.bytes + 20, body, 36);
    feed(&em, &r);
    assert(conn->error == NX_NET_TOO_BIG && nx_net_udp_connect(conn, PEER, 53) == NX_NET_OK && conn->error == NX_NET_OK);
    assert(nx_net_udp_stats().errors_notified == 2);
    nx_net_udp_close(conn); nx_net_udp_close(other); nx_net_udp_close(unconn);
    assert(pool.stats.mbufs == 0);
}

static void loopback(void)
{
    struct nx_net_udp *server, *client;
    assert(nx_net_udp_open(NULL, NULL, &server) == NX_NET_OK && nx_net_udp_bind(server, LOCALHOST, 7, true) == NX_NET_OK);
    assert(nx_net_udp_open(NULL, NULL, &client) == NX_NET_OK && nx_net_udp_connect(client, LOCALHOST, 7) == NX_NET_OK);
    assert(nx_net_udp_send(client, payload(NX_NET_UDP_SNDMAX), 0, 0) == NX_NET_OK && nx_net_loop_service(&lo, 10) == 0);
    assert(take(server) == NX_NET_UDP_SNDMAX && patterned(NX_NET_UDP_SNDMAX) && got_from == LOCALHOST && got_port == client->local_port);
    struct nx_net_mbuf *m = payload(3);
    assert(nx_net_udp_send(server, m, got_from, got_port) == NX_NET_OK && nx_net_loop_service(&lo, 10) == 0);
    assert(take(client) == 3 && got_port == 7);
    /* A closed port answers with an error the connected client sees. */
    nx_net_udp_close(server);
    assert(nx_net_udp_send(client, payload(3), 0, 0) == NX_NET_OK);
    assert(nx_net_loop_service(&lo, 10) == 0 && nx_net_loop_service(&lo, 10) == 0);
    assert(nx_net_udp_take_error(client) == NX_NET_REFUSED && take(client) == SIZE_MAX);
    nx_net_udp_close(client);
    assert(pool.stats.mbufs == 0);
}

int main(void)
{
    nx_net_budget_init(&budget, 512);
    assert(nx_net_mbuf_pool_init(&pool, &ops, &budget, (struct nx_net_mbuf_limits){ 1500, 200 }));
    nx_net_arp_init(&pool, clock_ms);
    nx_net_ip_init(&pool, clock_ms);
    nx_net_icmp_init();
    nx_net_udp_init();
    nx_net_if_set_change_hook(changed);
    assert(nx_net_proto_register(NX_NET_ETHERTYPE_IP, nx_net_ip_input) == NX_NET_OK);
    assert(nx_net_ip_register(NX_NET_IPPROTO_ICMP, nx_net_icmp_input) == NX_NET_OK);
    assert(nx_net_ip_register(NX_NET_IPPROTO_UDP, nx_net_udp_input) == NX_NET_OK);
    assert(nx_net_icmp_register_notify(NX_NET_IPPROTO_UDP, nx_net_udp_notify) == NX_NET_OK);
    nx_net_ip_set_unknown_hook(nx_net_icmp_unknown);
    nx_net_loop_init(&lo, "lo0", kick, NULL);
    nx_net_ether_init(&em, "em0", mac, &fake, NULL);
    assert(nx_net_if_attach(&lo) == NX_NET_OK && nx_net_if_attach(&em) == NX_NET_OK);
    assert(nx_net_if_up(&lo) == NX_NET_OK && nx_net_if_up(&em) == NX_NET_OK);
    assert(nx_net_inet_set(&lo, LOCALHOST, IP(255, 0, 0, 0)) == NX_NET_OK);
    assert(nx_net_inet_set(&em, ME, IP(255, 255, 255, 0)) == NX_NET_OK);
    assert(nx_net_route_set_default(PEER) == NX_NET_OK);
    uint8_t arp[28] = {0, 1, 8, 0, 6, 4, 0, 2};
    memcpy(arp + 8, peer_mac, 6); nx_net_put32(arp + 14, PEER); memcpy(arp + 18, mac, 6); nx_net_put32(arp + 24, ME);
    nx_net_arp_input(&em, chain(arp, sizeof(arp)));

    binding();
    output();
    input();
    errors();
    loopback();
    nx_net_if_detach(&em);
    nx_net_if_detach(&lo);
    nx_net_route_clear_default();
    nx_net_arp_flush(NULL);
    nx_net_ip_flush();
    assert(pool.stats.mbufs == 0 && pool.stats.clusters == 0 && nx_net_mbuf_pool_destroy(&pool) && !live_pages);
    puts("PASS: UDP bind/conflict/reuse/privileged ports, ephemeral allocation, demultiplexing, broadcast, checksums, zero-length/truncated datagrams, port unreachable, ICMP error reporting, bounded queues and mbuf ownership");
    return 0;
}
