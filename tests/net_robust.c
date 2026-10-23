/* Whole-stack robustness under ASan/UBSan (task 430): ARP, IPv4 with
 * fragments, ICMP, UDP, TCP and raw sockets behind a fake Ethernet card and
 * lo0. Deterministic mutation of valid frames, pool exhaustion, timer expiry
 * and teardown; after each phase every allocation counter must return to its
 * baseline and peaks must stay within the configured bounds. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/arp.h"
#include "net/compat.h"
#include "net/tcp.h"
#include "net/udp.h"
#include "net/wire.h"

#define IP(a, b, c, d) ((nx_net_addr_t)(a) << 24 | (nx_net_addr_t)(b) << 16 | (nx_net_addr_t)(c) << 8 | (d))
#define ME IP(10, 0, 2, 15)
#define PEER IP(10, 0, 2, 2)
#define LOCALHOST IP(127, 0, 0, 1)

static size_t live_pages;
static void *page_alloc(void *c) { (void)c; ++live_pages; void *p = aligned_alloc(4096, 4096); assert(p); return p; }
static void page_free(void *c, void *p) { (void)c; --live_pages; free(p); }
static const struct nx_net_page_ops ops = { page_alloc, page_free, NULL };
static struct nx_net_budget budget;
static struct nx_net_mbuf_pool pool;
static uint64_t now = 1000;
static uint64_t clock_ms(void) { return now; }

/* Deterministic xorshift64: every run sees the same frames. */
static uint64_t seed = 0x9e3779b97f4a7c15ull;
static uint64_t rnd(void) { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed; }
static size_t below(size_t n) { return n ? rnd() % n : 0; }

static uint64_t transmitted;
static bool fake_transmit(struct nx_net_if *i, struct nx_net_mbuf *m) { (void)i; ++transmitted; nx_net_mbuf_freem(m); return true; }
static const struct nx_net_if_ops fake = { NULL, NULL, fake_transmit };
static const uint8_t mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static const uint8_t peer_mac[6] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};
static struct nx_net_if lo, em;
static void changed(struct nx_net_if *ifp) { nx_net_arp_if_changed(ifp); nx_net_ip_if_changed(ifp); }
static void kick(void *a) { (void)a; }

