#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/arp.h"
#include "net/config.h"
#include <nexix/errno.h>

static size_t live_pages;
static void *page_alloc(void *c) { (void)c; ++live_pages; void *p = aligned_alloc(4096, 4096); assert(p); return p; }
static void page_free(void *c, void *p) { (void)c; --live_pages; free(p); }
static const struct nx_net_page_ops ops = { page_alloc, page_free, NULL };
static struct nx_net_budget budget;
static struct nx_net_mbuf_pool pool;
static uint64_t now;
static uint64_t clock_ms(void) { return now; }
#define IP(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (d))
#define BE(a, b, c, d) __builtin_bswap32(IP(a, b, c, d))

/* A fake Ethernet driver whose up can be made to fail. */
static bool fail_up;
static bool fake_up(struct nx_net_if *i) { (void)i; return !fail_up; }
static void fake_down(struct nx_net_if *i) { (void)i; }
static bool fake_transmit(struct nx_net_if *i, struct nx_net_mbuf *m) { (void)i; nx_net_mbuf_freem(m); return true; }
static const struct nx_net_if_ops fake = { fake_up, fake_down, fake_transmit };
static const uint8_t mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static struct nx_net_if lo, em, em1;
static void kick(void *a) { (void)a; }
static void changed(struct nx_net_if *ifp) { nx_net_arp_if_changed(ifp); }

static struct nx_ifconfig config(const char *name, uint32_t change, uint32_t addr, uint32_t mask)
{
    struct nx_ifconfig c = { .change = change, .addr = addr, .mask = mask };
    memcpy(c.name, name, strlen(name) + 1);
    return c;
}
static int apply(const char *name, uint32_t change, uint32_t addr, uint32_t mask)
{
    struct nx_ifconfig c = config(name, change, addr, mask);
    return nx_net_config_apply(&c);
}
static struct nx_ifinfo info(const char *name)
{
    struct nx_ifinfo r = {0};
    memcpy(r.name, name, strlen(name) + 1);
    assert(nx_net_config_ifinfo(&r) == 0);
    return r;
}

