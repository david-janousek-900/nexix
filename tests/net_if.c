#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/if.h"

static size_t live_pages;
static void *page_alloc(void *c) { (void)c; ++live_pages; void *p = aligned_alloc(4096, 4096); assert(p); return p; }
static void page_free(void *c, void *p) { (void)c; --live_pages; free(p); }
static const struct nx_net_page_ops ops = { page_alloc, page_free, NULL };
static struct nx_net_budget budget;
static struct nx_net_mbuf_pool pool;

/* Fake Ethernet driver: records the last frame, can refuse up or transmit. */
static bool up_ok = true, tx_ok = true;
static unsigned ups, downs, sent;
static uint8_t last[2048];
static size_t last_length;
static bool fake_up(struct nx_net_if *i) { (void)i; ++ups; return up_ok; }
static void fake_down(struct nx_net_if *i) { (void)i; ++downs; }
static bool fake_transmit(struct nx_net_if *i, struct nx_net_mbuf *m)
{
    (void)i;
    last_length = nx_net_mbuf_length(m);
    assert(last_length <= sizeof(last) && nx_net_mbuf_copydata(m, 0, last_length, last));
    if (!tx_ok) { nx_net_mbuf_drop(m); return false; }
    nx_net_mbuf_freem(m);
    ++sent;
    return true;
}
static const struct nx_net_if_ops fake = { fake_up, fake_down, fake_transmit };

/* Protocol sinks record what they receive and free it. */
static unsigned ip_count, arp_count;
static struct nx_net_if *ip_ifp;
static uint8_t payload[20000];
static size_t payload_length;
static void ip_input(struct nx_net_if *ifp, struct nx_net_mbuf *m)
{
    ip_ifp = ifp;
    payload_length = nx_net_mbuf_length(m);
    assert((m->flags & NX_NET_M_PKTHDR) && m->pkt_len == payload_length);
    assert(nx_net_mbuf_copydata(m, 0, payload_length, payload));
    nx_net_mbuf_freem(m);
    ++ip_count;
}
static void arp_input(struct nx_net_if *ifp, struct nx_net_mbuf *m) { (void)ifp; nx_net_mbuf_freem(m); ++arp_count; }

static struct nx_net_mbuf *packet(size_t length, uint8_t seed)
{
    static uint8_t bytes[20000];
    for (size_t i = 0; i < length; ++i) bytes[i] = (uint8_t)(seed + i);
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    assert(m && nx_net_mbuf_append(m, bytes, length));
    return m;
}
static struct nx_net_mbuf *frame(const uint8_t dst[6], uint16_t type, size_t payload_bytes)
{
    struct nx_net_mbuf *m = packet(NX_NET_ETHER_HEADER + payload_bytes, 0);
    memcpy(m->data, dst, 6);
    m->data[12] = type >> 8; m->data[13] = type & 0xff;
    return m;
}
static unsigned kicks;
static void kick(void *a) { assert(a == &kicks); ++kicks; }

static const uint8_t mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static const uint8_t peer[6] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};

static void registry(void)
{
    static struct nx_net_if a, b, c, d, e, unnamed;
    nx_net_ether_init(&a, "em0", mac, &fake, NULL);
    nx_net_ether_init(&b, "em0", mac, &fake, NULL);
    nx_net_ether_init(&unnamed, "", mac, &fake, NULL);
    assert(nx_net_if_attach(&a) == NX_NET_OK && nx_net_if_attach(&a) == NX_NET_INVALID);
    assert(nx_net_if_attach(&b) == NX_NET_EXISTS && nx_net_if_attach(&unnamed) == NX_NET_INVALID);
    nx_net_loop_init(&c, "lo0", kick, &kicks);
    nx_net_ether_init(&d, "em1", mac, &fake, NULL);
    nx_net_ether_init(&e, "em2", mac, &fake, NULL);
    assert(nx_net_if_attach(&c) == NX_NET_OK && nx_net_if_attach(&d) == NX_NET_OK);
    nx_net_ether_init(&b, "em3", mac, &fake, NULL);
    assert(nx_net_if_attach(&b) == NX_NET_OK && nx_net_if_attach(&e) == NX_NET_FULL);
    assert(nx_net_if_count() == NX_NET_IF_MAX && nx_net_if_find("lo0") == &c && !nx_net_if_find("lo1"));
    assert(nx_net_if_at(0) == &a && nx_net_if_at(4) == NULL);
    /* Long names are truncated to fit with their terminator. */
    nx_net_ether_init(&e, "abcdefghijk", mac, &fake, NULL);
    assert(!strcmp(e.name, "abcdefg"));
    for (size_t i = nx_net_if_count(); i; --i) nx_net_if_detach(nx_net_if_at(0));
    assert(nx_net_if_count() == 0 && !a.attached);

    assert(nx_net_proto_register(NX_NET_ETHERTYPE_IP, NULL) == NX_NET_INVALID);
    assert(nx_net_proto_register(NX_NET_ETHERTYPE_IP, ip_input) == NX_NET_OK);
    assert(nx_net_proto_register(NX_NET_ETHERTYPE_IP, arp_input) == NX_NET_EXISTS);
    assert(nx_net_proto_register(NX_NET_ETHERTYPE_ARP, arp_input) == NX_NET_OK);
    assert(nx_net_proto_register(1, arp_input) == NX_NET_OK && nx_net_proto_register(2, arp_input) == NX_NET_OK);
    assert(nx_net_proto_register(3, arp_input) == NX_NET_FULL);
    nx_net_proto_unregister(1); nx_net_proto_unregister(2);
}