static void stack_up(size_t pages, size_t mbufs, size_t clusters)
{
    nx_net_budget_init(&budget, pages);
    assert(nx_net_mbuf_pool_init(&pool, &ops, &budget, (struct nx_net_mbuf_limits){ mbufs, clusters }));
    nx_net_arp_init(&pool, clock_ms);
    nx_net_ip_init(&pool, clock_ms);
    nx_net_icmp_init();
    nx_net_udp_init();
    nx_net_tcp_init(&pool, clock_ms, rnd());
    nx_net_if_set_change_hook(changed);
    assert(nx_net_proto_register(NX_NET_ETHERTYPE_IP, nx_net_ip_input) == NX_NET_OK);
    assert(nx_net_proto_register(NX_NET_ETHERTYPE_ARP, nx_net_arp_input) == NX_NET_OK);
    assert(nx_net_ip_register(NX_NET_IPPROTO_ICMP, nx_net_icmp_input) == NX_NET_OK);
    assert(nx_net_ip_register(NX_NET_IPPROTO_UDP, nx_net_udp_input) == NX_NET_OK);
    assert(nx_net_ip_register(NX_NET_IPPROTO_TCP, nx_net_tcp_input) == NX_NET_OK);
    assert(nx_net_icmp_register_notify(NX_NET_IPPROTO_UDP, nx_net_udp_notify) == NX_NET_OK);
    assert(nx_net_icmp_register_notify(NX_NET_IPPROTO_TCP, nx_net_tcp_notify) == NX_NET_OK);
    nx_net_ip_set_unknown_hook(nx_net_icmp_unknown);
    nx_net_loop_init(&lo, "lo0", kick, NULL);
    lo.mtu = 1500;   /* many segments, not a few 16 KiB ones */
    nx_net_ether_init(&em, "em0", mac, &fake, NULL);
    assert(nx_net_if_attach(&lo) == NX_NET_OK && nx_net_if_attach(&em) == NX_NET_OK);
    assert(nx_net_if_up(&lo) == NX_NET_OK && nx_net_if_up(&em) == NX_NET_OK);
    assert(nx_net_inet_set(&lo, LOCALHOST, IP(255, 0, 0, 0)) == NX_NET_OK);
    assert(nx_net_inet_set(&em, ME, IP(255, 255, 255, 0)) == NX_NET_OK);
    assert(nx_net_route_set_default(PEER) == NX_NET_OK);
}
/* Every allocation back at its baseline, and peaks within their bounds. */
static void baseline(const char *phase)
{
    assert(pool.stats.mbufs == 0 && pool.stats.clusters == 0);
    assert(nx_net_ip_reassembling() == 0 && nx_net_tcp_used() == 0);
    assert(nx_net_udp_open_count() == 0 && nx_net_raw_open_count() == 0);
    assert(pool.stats.peak_mbufs <= pool.limits.mbufs && pool.stats.peak_clusters <= pool.limits.clusters);
    assert(budget.peak <= budget.limit);
    printf("  %s: baseline restored; peaks %zu/%zu mbufs, %zu/%zu clusters, %zu/%zu pages, %llu allocation failures\n",
           phase, pool.stats.peak_mbufs, pool.limits.mbufs, pool.stats.peak_clusters, pool.limits.clusters,
           budget.peak, budget.limit, (unsigned long long)pool.stats.failures);
}
static void stack_down(void)
{
    if (em.attached) nx_net_if_detach(&em);
    if (lo.attached) nx_net_if_detach(&lo);
    nx_net_route_clear_default();
    nx_net_tcp_flush();
    nx_net_udp_flush();
    nx_net_raw_flush();
    nx_net_arp_flush(NULL);
    nx_net_ip_flush();
    nx_net_ip_set_unknown_hook(NULL);
    nx_net_icmp_unregister_notify(NX_NET_IPPROTO_UDP);
    nx_net_icmp_unregister_notify(NX_NET_IPPROTO_TCP);
    nx_net_ip_unregister(NX_NET_IPPROTO_ICMP);
    nx_net_ip_unregister(NX_NET_IPPROTO_UDP);
    nx_net_ip_unregister(NX_NET_IPPROTO_TCP);
    nx_net_proto_unregister(NX_NET_ETHERTYPE_IP);
    nx_net_proto_unregister(NX_NET_ETHERTYPE_ARP);
    nx_net_if_set_change_hook(NULL);
    nx_net_tcp_init(&pool, clock_ms, 0);   /* frees attached-but-closed blocks too */
    nx_net_udp_init();
    assert(pool.stats.mbufs == 0 && nx_net_mbuf_pool_destroy(&pool) && !live_pages);
}
static void timers(void) { nx_net_arp_timer(); nx_net_ip_timer(); nx_net_tcp_timer(); }
static void advance(uint64_t ms, uint64_t step)
{
    for (uint64_t done = 0; done < ms; done += step) {
        now += step;
        timers();
        nx_net_loop_service(&lo, 1000);
    }
}

/* Frame construction: Ethernet, IPv4 and transport checksums. */
static uint16_t sum(const uint8_t *b, size_t n)
{
    struct nx_net_checksum c;
    nx_net_checksum_init(&c);
    nx_net_checksum_update(&c, b, n);
    return nx_net_checksum_finish(&c);
}
static void seal_ip(uint8_t *ip)
{
    size_t hl = (size_t)(ip[0] & 0xf) * 4;
    nx_net_put16(ip + 10, 0);
    nx_net_put16(ip + 10, sum(ip, hl));
}
/* Transport checksum over the IP total length, when the frame holds it. */
static void seal_transport(uint8_t *ip, size_t have)
{
    size_t hl = (size_t)(ip[0] & 0xf) * 4, total = nx_net_get16(ip + 2);
    if (hl < 20 || total < hl || total > have || (nx_net_get16(ip + 6) & 0x3fff)) return;
    uint8_t *t = ip + hl;
    size_t length = total - hl, at = ip[9] == 6 ? 16 : ip[9] == 17 ? 6 : ip[9] == 1 ? 2 : 0;
    if (!at || length < at + 2) return;
    nx_net_put16(t + at, 0);
    struct nx_net_checksum c;
    nx_net_checksum_init(&c);
    if (ip[9] != 1) nx_net_checksum_pseudo(&c, nx_net_get32(ip + 12), nx_net_get32(ip + 16), ip[9], (uint16_t)length);
    nx_net_checksum_update(&c, t, length);
    nx_net_put16(t + at, nx_net_checksum_finish(&c));
}
static size_t ether(uint8_t *f, uint16_t type)
{
    memcpy(f, mac, 6); memcpy(f + 6, peer_mac, 6); nx_net_put16(f + 12, type);
    return 14;
}
static size_t ipv4(uint8_t *f, nx_net_addr_t src, nx_net_addr_t dst, uint8_t protocol, uint16_t id, uint16_t fragment,
                   const uint8_t *body, size_t length)
{
    size_t at = ether(f, NX_NET_ETHERTYPE_IP);
    uint8_t *ip = f + at;
    memset(ip, 0, 20);
    ip[0] = 0x45; ip[8] = 64; ip[9] = protocol;
    nx_net_put16(ip + 2, (uint16_t)(20 + length));
    nx_net_put16(ip + 4, id);
    nx_net_put16(ip + 6, fragment);
    nx_net_put32(ip + 12, src); nx_net_put32(ip + 16, dst);
    memcpy(ip + 20, body, length);
    seal_ip(ip);
    seal_transport(ip, 20 + length);
    return at + 20 + length;
}
static void feed(struct nx_net_if *ifp, const uint8_t *frame, size_t length)
{
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    if (!m) return;   /* exhaustion: the "driver" drops it */
    if (!nx_net_mbuf_append(m, frame, length)) { nx_net_mbuf_freem(m); return; }
    nx_net_ether_input(ifp, m);
}

