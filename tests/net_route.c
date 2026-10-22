#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/arp.h"
#include "net/route.h"
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
static const nx_net_addr_t mask24 = IP(255, 255, 255, 0);

/* Fake Ethernet driver: records every frame it is given. */
#define FRAMES 64
static uint8_t frames[FRAMES][128];
static size_t frame_lengths[FRAMES], sent;
static bool fake_transmit(struct nx_net_if *i, struct nx_net_mbuf *m)
{
    (void)i;
    size_t length = nx_net_mbuf_length(m);
    assert(sent < FRAMES && length <= sizeof(frames[0]) && nx_net_mbuf_copydata(m, 0, length, frames[sent]));
    frame_lengths[sent++] = length;
    nx_net_mbuf_freem(m);
    return true;
}
static const struct nx_net_if_ops fake = { NULL, NULL, fake_transmit };
static const uint8_t mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static const uint8_t mac1[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x57};
static const uint8_t gw_mac[6] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};
static struct nx_net_if lo, em, em1;

static unsigned ip_count;
static uint8_t ip_first;
static void ip_input(struct nx_net_if *ifp, struct nx_net_mbuf *m)
{
    assert(ifp == &lo);
    assert(nx_net_mbuf_copydata(m, 0, 1, &ip_first));
    nx_net_mbuf_freem(m);
    ++ip_count;
}
static unsigned kicks;
static void kick(void *a) { (void)a; ++kicks; }

static struct nx_net_mbuf *packet(uint8_t tag)
{
    uint8_t bytes[20] = {tag};
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    assert(m && nx_net_mbuf_append(m, bytes, sizeof(bytes)));
    return m;
}
static void check_request(size_t index, const uint8_t *from, nx_net_addr_t spa, nx_net_addr_t tpa)
{
    const uint8_t *f = frames[index];
    assert(frame_lengths[index] == 14 + 28 && !memcmp(f, nx_net_ether_broadcast, 6) && !memcmp(f + 6, from, 6));
    assert(nx_net_get16(f + 12) == NX_NET_ETHERTYPE_ARP);
    const uint8_t *a = f + 14;
    static const uint8_t zero[6];
    assert(nx_net_get16(a) == 1 && nx_net_get16(a + 2) == 0x0800 && a[4] == 6 && a[5] == 4 && nx_net_get16(a + 6) == 1);
    assert(!memcmp(a + 8, from, 6) && nx_net_get32(a + 14) == spa && !memcmp(a + 18, zero, 6) && nx_net_get32(a + 24) == tpa);
}
/* An ARP message from a peer, padded to the Ethernet minimum like real frames. */
static struct nx_net_mbuf *arp_message(unsigned op, const uint8_t *sha, nx_net_addr_t spa, nx_net_addr_t tpa)
{
    uint8_t a[46] = {0};
    nx_net_put16(a, 1); nx_net_put16(a + 2, 0x0800); a[4] = 6; a[5] = 4; nx_net_put16(a + 6, (uint16_t)op);
    memcpy(a + 8, sha, 6); nx_net_put32(a + 14, spa); nx_net_put32(a + 24, tpa);
    if (op == 2) memcpy(a + 18, mac, 6);
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    assert(m && nx_net_mbuf_append(m, a, sizeof(a)));
    return m;
}
static bool sent_ip(size_t index, const uint8_t *to, uint8_t tag)
{
    const uint8_t *f = frames[index];
    return frame_lengths[index] == 14 + 20 && !memcmp(f, to, 6) && nx_net_get16(f + 12) == NX_NET_ETHERTYPE_IP && f[14] == tag;
}