static void ethernet(void)
{
    static struct nx_net_if em;
    nx_net_ether_init(&em, "em0", mac, &fake, NULL);
    assert(nx_net_if_attach(&em) == NX_NET_OK && !(em.flags & NX_NET_IFF_UP));
    /* Down: output and input are refused and their chains freed. */
    assert(nx_net_if_output(&em, packet(10, 1), peer, NX_NET_ETHERTYPE_IP) == NX_NET_DOWN);
    nx_net_ether_input(&em, frame(mac, NX_NET_ETHERTYPE_IP, 50));
    assert(em.stats.oerrors == 1 && em.stats.iqdrops == 1 && !ip_count);
    up_ok = false;
    assert(nx_net_if_up(&em) == NX_NET_DOWN && !(em.flags & NX_NET_IFF_UP));
    up_ok = true;
    assert(nx_net_if_up(&em) == NX_NET_OK && nx_net_if_up(&em) == NX_NET_OK && ups == 2);
    assert(em.flags == (NX_NET_IFF_UP | NX_NET_IFF_RUNNING | NX_NET_IFF_BROADCAST));

    /* Framing: header fields and payload, at both MTU boundaries. */
    assert(nx_net_if_output(&em, packet(0, 0), peer, NX_NET_ETHERTYPE_ARP) == NX_NET_OK);
    assert(last_length == 14 && !memcmp(last, peer, 6) && !memcmp(last + 6, mac, 6) && last[12] == 8 && last[13] == 6);
    assert(nx_net_if_output(&em, packet(NX_NET_ETHER_MTU, 3), peer, NX_NET_ETHERTYPE_IP) == NX_NET_OK);
    assert(last_length == 1514 && last[12] == 8 && last[13] == 0);
    for (size_t i = 0; i < NX_NET_ETHER_MTU; ++i) assert(last[14 + i] == (uint8_t)(3 + i));
    assert(nx_net_if_output(&em, packet(NX_NET_ETHER_MTU + 1, 0), peer, NX_NET_ETHERTYPE_IP) == NX_NET_TOO_BIG);
    struct nx_net_mbuf *bare = nx_net_mbuf_get(&pool, false);
    assert(nx_net_if_output(&em, bare, peer, NX_NET_ETHERTYPE_IP) == NX_NET_INVALID);
    tx_ok = false;
    assert(nx_net_if_output(&em, packet(20, 0), peer, NX_NET_ETHERTYPE_IP) == NX_NET_FULL);
    tx_ok = true;
    assert(em.stats.opackets == 2 && em.stats.obytes == 14 + 1514 && em.stats.oerrors == 4);

    /* Input bounds, address filtering and demultiplexing. */
    nx_net_ether_input(&em, packet(13, 0));
    nx_net_ether_input(&em, frame(mac, NX_NET_ETHERTYPE_IP, NX_NET_ETHER_MTU + 1));
    struct nx_net_mbuf *no_header = nx_net_mbuf_get(&pool, false);
    assert(nx_net_mbuf_append(no_header, mac, 6));
    nx_net_ether_input(&em, no_header);
    assert(em.stats.ierrors == 3 && !ip_count);
    nx_net_ether_input(&em, frame(peer, NX_NET_ETHERTYPE_IP, 40));            /* not ours */
    assert(em.stats.iqdrops == 2 && !ip_count);
    nx_net_ether_input(&em, frame(mac, NX_NET_ETHERTYPE_IP, 0));
    assert(ip_count == 1 && payload_length == 0 && ip_ifp == &em);
    nx_net_ether_input(&em, frame(mac, NX_NET_ETHERTYPE_IP, NX_NET_ETHER_MTU));
    assert(ip_count == 2 && payload_length == NX_NET_ETHER_MTU && payload[0] == 14);
    nx_net_ether_input(&em, frame(nx_net_ether_broadcast, NX_NET_ETHERTYPE_ARP, 28));
    assert(arp_count == 1);
    nx_net_ether_input(&em, frame(mac, 0x86dd, 40));                         /* IPv6: unregistered */
    assert(em.stats.noproto == 1 && em.stats.ipackets == 4);

    /* A header split across buffers is pulled up before parsing. */
    struct nx_net_mbuf *split = nx_net_mbuf_get(&pool, true);
    uint8_t first[6]; memcpy(first, mac, 6);
    assert(nx_net_mbuf_append(split, first, 6));
    struct nx_net_mbuf *rest = nx_net_mbuf_get(&pool, false);
    uint8_t tail[8 + 20] = {0, 0, 0, 0, 0, 0, 0x08, 0x00};
    assert(nx_net_mbuf_append(rest, tail, sizeof(tail)));
    nx_net_mbuf_cat(split, rest);
    nx_net_ether_input(&em, split);
    assert(ip_count == 3 && payload_length == 20);

    nx_net_if_down(&em);
    assert(downs == 1 && !(em.flags & (NX_NET_IFF_UP | NX_NET_IFF_RUNNING)));
    nx_net_if_detach(&em);
}

