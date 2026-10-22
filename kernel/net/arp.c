#include "net/arp.h"
#include "net/route.h"
#include "net/wire.h"

enum { FREE, INCOMPLETE, COMPLETE, REJECT };
struct entry {
    struct nx_net_if *ifp;
    nx_net_addr_t ip;
    uint8_t mac[NX_NET_ETHER_ADDR];
    unsigned state, tries;
    uint64_t deadline;   /* incomplete: next request; otherwise expiry */
    uint64_t used;
    struct nx_net_mbuf *head, *tail;   /* held packets, nextpkt-linked */
    size_t held;
};
static struct entry table[NX_NET_ARP_ENTRIES];
static size_t held_total;
static struct nx_net_mbuf_pool *pool;
static uint64_t (*now_ms)(void);
static struct nx_net_arp_stats stats;

static void drop_held(struct entry *e)
{
    while (e->head) {
        struct nx_net_mbuf *m = e->head;
        e->head = m->nextpkt;
        m->nextpkt = NULL;
        ++stats.hold_drops;
        nx_net_mbuf_drop(m);
    }
    e->tail = NULL;
    held_total -= e->held;
    e->held = 0;
}
static void release(struct entry *e)
{
    drop_held(e);
    *e = (struct entry){0};
}
void nx_net_arp_flush(struct nx_net_if *ifp)
{
    for (unsigned i = 0; i < NX_NET_ARP_ENTRIES; ++i)
        if (table[i].state != FREE && (!ifp || table[i].ifp == ifp)) release(&table[i]);
}
void nx_net_arp_if_changed(struct nx_net_if *ifp) { nx_net_arp_flush(ifp); }
void nx_net_arp_init(struct nx_net_mbuf_pool *packets, uint64_t (*clock)(void))
{
    nx_net_arp_flush(NULL);
    pool = packets;
    now_ms = clock;
    stats = (struct nx_net_arp_stats){0};
}

static void fill(uint8_t *arp, unsigned operation, const uint8_t *sha, nx_net_addr_t spa,
                 const uint8_t *tha, nx_net_addr_t tpa)
{
    nx_net_put16(arp, 1);
    nx_net_put16(arp + 2, NX_NET_ETHERTYPE_IP);
    arp[4] = NX_NET_ETHER_ADDR;
    arp[5] = 4;
    nx_net_put16(arp + 6, (uint16_t)operation);
    for (unsigned i = 0; i < NX_NET_ETHER_ADDR; ++i) { arp[8 + i] = sha[i]; arp[18 + i] = tha[i]; }
    nx_net_put32(arp + 14, spa);
    nx_net_put32(arp + 24, tpa);
}
/* Broadcast a request; an allocation failure just waits for the next retry. */
static void request(struct entry *e)
{
    static const uint8_t unknown[NX_NET_ETHER_ADDR];
    uint8_t arp[sizeof(struct nx_net_arp_wire)];
    fill(arp, 1, e->ifp->lladdr, e->ifp->inet_addr, unknown, e->ip);
    struct nx_net_mbuf *m = nx_net_mbuf_get(pool, true);
    if (!m) return;
    if (!nx_net_mbuf_append(m, arp, sizeof(arp))) { nx_net_mbuf_drop(m); return; }
    if (nx_net_if_output(e->ifp, m, nx_net_ether_broadcast, NX_NET_ETHERTYPE_ARP) == NX_NET_OK)
        ++stats.requests;
}

static struct entry *find(const struct nx_net_if *ifp, nx_net_addr_t ip)
{
    for (unsigned i = 0; i < NX_NET_ARP_ENTRIES; ++i)
        if (table[i].state != FREE && table[i].ifp == ifp && table[i].ip == ip) return &table[i];
    return NULL;
}
/* A free slot, else the least recently used resolved or negative entry;
 * unresolved entries are never evicted, so their held packets stay bounded. */