/* Phase 1: mutations of every kind of valid frame. */
#define SEEDS 10
static uint8_t seeds[SEEDS][1600];
static size_t seed_lengths[SEEDS];
static void make_seeds(nx_net_port_t tcp_port, nx_net_seq_t tcp_seq, nx_net_seq_t tcp_ack)
{
    uint8_t body[1500];
    memset(body, 0xa5, sizeof(body));
    size_t n = 0;
    /* ARP reply from a new neighbour, and a request for us. */
    uint8_t arp[28] = {0, 1, 8, 0, 6, 4, 0, 2};
    memcpy(arp + 8, peer_mac, 6); nx_net_put32(arp + 14, IP(10, 0, 2, 9)); memcpy(arp + 18, mac, 6); nx_net_put32(arp + 24, ME);
    seed_lengths[n] = ether(seeds[n], NX_NET_ETHERTYPE_ARP) + 28; memcpy(seeds[n] + 14, arp, 28); ++n;
    arp[7] = 1; memset(arp + 18, 0, 6);
    seed_lengths[n] = ether(seeds[n], NX_NET_ETHERTYPE_ARP) + 28; memcpy(seeds[n] + 14, arp, 28); ++n;
    /* ICMP echo request. */
    uint8_t icmp[64] = {8, 0, 0, 0, 0x12, 0x34, 0, 1};
    seed_lengths[n] = ipv4(seeds[n], PEER, ME, 1, 1, 0, icmp, sizeof(icmp)); ++n;
    /* UDP to a bound port, and to a closed one. */
    uint8_t udp[8 + 100] = {0x10, 0x00, 0x1b, 0x58, 0, 108};
    seed_lengths[n] = ipv4(seeds[n], PEER, ME, 17, 2, 0, udp, sizeof(udp)); ++n;
    udp[3] = 0x59;
    seed_lengths[n] = ipv4(seeds[n], PEER, ME, 17, 3, 0, udp, sizeof(udp)); ++n;
    /* TCP SYN to the listener, and data on the established connection. */
    uint8_t syn[24] = {0x0f, 0xa0, 0x1f, 0x40, 0, 0, 0, 1, 0, 0, 0, 0, 0x60, 0x02, 0x10, 0, 0, 0, 0, 0, 2, 4, 5, 0xb4};
    seed_lengths[n] = ipv4(seeds[n], PEER, ME, 6, 4, 0, syn, sizeof(syn)); ++n;
    uint8_t data[20 + 200] = {0x0f, 0xa1};
    nx_net_put16(data + 2, tcp_port); nx_net_put32(data + 4, tcp_seq); nx_net_put32(data + 8, tcp_ack);
    data[12] = 0x50; data[13] = 0x18; nx_net_put16(data + 14, 8000);
    seed_lengths[n] = ipv4(seeds[n], PEER, ME, 6, 5, 0, data, sizeof(data)); ++n;
    /* Fragments: first, middle and last of one datagram. */
    seed_lengths[n] = ipv4(seeds[n], PEER, ME, 17, 99, 0x2000, body, 800); ++n;
    seed_lengths[n] = ipv4(seeds[n], PEER, ME, 17, 99, 0x2000 | 100, body, 800); ++n;
    seed_lengths[n] = ipv4(seeds[n], PEER, ME, 17, 99, 200, body, 300); ++n;
    assert(n == SEEDS);
}
static size_t mutate(uint8_t *f, size_t length)
{
    for (unsigned k = 1 + (unsigned)below(4); k; --k) {
        switch (below(5)) {
        case 0: f[below(length)] ^= (uint8_t)(1u << below(8)); break;                 /* flip a bit */
        case 1: f[14 + below(length > 14 ? length - 14 : 1)] = (uint8_t)rnd(); break;  /* any header byte */
        case 2: length = 14 + below(length - 13); break;                               /* truncate */
        case 3: {                                                                      /* extend */
            size_t extra = below(65);
            for (size_t i = 0; i < extra && length < 1514; ++i) f[length++] = (uint8_t)rnd();
            break;
        }
        case 4: if (length >= 34) nx_net_put16(f + 14 + 2 + 2 * below(4), (uint16_t)rnd()); break;   /* length/id/offset */
        }
    }
    /* Often repair the checksums, so mutations reach the deeper layers. */
    if (length >= 34 && nx_net_get16(f + 12) == NX_NET_ETHERTYPE_IP && (f[14] >> 4) == 4) {
        size_t hl = (size_t)(f[14] & 0xf) * 4;
        if (hl >= 20 && 14 + hl <= length && below(4)) seal_ip(f + 14);
        if (below(2)) seal_transport(f + 14, length - 14);
    }
    return length;
}
static void fuzz(void)
{
    stack_up(256, 1000, 200);
    struct nx_net_udp *u;
    struct nx_net_raw *r;
    struct nx_net_tcp *l, *c;
    assert(nx_net_udp_open(NULL, NULL, &u) == NX_NET_OK && nx_net_udp_bind(u, 0, 7000, false) == NX_NET_OK);
    assert(nx_net_raw_open(true, NULL, NULL, &r) == NX_NET_OK);
    assert(nx_net_tcp_open(NULL, NULL, &l) == NX_NET_OK && nx_net_tcp_bind(l, 0, 8000, false) == NX_NET_OK);
    assert(nx_net_tcp_listen(l, 8) == NX_NET_OK);
    /* An established connection to the peer, its SYN-ACK forged here. */
    uint8_t arp[28] = {0, 1, 8, 0, 6, 4, 0, 2}, f[1600];
    memcpy(arp + 8, peer_mac, 6); nx_net_put32(arp + 14, PEER); memcpy(arp + 18, mac, 6); nx_net_put32(arp + 24, ME);
    size_t n = ether(f, NX_NET_ETHERTYPE_ARP); memcpy(f + n, arp, 28); feed(&em, f, n + 28);
    assert(nx_net_tcp_open(NULL, NULL, &c) == NX_NET_OK && nx_net_tcp_connect(c, PEER, 4001) == NX_NET_OK);
    uint8_t synack[24] = {0x0f, 0xa1};
    nx_net_put16(synack + 2, c->local_port); nx_net_put32(synack + 4, 5000); nx_net_put32(synack + 8, c->iss + 1);
    synack[12] = 0x60; synack[13] = 0x12; nx_net_put16(synack + 14, 8000); synack[20] = 2; synack[21] = 4; nx_net_put16(synack + 22, 1460);
    feed(&em, f, ipv4(f, PEER, ME, 6, 7, 0, synack, sizeof(synack)));
    assert(c->state == NX_NET_TCP_ESTABLISHED);
    make_seeds(c->local_port, 5001, c->iss + 1);
    uint8_t sink[4096];
    size_t got;
    for (unsigned i = 0; i < 40000; ++i) {
        unsigned which = (unsigned)below(SEEDS);
        memcpy(f, seeds[which], seed_lengths[which]);
        size_t length = below(8) ? mutate(f, seed_lengths[which]) : seed_lengths[which];
        feed(below(16) ? &em : &lo, f, length);
        if (!(i % 16)) { now += below(1500); timers(); nx_net_loop_service(&lo, 1000); }
        if (!(i % 64)) {   /* the application side keeps up, mostly */
            struct nx_net_mbuf *m;
            while ((m = nx_net_udp_receive(u, NULL, NULL))) nx_net_mbuf_freem(m);
            while ((m = nx_net_raw_receive(r))) nx_net_mbuf_freem(m);
            while (nx_net_tcp_read(c, sink, sizeof(sink), &got) == NX_NET_OK && got) {}
            struct nx_net_tcp *child;
            while (nx_net_tcp_accept(l, NULL, NULL, &child) == NX_NET_OK) {
                if (below(2)) nx_net_tcp_write(child, sink, below(3000), &got);
                nx_net_tcp_close(child);
            }
            if (c->state == NX_NET_TCP_ESTABLISHED) nx_net_tcp_write(c, sink, below(2000), &got);
        }
    }
    struct nx_net_ip_stats ip = nx_net_ip_stats();
    struct nx_net_tcp_stats tcp = nx_net_tcp_stats();
    printf("  fuzz: %llu IP received, %llu bad checksums, %llu bad lengths, %llu fragments (%llu overlaps), "
           "%llu TCP segments (%llu resets sent), %llu frames sent\n",
           (unsigned long long)ip.received, (unsigned long long)ip.bad_checksum, (unsigned long long)ip.bad_length,
           (unsigned long long)ip.fragments, (unsigned long long)ip.frag_overlaps, (unsigned long long)tcp.received,
           (unsigned long long)tcp.resets_sent, (unsigned long long)transmitted);
    assert(ip.bad_checksum && ip.bad_length && ip.fragments && tcp.received && nx_net_udp_stats().received);
    /* Close everything and let every protocol timer run out: no flushes. */
    nx_net_udp_close(u);
    nx_net_raw_close(r);
    nx_net_tcp_close(l);
    nx_net_tcp_close(c);
    advance(3ull * 3600 * 1000, 500);
    baseline("timer expiry");
    stack_down();
}