static void configuration(void)
{
    assert(nx_net_inet_mask_valid(UINT32_MAX) && nx_net_inet_mask_valid(IP(255, 0, 0, 0)));
    assert(!nx_net_inet_mask_valid(0) && !nx_net_inet_mask_valid(IP(255, 0, 255, 0)) && !nx_net_inet_mask_valid(1));
    assert(nx_net_inet_unicast(IP(10, 0, 2, 15)) && !nx_net_inet_unicast(0) && !nx_net_inet_unicast(IP(0, 1, 2, 3)));
    assert(!nx_net_inet_unicast(IP(224, 0, 0, 1)) && !nx_net_inet_unicast(IP(240, 0, 0, 1)) && !nx_net_inet_unicast(UINT32_MAX));

    static struct nx_net_if detached;
    nx_net_ether_init(&detached, "em9", mac, &fake, NULL);
    assert(nx_net_inet_set(&detached, IP(10, 0, 2, 15), mask24) == NX_NET_INVALID);
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 15), IP(255, 0, 255, 0)) == NX_NET_INVALID);
    assert(nx_net_inet_set(&em, IP(224, 0, 0, 1), mask24) == NX_NET_INVALID);
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 0), mask24) == NX_NET_INVALID);     /* network */
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 255), mask24) == NX_NET_INVALID);   /* broadcast */
    assert(nx_net_inet_set(&em, IP(127, 0, 0, 2), IP(255, 0, 0, 0)) == NX_NET_INVALID);
    assert(nx_net_inet_set(&lo, IP(10, 9, 9, 9), mask24) == NX_NET_INVALID);
    assert(!em.inet_configured && !lo.inet_configured);
    /* /31 and /32 have no network or broadcast address to refuse. */
    assert(nx_net_inet_set(&em1, IP(192, 168, 7, 0), IP(255, 255, 255, 254)) == NX_NET_OK);
    assert(nx_net_inet_set(&em1, IP(192, 168, 7, 255), UINT32_MAX) == NX_NET_OK);
    assert(nx_net_inet_set(&lo, IP(127, 0, 0, 1), IP(255, 0, 0, 0)) == NX_NET_OK);
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 15), mask24) == NX_NET_OK);
    /* Overlap either way is refused, and the old address stays. */
    assert(nx_net_inet_set(&em1, IP(10, 0, 2, 16), IP(255, 255, 255, 128)) == NX_NET_EXISTS);
    assert(nx_net_inet_set(&em1, IP(10, 1, 1, 1), IP(255, 0, 0, 0)) == NX_NET_EXISTS);
    assert(em1.inet_addr == IP(192, 168, 7, 255) && em1.inet_mask == UINT32_MAX);
    assert(nx_net_inet_set(&em1, IP(192, 168, 1, 10), mask24) == NX_NET_OK);
    /* Reconfiguring the same interface does not overlap with itself. */
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 15), IP(255, 255, 0, 0)) == NX_NET_OK);
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 15), mask24) == NX_NET_OK);

    nx_net_addr_t gw;
    assert(!nx_net_route_get_default(&gw));
    assert(nx_net_route_set_default(0) == NX_NET_INVALID && nx_net_route_set_default(IP(224, 0, 0, 9)) == NX_NET_INVALID);
    assert(nx_net_route_set_default(IP(10, 0, 2, 15)) == NX_NET_INVALID);        /* our own */
    assert(nx_net_route_set_default(IP(172, 16, 0, 1)) == NX_NET_UNREACHABLE);   /* off-link */
    assert(nx_net_route_set_default(IP(127, 0, 0, 9)) == NX_NET_UNREACHABLE);    /* loopback */
    assert(!nx_net_route_get_default(&gw));
    assert(nx_net_route_set_default(IP(10, 0, 2, 2)) == NX_NET_OK && nx_net_route_get_default(&gw) && gw == IP(10, 0, 2, 2));
}