static void loopback(void)
{
    static struct nx_net_if lo;
    nx_net_loop_init(&lo, "lo0", kick, &kicks);
    assert(nx_net_if_attach(&lo) == NX_NET_OK && nx_net_if_up(&lo) == NX_NET_OK);
    assert(lo.mtu == NX_NET_LOOP_MTU && (lo.flags & NX_NET_IFF_LOOPBACK));
    unsigned before = ip_count;
    /* Output only queues: nothing is delivered until service, in order. */
    for (unsigned i = 0; i < 3; ++i)
        assert(nx_net_if_output(&lo, packet(100 + i, (uint8_t)i), NULL, NX_NET_ETHERTYPE_IP) == NX_NET_OK);
    assert(kicks == 3 && ip_count == before && lo.queue_length == 3);
    assert(nx_net_loop_service(&lo, 2) == 1 && ip_count == before + 2 && payload_length == 101 && payload[0] == 1);
    assert(nx_net_loop_service(&lo, 10) == 0 && ip_count == before + 3 && payload_length == 102);
    assert(nx_net_if_output(&lo, packet(NX_NET_LOOP_MTU, 9), NULL, NX_NET_ETHERTYPE_IP) == NX_NET_OK);
    assert(nx_net_loop_service(&lo, 1) == 0 && payload_length == NX_NET_LOOP_MTU && payload[NX_NET_LOOP_MTU - 1] == (uint8_t)(9 + NX_NET_LOOP_MTU - 1));
    assert(nx_net_if_output(&lo, packet(NX_NET_LOOP_MTU + 1, 0), NULL, NX_NET_ETHERTYPE_IP) == NX_NET_TOO_BIG);
    assert(nx_net_if_output(&lo, packet(4, 0), NULL, 0x1234) == NX_NET_OK);
    assert(nx_net_loop_service(&lo, 1) == 0 && lo.stats.noproto == 1);
    /* Bounded queue: the 51st packet is dropped; down flushes the rest. */
    for (unsigned i = 0; i < NX_NET_IFQ_MAXLEN; ++i)
        assert(nx_net_if_output(&lo, packet(1, 0), NULL, NX_NET_ETHERTYPE_IP) == NX_NET_OK);
    uint64_t drops = pool.stats.drops;
    assert(nx_net_if_output(&lo, packet(1, 0), NULL, NX_NET_ETHERTYPE_IP) == NX_NET_NO_BUFFERS);
    assert(pool.stats.drops == drops + 1 && lo.queue_length == NX_NET_IFQ_MAXLEN);
    nx_net_if_down(&lo);
    assert(lo.queue_length == 0 && !lo.queue_head && lo.stats.iqdrops == 1 + NX_NET_IFQ_MAXLEN);
    assert(nx_net_if_output(&lo, packet(1, 0), NULL, NX_NET_ETHERTYPE_IP) == NX_NET_DOWN);
    /* Detach of an up interface with queued packets frees them too. */
    assert(nx_net_if_up(&lo) == NX_NET_OK && nx_net_if_output(&lo, packet(8, 0), NULL, NX_NET_ETHERTYPE_IP) == NX_NET_OK);
    nx_net_if_detach(&lo);
    assert(!lo.queue_head && nx_net_if_count() == 0);
}

int main(void)
{
    nx_net_budget_init(&budget, 64);
    assert(nx_net_mbuf_pool_init(&pool, &ops, &budget, (struct nx_net_mbuf_limits){ 400, 40 }));
    registry();
    ethernet();
    loopback();
    /* Every chain was consumed exactly once. */
    assert(pool.stats.mbufs == 0 && pool.stats.clusters == 0 && nx_net_mbuf_pool_destroy(&pool) && !live_pages);
    puts("PASS: interface registry/state, Ethernet framing/bounds/filtering/demux, loopback deferral/limits and mbuf ownership");
    return 0;
}