/* Phase 2: a pool far too small for the traffic. */
static void exhaustion(void)
{
    stack_up(24, 48, 12);
    /* A loopback TCP transfer completes despite allocation failures. */
    struct nx_net_tcp *l, *client, *server = NULL;
    assert(nx_net_tcp_open(NULL, NULL, &l) == NX_NET_OK && nx_net_tcp_bind(l, LOCALHOST, 9000, false) == NX_NET_OK);
    assert(nx_net_tcp_listen(l, 1) == NX_NET_OK);
    assert(nx_net_tcp_open(NULL, NULL, &client) == NX_NET_OK && nx_net_tcp_connect(client, LOCALHOST, 9000) == NX_NET_OK);
    static uint8_t out[60000], in[60000];
    for (size_t i = 0; i < sizeof(out); ++i) out[i] = (uint8_t)(i * 13);
    size_t written = 0, received = 0, n;
    /* UDP floods compete for the same pool. */
    struct nx_net_udp *flood, *deaf;
    assert(nx_net_udp_open(NULL, NULL, &flood) == NX_NET_OK && nx_net_udp_open(NULL, NULL, &deaf) == NX_NET_OK);
    assert(nx_net_udp_bind(deaf, LOCALHOST, 9001, false) == NX_NET_OK);
    for (unsigned round = 0; round < 200000 && received < sizeof(out); ++round) {
        if (!server && nx_net_tcp_accept(l, NULL, NULL, &server) == NX_NET_OK) {}
        if (written < sizeof(out) && client->state == NX_NET_TCP_ESTABLISHED &&
            nx_net_tcp_write(client, out + written, sizeof(out) - written, &n) == NX_NET_OK)
            written += n;
        if (server) while (nx_net_tcp_read(server, in + received, sizeof(in) - received, &n) == NX_NET_OK && n) received += n;
        if (round % 4 == 0) {
            struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
            if (m && nx_net_mbuf_append(m, out, 900)) nx_net_udp_send(flood, m, LOCALHOST, 9001);
            else if (m) nx_net_mbuf_freem(m);
        }
        nx_net_loop_service(&lo, 8);
        if (round % 8 == 0) { now += 100; timers(); }
    }
    assert(received == sizeof(out) && !memcmp(in, out, sizeof(out)));
    struct nx_net_tcp_stats tcp = nx_net_tcp_stats();
    printf("  exhaustion: 60000 bytes delivered intact; %llu retransmits, %llu UDP queue drops, %llu loopback drops\n",
           (unsigned long long)tcp.retransmits, (unsigned long long)nx_net_udp_stats().queue_drops,
           (unsigned long long)lo.stats.iqdrops);
    assert(pool.stats.failures > 0 && tcp.retransmits > 0);   /* pressure really happened */
    /* Fragment floods stay within the reassembly bounds. */
    uint8_t body[256], f[1600];
    memset(body, 1, sizeof(body));
    for (uint16_t id = 0; id < 300; ++id) feed(&em, f, ipv4(f, PEER, ME, 17, id, 0x2000, body, 64));
    assert(nx_net_ip_reassembling() <= NX_NET_IP_QUEUES && nx_net_ip_stats().frag_dropped > 0);
    nx_net_tcp_close(server);
    nx_net_tcp_close(client);
    nx_net_tcp_close(l);
    nx_net_udp_close(flood);
    nx_net_udp_close(deaf);
    advance(3ull * 3600 * 1000, 500);
    baseline("exhaustion");
    stack_down();
}