static void lookup(void)
{
    struct nx_net_route r;
    assert(nx_net_route_lookup(IP(10, 0, 2, 7), &r) == NX_NET_OK && r.ifp == &em && r.next_hop == IP(10, 0, 2, 7));
    assert(r.source == IP(10, 0, 2, 15) && !r.gateway && !r.local && !r.broadcast);
    assert(nx_net_route_lookup(IP(192, 168, 1, 99), &r) == NX_NET_OK && r.ifp == &em1);
    assert(nx_net_route_lookup(IP(8, 8, 8, 8), &r) == NX_NET_OK && r.ifp == &em && r.next_hop == IP(10, 0, 2, 2) && r.gateway);
    assert(nx_net_route_lookup(IP(10, 0, 2, 255), &r) == NX_NET_OK && r.broadcast && r.ifp == &em);
    assert(nx_net_route_lookup(IP(10, 0, 2, 15), &r) == NX_NET_OK && r.local && r.ifp == &lo && r.source == IP(10, 0, 2, 15));
    assert(nx_net_route_lookup(IP(127, 0, 0, 1), &r) == NX_NET_OK && r.local && r.ifp == &lo);
    assert(nx_net_route_lookup(IP(127, 0, 0, 5), &r) == NX_NET_OK && !r.local && r.ifp == &lo);
    assert(nx_net_route_lookup(0, &r) == NX_NET_INVALID && nx_net_route_lookup(IP(224, 0, 0, 1), &r) == NX_NET_INVALID);
    assert(nx_net_route_lookup(UINT32_MAX, &r) == NX_NET_UNREACHABLE && !r.ifp);

    /* Longest prefix: a /25 on em1 beats em's /16 for its half only. */
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 15), IP(255, 255, 0, 0)) == NX_NET_OK);
    assert(nx_net_inet_set(&em1, IP(10, 0, 9, 1), IP(255, 255, 255, 128)) == NX_NET_EXISTS);
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 15), mask24) == NX_NET_OK);

    /* A down interface routes nothing; its gateway goes with it. */
    nx_net_if_down(&em);
    assert(nx_net_route_lookup(IP(10, 0, 2, 7), &r) == NX_NET_UNREACHABLE);
    assert(nx_net_route_lookup(IP(8, 8, 8, 8), &r) == NX_NET_UNREACHABLE);
    assert(nx_net_route_lookup(IP(10, 0, 2, 15), &r) == NX_NET_UNREACHABLE);    /* not local while down */
    assert(nx_net_if_up(&em) == NX_NET_OK && nx_net_route_lookup(IP(8, 8, 8, 8), &r) == NX_NET_OK);
    /* Without a usable default route, off-link is unreachable. */
    nx_net_route_clear_default();
    assert(nx_net_route_lookup(IP(8, 8, 8, 8), &r) == NX_NET_UNREACHABLE);
    assert(nx_net_route_set_default(IP(10, 0, 2, 2)) == NX_NET_OK);
    nx_net_inet_clear(&em);   /* the gateway is no longer on-link */
    assert(nx_net_route_lookup(IP(8, 8, 8, 8), &r) == NX_NET_UNREACHABLE);
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 15), mask24) == NX_NET_OK);
    assert(nx_net_route_lookup(IP(8, 8, 8, 8), &r) == NX_NET_OK);
}