int main(void)
{
    nx_net_budget_init(&budget, 64);
    assert(nx_net_mbuf_pool_init(&pool, &ops, &budget, (struct nx_net_mbuf_limits){ 100, 20 }));
    nx_net_arp_init(&pool, clock_ms);
    nx_net_if_set_change_hook(changed);
    nx_net_loop_init(&lo, "lo0", kick, NULL);
    nx_net_ether_init(&em, "em0", mac, &fake, NULL);
    nx_net_ether_init(&em1, "em1", mac, &fake, NULL);
    assert(nx_net_if_attach(&lo) == NX_NET_OK && nx_net_if_attach(&em) == NX_NET_OK && nx_net_if_attach(&em1) == NX_NET_OK);

    /* Queries by index and by name. */
    struct nx_ifinfo r = {0};
    assert(nx_net_config_ifinfo(&r) == 0 && !strcmp(r.name, "lo0") && r.index == 0 && r.mtu == 16384);
    assert((r.flags & NX_IFF_LOOPBACK) && !(r.flags & NX_IFF_UP) && !r.addressed);
    r = (struct nx_ifinfo){ .index = 1 };
    assert(nx_net_config_ifinfo(&r) == 0 && !strcmp(r.name, "em0") && r.mtu == 1500 && !memcmp(r.lladdr, mac, 6));
    r = (struct nx_ifinfo){ .index = 3 };
    assert(nx_net_config_ifinfo(&r) == -NX_ENXIO);
    r = (struct nx_ifinfo){ .name = "eth9" };
    assert(nx_net_config_ifinfo(&r) == -NX_ENXIO);
    memcpy(r.name, "longname", 8);   /* no terminator */
    assert(nx_net_config_ifinfo(&r) == -NX_EINVAL);
    r = info("em1");
    assert(r.index == 2);

    /* Malformed changes touch nothing. */
    assert(apply("em0", 0, 0, 0) == -NX_EINVAL);
    assert(apply("em0", 0x10, 0, 0) == -NX_EINVAL);
    assert(apply("em0", NX_IFC_ADDRESS | NX_IFC_CLEAR, BE(10, 0, 2, 15), BE(255, 255, 255, 0)) == -NX_EINVAL);
    assert(apply("em0", NX_IFC_UP | NX_IFC_DOWN, 0, 0) == -NX_EINVAL);
    struct nx_ifconfig bad = config("em0", NX_IFC_UP, 0, 0);
    bad.reserved = 1;
    assert(nx_net_config_apply(&bad) == -NX_EINVAL);
    assert(apply("nope", NX_IFC_UP, 0, 0) == -NX_ENXIO);
    assert(apply("em0", NX_IFC_ADDRESS, BE(10, 0, 2, 15), BE(255, 0, 255, 0)) == -NX_EINVAL);     /* noncontiguous */
    assert(apply("em0", NX_IFC_ADDRESS, BE(224, 0, 0, 1), BE(255, 255, 255, 0)) == -NX_EINVAL);   /* multicast */
    assert(apply("em0", NX_IFC_ADDRESS, BE(10, 0, 2, 255), BE(255, 255, 255, 0)) == -NX_EINVAL);  /* broadcast */
    assert(apply("em0", NX_IFC_ADDRESS, BE(127, 0, 0, 2), BE(255, 0, 0, 0)) == -NX_EINVAL);       /* loopback net */
    assert(!info("em0").addressed && !(info("em0").flags & NX_IFF_UP));

    /* Address and up together; the broadcast address is derived. */
    assert(apply("lo0", NX_IFC_ADDRESS | NX_IFC_UP, BE(127, 0, 0, 1), BE(255, 0, 0, 0)) == 0);
    assert(apply("em0", NX_IFC_ADDRESS | NX_IFC_UP, BE(10, 0, 2, 15), BE(255, 255, 255, 0)) == 0);
    assert(info("lo0").addressed && !info("lo0").broadcast);   /* loopback has no broadcast address */
    r = info("em0");
    assert(r.addressed && r.addr == BE(10, 0, 2, 15) && r.mask == BE(255, 255, 255, 0) && r.broadcast == BE(10, 0, 2, 255));
    assert((r.flags & (NX_IFF_UP | NX_IFF_RUNNING | NX_IFF_BROADCAST)) == (NX_IFF_UP | NX_IFF_RUNNING | NX_IFF_BROADCAST));
    /* An overlapping subnet elsewhere is EEXIST. */
    assert(apply("em1", NX_IFC_ADDRESS, BE(10, 0, 2, 16), BE(255, 255, 0, 0)) == -NX_EEXIST);
    assert(!info("em1").addressed);

    /* Rollback: when up fails, the previous address (or none) comes back. */
    fail_up = true;
    assert(apply("em1", NX_IFC_ADDRESS | NX_IFC_UP, BE(192, 168, 7, 1), BE(255, 255, 255, 0)) == -NX_EIO);
    assert(!info("em1").addressed && !(info("em1").flags & NX_IFF_UP));
    fail_up = false;
    assert(apply("em1", NX_IFC_ADDRESS, BE(192, 168, 7, 1), BE(255, 255, 255, 0)) == 0);
    fail_up = true;
    assert(apply("em1", NX_IFC_ADDRESS | NX_IFC_UP, BE(192, 168, 8, 1), BE(255, 255, 255, 0)) == -NX_EIO);
    assert(info("em1").addr == BE(192, 168, 7, 1));
    assert(apply("em1", NX_IFC_CLEAR | NX_IFC_UP, 0, 0) == -NX_EIO && info("em1").addressed);
    fail_up = false;
    /* A /31 has no broadcast address. */
    assert(apply("em1", NX_IFC_ADDRESS, BE(192, 168, 9, 0), BE(255, 255, 255, 254)) == 0 && !info("em1").broadcast);

    /* Default route: must be on-link and not ours; 0 clears it. */
    struct nx_route route = { .gateway = BE(10, 0, 2, 2) }, got;
    assert(nx_net_config_get_route(&got) == 0 && !got.flags && !got.gateway);
    assert(nx_net_config_set_route(&route) == 0);
    assert(nx_net_config_get_route(&got) == 0 && got.flags == NX_RT_DEFAULT && got.gateway == BE(10, 0, 2, 2));
    struct nx_route off_link = { .gateway = BE(172, 16, 0, 1) }, ours = { .gateway = BE(10, 0, 2, 15) };
    struct nx_route multicast = { .gateway = BE(224, 0, 0, 1) }, flagged = { .gateway = BE(10, 0, 2, 2), .flags = 1 };
    assert(nx_net_config_set_route(&off_link) == -NX_ENETUNREACH && nx_net_config_set_route(&ours) == -NX_EINVAL);
    assert(nx_net_config_set_route(&multicast) == -NX_EINVAL && nx_net_config_set_route(&flagged) == -NX_EINVAL);
    assert(nx_net_config_get_route(&got) == 0 && got.gateway == BE(10, 0, 2, 2));   /* unchanged */

    /* Lookups: local, connected, through the gateway, broadcast, refused. */
    struct nx_route_lookup l = { .dst = BE(10, 0, 2, 15) };
    assert(nx_net_config_lookup(&l) == 0 && !strcmp(l.ifname, "lo0") && (l.flags & NX_RT_LOCAL));
    l = (struct nx_route_lookup){ .dst = BE(10, 0, 2, 3) };
    assert(nx_net_config_lookup(&l) == 0 && !strcmp(l.ifname, "em0") && l.next_hop == BE(10, 0, 2, 3) && !l.flags &&
           l.source == BE(10, 0, 2, 15));
    l = (struct nx_route_lookup){ .dst = BE(8, 8, 8, 8) };
    assert(nx_net_config_lookup(&l) == 0 && l.next_hop == BE(10, 0, 2, 2) && l.flags == NX_RT_GATEWAY);
    l = (struct nx_route_lookup){ .dst = BE(10, 0, 2, 255) };
    assert(nx_net_config_lookup(&l) == 0 && (l.flags & NX_RT_BROADCAST));
    l = (struct nx_route_lookup){ .dst = BE(224, 0, 0, 1) };
    assert(nx_net_config_lookup(&l) == -NX_EINVAL);
    route.gateway = 0;
    assert(nx_net_config_set_route(&route) == 0 && nx_net_config_get_route(&got) == 0 && !got.flags);
    l = (struct nx_route_lookup){ .dst = BE(8, 8, 8, 8) };
    assert(nx_net_config_lookup(&l) == -NX_ENETUNREACH);

    /* Down keeps the address but stops routing; clear removes it. */
    assert(apply("em0", NX_IFC_DOWN, 0, 0) == 0 && !(info("em0").flags & NX_IFF_UP) && info("em0").addressed);
    l = (struct nx_route_lookup){ .dst = BE(10, 0, 2, 3) };
    assert(nx_net_config_lookup(&l) == -NX_ENETUNREACH);
    assert(apply("em0", NX_IFC_CLEAR, 0, 0) == 0 && !info("em0").addressed);

    nx_net_if_detach(&em1);
    nx_net_if_detach(&em);
    nx_net_if_detach(&lo);
    nx_net_arp_flush(NULL);
    assert(pool.stats.mbufs == 0 && nx_net_mbuf_pool_destroy(&pool) && !live_pages);
    puts("PASS: interface queries, malformed and overlapping changes, address/up rollback, default route validation, route lookups and down/clear");
    return 0;
}