/* Phase 3: tear the stack down while everything is in flight. */
static void teardown(void)
{
    stack_up(256, 1000, 200);
    uint8_t f[1600], body[1000];
    memset(body, 7, sizeof(body));
    /* Packets held for ARP (an absent host), partial reassembly, queued
     * datagrams, raw copies, an unacknowledged stream and loopback queues. */
    struct nx_net_udp *u;
    struct nx_net_raw *r;
    struct nx_net_tcp *t, *l, *client;
    assert(nx_net_udp_open(NULL, NULL, &u) == NX_NET_OK && nx_net_udp_bind(u, 0, 7000, false) == NX_NET_OK);
    assert(nx_net_raw_open(true, NULL, NULL, &r) == NX_NET_OK);
    for (int i = 0; i < 4; ++i) {
        struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
        assert(m && nx_net_mbuf_append(m, body, 500));
        nx_net_udp_send(u, m, IP(10, 0, 2, 77), 9);
    }
    feed(&em, f, ipv4(f, PEER, ME, 17, 42, 0x2000, body, 800));
    uint8_t udp[8 + 100] = {0x10, 0x00, 0x1b, 0x58, 0, 108};
    for (int i = 0; i < 5; ++i) feed(&em, f, ipv4(f, PEER, ME, 17, (uint16_t)(50 + i), 0, udp, sizeof(udp)));
    uint8_t icmp[16] = {0, 0, 0, 0, 1, 2, 0, 1};
    feed(&em, f, ipv4(f, PEER, ME, 1, 60, 0, icmp, sizeof(icmp)));
    assert(nx_net_tcp_open(NULL, NULL, &t) == NX_NET_OK && nx_net_tcp_connect(t, IP(10, 0, 2, 78), 80) == NX_NET_OK);
    assert(nx_net_tcp_open(NULL, NULL, &l) == NX_NET_OK && nx_net_tcp_bind(l, LOCALHOST, 9100, false) == NX_NET_OK &&
           nx_net_tcp_listen(l, 2) == NX_NET_OK);
    assert(nx_net_tcp_open(NULL, NULL, &client) == NX_NET_OK && nx_net_tcp_connect(client, LOCALHOST, 9100) == NX_NET_OK);
    nx_net_loop_service(&lo, 2);
    assert(pool.stats.mbufs > 0 && nx_net_ip_reassembling() == 1 && u->receive.cc > 0 && r->receive.cc > 0);
    printf("  teardown: %zu mbufs and %zu clusters in flight\n", pool.stats.mbufs, pool.stats.clusters);
    /* The order of net_stop: interfaces go first, everything else follows. */
    nx_net_if_detach(&em);
    nx_net_if_detach(&lo);
    nx_net_udp_close(u);
    nx_net_raw_close(r);
    nx_net_tcp_close(t);
    nx_net_tcp_close(client);
    nx_net_tcp_close(l);
    nx_net_tcp_flush();
    nx_net_arp_flush(NULL);
    nx_net_ip_flush();
    baseline("teardown");
    stack_down();
}

int main(void)
{
    /* NET_ROBUST_SEED explores other deterministic sequences. */
    const char *chosen = getenv("NET_ROBUST_SEED");
    if (chosen && strtoull(chosen, NULL, 0)) seed = strtoull(chosen, NULL, 0);
    printf("net robustness, seed %#llx\n", (unsigned long long)seed);
    fuzz();
    exhaustion();
    teardown();
    puts("PASS: whole-stack mutation of ARP/IP/fragment/ICMP/UDP/TCP frames, pool exhaustion with an intact TCP transfer, "
         "fragment bounds, timer expiry and in-flight teardown, all returning to baseline within bounds");
    return 0;
}