static void delivery(void)
{
    /* Local and loopback destinations are queued on lo0, never sent out. */
    size_t before = sent;
    assert(nx_net_route_output(packet(1), IP(10, 0, 2, 15)) == NX_NET_OK);
    assert(nx_net_route_output(packet(2), IP(127, 0, 0, 1)) == NX_NET_OK);
    assert(kicks == 2 && nx_net_loop_service(&lo, 10) == 0 && ip_count == 2 && ip_first == 2 && sent == before);
    /* Directed broadcast needs no resolution. */
    assert(nx_net_route_output(packet(3), IP(10, 0, 2, 255)) == NX_NET_OK && sent_ip(sent - 1, nx_net_ether_broadcast, 3));
    assert(nx_net_route_output(packet(4), UINT32_MAX) == NX_NET_UNREACHABLE);

    /* Gateway delivery: one request, packets held in order until the reply. */
    sent = 0;
    assert(nx_net_route_output(packet(10), IP(8, 8, 8, 8)) == NX_NET_OK);
    assert(nx_net_route_output(packet(11), IP(1, 1, 1, 1)) == NX_NET_OK);
    assert(sent == 1);
    check_request(0, mac, IP(10, 0, 2, 15), IP(10, 0, 2, 2));
    uint8_t found[6];
    assert(!nx_net_arp_lookup(&em, IP(10, 0, 2, 2), found));
    nx_net_arp_input(&em, arp_message(2, gw_mac, IP(10, 0, 2, 2), IP(10, 0, 2, 15)));
    assert(sent == 3 && sent_ip(1, gw_mac, 10) && sent_ip(2, gw_mac, 11));
    assert(nx_net_arp_lookup(&em, IP(10, 0, 2, 2), found) && !memcmp(found, gw_mac, 6));
    /* Cached: sent directly. */
    assert(nx_net_route_output(packet(12), IP(9, 9, 9, 9)) == NX_NET_OK && sent == 4 && sent_ip(3, gw_mac, 12));

    /* On-link peer with a per-entry hold limit: the oldest are dropped. */
    sent = 0;
    uint64_t drops = nx_net_arp_stats().hold_drops;
    for (uint8_t i = 0; i < NX_NET_ARP_HOLD + 2; ++i)
        assert(nx_net_route_output(packet(20 + i), IP(10, 0, 2, 3)) == NX_NET_OK);
    assert(sent == 1 && nx_net_arp_stats().hold_drops == drops + 2);
    static const uint8_t peer[6] = {0x02, 0, 0, 0, 0, 3};
    nx_net_arp_input(&em, arp_message(2, peer, IP(10, 0, 2, 3), IP(10, 0, 2, 15)));
    assert(sent == 1 + NX_NET_ARP_HOLD);
    for (unsigned i = 0; i < NX_NET_ARP_HOLD; ++i) assert(sent_ip(1 + i, peer, (uint8_t)(22 + i)));

    /* Retries every second, then a negative entry for arpt_down. */
    sent = 0;
    assert(nx_net_route_output(packet(30), IP(10, 0, 2, 77)) == NX_NET_OK);
    for (unsigned t = 1; t < NX_NET_ARP_MAXTRIES; ++t) {
        now += NX_NET_ARP_RETRY_MS - 1;
        nx_net_arp_timer();
        assert(sent == t);
        now += 1;
        nx_net_arp_timer();
        assert(sent == t + 1);
        check_request(t, mac, IP(10, 0, 2, 15), IP(10, 0, 2, 77));
    }
    drops = nx_net_arp_stats().hold_drops;
    now += NX_NET_ARP_RETRY_MS;
    nx_net_arp_timer();
    assert(sent == NX_NET_ARP_MAXTRIES && nx_net_arp_stats().timeouts == 1 && nx_net_arp_stats().hold_drops == drops + 1);
    assert(nx_net_route_output(packet(31), IP(10, 0, 2, 77)) == NX_NET_HOST_UNREACHABLE && sent == NX_NET_ARP_MAXTRIES);
    now += NX_NET_ARP_DOWN_MS;
    nx_net_arp_timer();
    assert(nx_net_route_output(packet(32), IP(10, 0, 2, 77)) == NX_NET_OK && sent == NX_NET_ARP_MAXTRIES + 1);

    /* Resolved entries expire after arpt_keep and are requested again. The
     * gateway was learned before the retries above; refresh it first. */
    nx_net_arp_input(&em, arp_message(2, gw_mac, IP(10, 0, 2, 2), IP(10, 0, 2, 15)));
    now += NX_NET_ARP_KEEP_MS - 1;
    assert(nx_net_arp_lookup(&em, IP(10, 0, 2, 2), found));
    now += 1;
    assert(!nx_net_arp_lookup(&em, IP(10, 0, 2, 2), found));
    sent = 0;
    assert(nx_net_route_output(packet(33), IP(8, 8, 8, 8)) == NX_NET_OK && sent == 1);
    check_request(0, mac, IP(10, 0, 2, 15), IP(10, 0, 2, 2));
    nx_net_arp_flush(NULL);
}