static struct entry *allocate(struct nx_net_if *ifp, nx_net_addr_t ip)
{
    struct entry *victim = NULL;
    for (unsigned i = 0; i < NX_NET_ARP_ENTRIES; ++i) {
        struct entry *e = &table[i];
        if (e->state == FREE) { victim = e; break; }
        if (e->state != INCOMPLETE && (!victim || e->used < victim->used)) victim = e;
    }
    if (!victim) return NULL;
    if (victim->state != FREE) { ++stats.evictions; release(victim); }
    *victim = (struct entry){ .ifp = ifp, .ip = ip, .state = INCOMPLETE };
    return victim;
}
/* Queue behind an unresolved entry. A full entry drops its oldest packet, as
 * BSD keeps only the newest; with the global bound reached and nothing of
 * this entry's to give up, the new packet is dropped instead. */
static bool hold(struct entry *e, struct nx_net_mbuf *m)
{
    if (e->held == NX_NET_ARP_HOLD || (held_total == NX_NET_ARP_PENDING && e->head)) {
        struct nx_net_mbuf *old = e->head;
        e->head = old->nextpkt;
        if (!e->head) e->tail = NULL;
        old->nextpkt = NULL;
        --e->held;
        --held_total;
        ++stats.hold_drops;
        nx_net_mbuf_drop(old);
    } else if (held_total == NX_NET_ARP_PENDING) {
        ++stats.hold_drops;
        nx_net_mbuf_drop(m);
        return false;
    }
    m->nextpkt = NULL;
    if (e->tail) e->tail->nextpkt = m; else e->head = m;
    e->tail = m;
    ++e->held;
    ++held_total;
    ++stats.held;
    return true;
}

enum nx_net_result nx_net_arp_output(struct nx_net_if *ifp, nx_net_addr_t next_hop, struct nx_net_mbuf *m)
{
    enum nx_net_result result = NX_NET_OK;
    if (!(ifp->flags & NX_NET_IFF_RUNNING)) result = NX_NET_DOWN;
    else if ((ifp->flags & NX_NET_IFF_LOOPBACK) || !ifp->inet_configured ||
             ((next_hop ^ ifp->inet_addr) & ifp->inet_mask) || next_hop == ifp->inet_addr)
        result = NX_NET_INVALID;
    if (result != NX_NET_OK) {
        nx_net_mbuf_drop(m);
        return result;
    }
    uint64_t now = now_ms();
    struct entry *e = find(ifp, next_hop);
    if (e && e->state != INCOMPLETE && now >= e->deadline) { release(e); e = NULL; }
    if (e && e->state == COMPLETE) {
        e->used = now;
        return nx_net_if_output(ifp, m, e->mac, NX_NET_ETHERTYPE_IP);
    }
    if (e && e->state == REJECT) {
        ++stats.rejected;
        nx_net_mbuf_drop(m);
        return NX_NET_HOST_UNREACHABLE;
    }
    if (!e && !(e = allocate(ifp, next_hop))) {
        ++stats.table_full;
        nx_net_mbuf_drop(m);
        return NX_NET_NO_BUFFERS;
    }
    if (!hold(e, m)) result = NX_NET_NO_BUFFERS;
    if (!e->tries) {
        request(e);
        e->tries = 1;
        e->deadline = now + NX_NET_ARP_RETRY_MS;
    }
    return result;
}

void nx_net_arp_timer(void)
{
    uint64_t now = now_ms();
    for (unsigned i = 0; i < NX_NET_ARP_ENTRIES; ++i) {
        struct entry *e = &table[i];
        if (e->state == FREE || now < e->deadline) continue;
        if (e->state != INCOMPLETE) {
            release(e);
        } else if (e->tries >= NX_NET_ARP_MAXTRIES) {
            /* Unreachable: discard what waited and refuse for arpt_down. */
            ++stats.timeouts;
            drop_held(e);
            e->state = REJECT;
            e->used = now;
            e->deadline = now + NX_NET_ARP_DOWN_MS;
        } else {
            request(e);
            ++e->tries;
            e->deadline = now + NX_NET_ARP_RETRY_MS;
        }
    }
}