static void input(void)
{
    uint64_t invalid = nx_net_arp_stats().invalid;
    /* Malformed: short, wrong hardware/protocol/lengths/operation, group sender. */
    struct nx_net_mbuf *m = arp_message(1, gw_mac, IP(10, 0, 2, 2), IP(10, 0, 2, 15));
    assert(nx_net_mbuf_adj_tail(m, nx_net_mbuf_length(m) - 27));
    nx_net_arp_input(&em, m);
    static const unsigned offsets[] = {1, 3, 4, 5, 7};
    for (unsigned i = 0; i < 5; ++i) {
        m = arp_message(1, gw_mac, IP(10, 0, 2, 2), IP(10, 0, 2, 15));
        m->data[offsets[i]] ^= 0x10;
        nx_net_arp_input(&em, m);
    }
    nx_net_arp_input(&em, arp_message(1, nx_net_ether_broadcast, IP(10, 0, 2, 2), IP(10, 0, 2, 15)));
    static const uint8_t group[6] = {0x01, 0, 0x5e, 0, 0, 1};
    nx_net_arp_input(&em, arp_message(2, group, IP(10, 0, 2, 2), IP(10, 0, 2, 15)));
    assert(nx_net_arp_stats().invalid == invalid + 8);

    /* A request for us is answered in place, without padding, and teaches us the sender. */
    sent = 0;
    uint8_t found[6];
    nx_net_arp_input(&em, arp_message(1, gw_mac, IP(10, 0, 2, 2), IP(10, 0, 2, 15)));
    assert(sent == 1 && frame_lengths[0] == 42 && !memcmp(frames[0], gw_mac, 6) && !memcmp(frames[0] + 6, mac, 6));
    const uint8_t *a = frames[0] + 14;
    assert(nx_net_get16(a + 6) == 2 && !memcmp(a + 8, mac, 6) && nx_net_get32(a + 14) == IP(10, 0, 2, 15));
    assert(!memcmp(a + 18, gw_mac, 6) && nx_net_get32(a + 24) == IP(10, 0, 2, 2) && nx_net_arp_stats().replies == 1);
    assert(nx_net_arp_lookup(&em, IP(10, 0, 2, 2), found) && !memcmp(found, gw_mac, 6));
    /* A cached sender is updated by any message, even one for another host. */
    static const uint8_t moved[6] = {0x02, 0, 0, 0, 0, 0x22};
    nx_net_arp_input(&em, arp_message(1, moved, IP(10, 0, 2, 2), IP(10, 0, 2, 50)));
    assert(sent == 1 && nx_net_arp_lookup(&em, IP(10, 0, 2, 2), found) && !memcmp(found, moved, 6));
    /* Uncached senders are learned only when they address us, and only on-link. */
    static const uint8_t other[6] = {0x02, 0, 0, 0, 0, 0x33};
    nx_net_arp_input(&em, arp_message(1, other, IP(10, 0, 2, 40), IP(10, 0, 2, 50)));
    assert(!nx_net_arp_lookup(&em, IP(10, 0, 2, 40), found));
    nx_net_arp_input(&em, arp_message(2, other, IP(10, 0, 3, 40), IP(10, 0, 2, 15)));
    assert(!nx_net_arp_lookup(&em, IP(10, 0, 3, 40), found) && sent == 1);
    nx_net_arp_input(&em, arp_message(2, other, IP(10, 0, 2, 255), IP(10, 0, 2, 15)));
    assert(!nx_net_arp_lookup(&em, IP(10, 0, 2, 255), found));
    /* A probe (sender 0.0.0.0) is answered but not learned. */
    nx_net_arp_input(&em, arp_message(1, other, 0, IP(10, 0, 2, 15)));
    assert(sent == 2 && nx_net_get32(frames[1] + 14 + 24) == 0);
    /* Conflicts, our own echoes and unconfigured interfaces are ignored. */
    nx_net_arp_input(&em, arp_message(1, other, IP(10, 0, 2, 15), IP(10, 0, 2, 15)));
    nx_net_arp_input(&em, arp_message(1, mac, IP(10, 0, 2, 9), IP(10, 0, 2, 15)));
    nx_net_inet_clear(&em1);
    nx_net_arp_input(&em1, arp_message(1, other, IP(10, 0, 2, 9), IP(10, 0, 2, 15)));
    assert(sent == 2 && nx_net_arp_stats().conflicts == 1 && nx_net_arp_stats().ignored == 2);

    /* Address change and interface down discard entries for that interface. */
    assert(nx_net_arp_lookup(&em, IP(10, 0, 2, 2), found));
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 16), mask24) == NX_NET_OK && !nx_net_arp_lookup(&em, IP(10, 0, 2, 2), found));
    assert(nx_net_inet_set(&em, IP(10, 0, 2, 15), mask24) == NX_NET_OK);
    assert(nx_net_route_output(packet(40), IP(10, 0, 2, 3)) == NX_NET_OK);
    uint64_t drops = nx_net_arp_stats().hold_drops;
    nx_net_if_down(&em);
    assert(nx_net_arp_stats().hold_drops == drops + 1);
    assert(nx_net_arp_output(&em, IP(10, 0, 2, 3), packet(41)) == NX_NET_DOWN);
    assert(nx_net_if_up(&em) == NX_NET_OK);
    /* Direct ARP misuse: loopback, off-link or own next hop. */
    assert(nx_net_arp_output(&lo, IP(127, 0, 0, 1), packet(42)) == NX_NET_INVALID);
    assert(nx_net_arp_output(&em, IP(10, 0, 3, 1), packet(43)) == NX_NET_INVALID);
    assert(nx_net_arp_output(&em, IP(10, 0, 2, 15), packet(44)) == NX_NET_INVALID);
}

static void bounds(void)
{
    /* Global hold bound: one packet per unresolved entry until 16 wait. */
    sent = 0;
    for (unsigned i = 0; i < NX_NET_ARP_ENTRIES; ++i)
        assert(nx_net_route_output(packet((uint8_t)i), IP(10, 0, 2, 100 + i)) == NX_NET_OK);
    assert(sent == NX_NET_ARP_ENTRIES);
    /* Every slot is unresolved: nothing is evicted, new peers are refused. */
    assert(nx_net_route_output(packet(0), IP(10, 0, 2, 200)) == NX_NET_NO_BUFFERS);
    assert(nx_net_arp_stats().table_full == 1);
    /* At the global bound, an entry trades its oldest packet for the newest. */
    uint64_t drops = nx_net_arp_stats().hold_drops;
    assert(nx_net_route_output(packet(99), IP(10, 0, 2, 100)) == NX_NET_OK && nx_net_arp_stats().hold_drops == drops + 1);
    nx_net_arp_flush(NULL);

    /* Resolved entries are evicted least recently used first. */
    for (unsigned i = 0; i < NX_NET_ARP_ENTRIES; ++i) {
        uint8_t peer[6] = {0x02, 0, 0, 0, 1, (uint8_t)i};
        now += 1;
        nx_net_arp_input(&em, arp_message(1, peer, IP(10, 0, 2, 100 + i), IP(10, 0, 2, 15)));
    }
    uint8_t found[6];
    now += 1;
    assert(nx_net_route_output(packet(1), IP(10, 0, 2, 100)) == NX_NET_OK);   /* refresh .100 */
    assert(nx_net_route_output(packet(2), IP(10, 0, 2, 200)) == NX_NET_OK);
    assert(nx_net_arp_stats().evictions == 1 && nx_net_arp_lookup(&em, IP(10, 0, 2, 100), found));
    assert(!nx_net_arp_lookup(&em, IP(10, 0, 2, 101), found));

    /* Request allocation failure: the packet waits and a retry asks again. */
    nx_net_arp_flush(NULL);
    struct nx_net_mbuf *held = packet(5), *all = NULL;
    for (struct nx_net_mbuf *m; (m = nx_net_mbuf_get(&pool, false)); ) { m->next = all; all = m; }
    sent = 0;
    assert(nx_net_route_output(held, IP(10, 0, 2, 3)) == NX_NET_OK && sent == 0);
    nx_net_mbuf_freem(all);
    now += NX_NET_ARP_RETRY_MS;
    nx_net_arp_timer();
    assert(sent == 1);
    check_request(0, mac, IP(10, 0, 2, 15), IP(10, 0, 2, 3));
    nx_net_arp_flush(NULL);
}

int main(void)
{
    nx_net_budget_init(&budget, 64);
    assert(nx_net_mbuf_pool_init(&pool, &ops, &budget, (struct nx_net_mbuf_limits){ 400, 40 }));
    nx_net_arp_init(&pool, clock_ms);
    nx_net_if_set_change_hook(nx_net_arp_if_changed);
    assert(nx_net_proto_register(NX_NET_ETHERTYPE_IP, ip_input) == NX_NET_OK);
    nx_net_loop_init(&lo, "lo0", kick, NULL);
    nx_net_ether_init(&em, "em0", mac, &fake, NULL);
    nx_net_ether_init(&em1, "em1", mac1, &fake, NULL);
    assert(nx_net_if_attach(&lo) == NX_NET_OK && nx_net_if_attach(&em) == NX_NET_OK && nx_net_if_attach(&em1) == NX_NET_OK);
    assert(nx_net_if_up(&lo) == NX_NET_OK && nx_net_if_up(&em) == NX_NET_OK && nx_net_if_up(&em1) == NX_NET_OK);
    configuration();
    lookup();
    delivery();
    input();
    bounds();
    /* Detach clears the address; the default gateway then no longer routes. */
    nx_net_if_detach(&em);
    struct nx_net_route r;
    assert(!em.inet_configured && nx_net_route_lookup(IP(8, 8, 8, 8), &r) == NX_NET_UNREACHABLE);
    nx_net_if_detach(&em1);
    nx_net_if_detach(&lo);
    nx_net_route_clear_default();
    nx_net_arp_flush(NULL);
    assert(pool.stats.mbufs == 0 && pool.stats.clusters == 0 && nx_net_mbuf_pool_destroy(&pool) && !live_pages);
    puts("PASS: IPv4 address validation, connected/default routes, ARP resolution/holding/retries/expiry/replies and mbuf ownership");
    return 0;
}