static bool same(const uint8_t *a, const uint8_t *b)
{
    for (unsigned i = 0; i < NX_NET_ETHER_ADDR; ++i) if (a[i] != b[i]) return false;
    return true;
}
static void resolved(struct entry *e, const uint8_t *mac, uint64_t now)
{
    for (unsigned i = 0; i < NX_NET_ETHER_ADDR; ++i) e->mac[i] = mac[i];
    e->state = COMPLETE;
    e->tries = 0;
    e->used = now;
    e->deadline = now + NX_NET_ARP_KEEP_MS;
    while (e->head) {   /* release held packets in order */
        struct nx_net_mbuf *m = e->head;
        e->head = m->nextpkt;
        m->nextpkt = NULL;
        --e->held;
        --held_total;
        nx_net_if_output(e->ifp, m, e->mac, NX_NET_ETHERTYPE_IP);
    }
    e->tail = NULL;
}

void nx_net_arp_input(struct nx_net_if *ifp, struct nx_net_mbuf *m)
{
    const size_t size = sizeof(struct nx_net_arp_wire);
    if (!(m->flags & NX_NET_M_PKTHDR) || nx_net_mbuf_length(m) < size || !nx_net_mbuf_pullup(&m, size)) {
        ++stats.invalid;
        nx_net_mbuf_drop(m);
        return;
    }
    uint8_t *arp = m->data;
    unsigned operation = nx_net_get16(arp + 6);
    uint8_t sha[NX_NET_ETHER_ADDR];
    for (unsigned i = 0; i < NX_NET_ETHER_ADDR; ++i) sha[i] = arp[8 + i];
    nx_net_addr_t spa = nx_net_get32(arp + 14), tpa = nx_net_get32(arp + 24);
    if (nx_net_get16(arp) != 1 || nx_net_get16(arp + 2) != NX_NET_ETHERTYPE_IP ||
        arp[4] != NX_NET_ETHER_ADDR || arp[5] != 4 || (operation != 1 && operation != 2) ||
        (sha[0] & 1)) {   /* group (incl. broadcast) sender addresses are bogus */
        ++stats.invalid;
        nx_net_mbuf_drop(m);
        return;
    }
    ++stats.received;
    if ((ifp->flags & NX_NET_IFF_LOOPBACK) || !ifp->inet_configured || same(sha, ifp->lladdr)) {
        ++stats.ignored;   /* unconfigured, or our own broadcast echoed */
        nx_net_mbuf_freem(m);
        return;
    }
    if (spa == ifp->inet_addr) {
        ++stats.conflicts;   /* another host claims our address */
        nx_net_mbuf_freem(m);
        return;
    }
    uint64_t now = now_ms();
    bool for_us = tpa == ifp->inet_addr;
    bool learnable = nx_net_inet_unicast(spa) && !((spa ^ ifp->inet_addr) & ifp->inet_mask) &&
                     (~ifp->inet_mask <= 1 || (spa & ~ifp->inet_mask) != ~ifp->inet_mask);
    struct entry *e = learnable ? find(ifp, spa) : NULL;
    if (!e && learnable && for_us) e = allocate(ifp, spa);
    if (e) resolved(e, sha, now);
    if (operation == 1 && for_us) {
        /* Reply in place, as BSD in_arpinput does, without Ethernet padding. */
        nx_net_mbuf_adj_tail(m, nx_net_mbuf_length(m) - size);
        fill(arp, 2, ifp->lladdr, ifp->inet_addr, sha, spa);
        if (nx_net_if_output(ifp, m, sha, NX_NET_ETHERTYPE_ARP) == NX_NET_OK) ++stats.replies;
        return;
    }
    nx_net_mbuf_freem(m);
}

bool nx_net_arp_lookup(const struct nx_net_if *ifp, nx_net_addr_t ip, uint8_t mac[NX_NET_ETHER_ADDR])
{
    const struct entry *e = find(ifp, ip);
    if (!e || e->state != COMPLETE || now_ms() >= e->deadline) return false;
    for (unsigned i = 0; i < NX_NET_ETHER_ADDR; ++i) mac[i] = e->mac[i];
    return true;
}
struct nx_net_arp_stats nx_net_arp_stats(void) { return stats; }
