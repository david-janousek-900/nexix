#include "net/if.h"
#include "net/wire.h"

static struct nx_net_if *interfaces[NX_NET_IF_MAX];
static struct { uint16_t type; nx_net_proto_input input; } protocols[NX_NET_PROTOCOLS];
static void (*change_hook)(struct nx_net_if *);
const uint8_t nx_net_ether_broadcast[NX_NET_ETHER_ADDR] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static bool same_name(const char *a, const char *b)
{
    for (size_t i = 0; i < NX_NET_IF_NAME; ++i) {
        if (a[i] != b[i]) return false;
        if (!a[i]) return true;
    }
    return true;
}
static bool name_valid(const char *name)
{
    for (size_t i = 0; i < NX_NET_IF_NAME; ++i) if (!name[i]) return i > 0;
    return false;
}
static void copy_name(char *to, const char *from)
{
    size_t i = 0;
    for (; i + 1 < NX_NET_IF_NAME && from[i]; ++i) to[i] = from[i];
    for (; i < NX_NET_IF_NAME; ++i) to[i] = 0;
}
static bool same_address(const uint8_t *a, const uint8_t *b)
{
    for (unsigned i = 0; i < NX_NET_ETHER_ADDR; ++i) if (a[i] != b[i]) return false;
    return true;
}

enum nx_net_result nx_net_proto_register(uint16_t type, nx_net_proto_input input)
{
    if (!input) return NX_NET_INVALID;
    for (unsigned i = 0; i < NX_NET_PROTOCOLS; ++i)
        if (protocols[i].input && protocols[i].type == type) return NX_NET_EXISTS;
    for (unsigned i = 0; i < NX_NET_PROTOCOLS; ++i)
        if (!protocols[i].input) { protocols[i].type = type; protocols[i].input = input; return NX_NET_OK; }
    return NX_NET_FULL;
}
void nx_net_proto_unregister(uint16_t type)
{
    for (unsigned i = 0; i < NX_NET_PROTOCOLS; ++i)
        if (protocols[i].input && protocols[i].type == type) protocols[i].input = NULL;
}
/* Hand a payload to its protocol, or drop it as BSD counts if_noproto. */
static void demux(struct nx_net_if *ifp, uint16_t type, struct nx_net_mbuf *m)
{
    for (unsigned i = 0; i < NX_NET_PROTOCOLS; ++i)
        if (protocols[i].input && protocols[i].type == type) { protocols[i].input(ifp, m); return; }
    ++ifp->stats.noproto;
    nx_net_mbuf_drop(m);
}

enum nx_net_result nx_net_if_attach(struct nx_net_if *ifp)
{
    if (!ifp || ifp->attached || !name_valid(ifp->name) || !ifp->mtu ||
        !ifp->ops || !ifp->ops->transmit) return NX_NET_INVALID;
    if (nx_net_if_find(ifp->name)) return NX_NET_EXISTS;
    for (unsigned i = 0; i < NX_NET_IF_MAX; ++i)
        if (!interfaces[i]) {
            interfaces[i] = ifp;
            ifp->attached = true;
            ifp->flags &= ~(NX_NET_IFF_UP | NX_NET_IFF_RUNNING);
            return NX_NET_OK;
        }
    return NX_NET_FULL;
}
void nx_net_if_detach(struct nx_net_if *ifp)
{
    if (!ifp->attached) return;
    nx_net_if_down(ifp);
    for (unsigned i = 0; i < NX_NET_IF_MAX; ++i)
        if (interfaces[i] == ifp) interfaces[i] = NULL;
    ifp->attached = false;
    ifp->inet_configured = false;
    ifp->inet_addr = ifp->inet_mask = 0;
    nx_net_if_changed(ifp);
}
void nx_net_if_set_change_hook(void (*hook)(struct nx_net_if *)) { change_hook = hook; }
void nx_net_if_changed(struct nx_net_if *ifp) { if (change_hook) change_hook(ifp); }
struct nx_net_if *nx_net_if_find(const char *name)
{
    for (unsigned i = 0; i < NX_NET_IF_MAX; ++i)
        if (interfaces[i] && same_name(interfaces[i]->name, name)) return interfaces[i];
    return NULL;
}
size_t nx_net_if_count(void)
{
    size_t count = 0;
    for (unsigned i = 0; i < NX_NET_IF_MAX; ++i) count += interfaces[i] != NULL;
    return count;
}
struct nx_net_if *nx_net_if_at(size_t index)
{
    for (unsigned i = 0; i < NX_NET_IF_MAX; ++i)
        if (interfaces[i] && !index--) return interfaces[i];
    return NULL;
}
enum nx_net_result nx_net_if_up(struct nx_net_if *ifp)
{
    if (!ifp->attached) return NX_NET_INVALID;
    if (ifp->flags & NX_NET_IFF_UP) return NX_NET_OK;
    if (ifp->ops->up && !ifp->ops->up(ifp)) return NX_NET_DOWN;
    ifp->flags |= NX_NET_IFF_UP | NX_NET_IFF_RUNNING;
    return NX_NET_OK;
}
static void flush_queue(struct nx_net_if *ifp)
{
    while (ifp->queue_head) {
        struct nx_net_mbuf *m = ifp->queue_head;
        ifp->queue_head = m->nextpkt;
        m->nextpkt = NULL;
        ++ifp->stats.iqdrops;
        nx_net_mbuf_drop(m);
    }
    ifp->queue_tail = NULL;
    ifp->queue_length = 0;
}
void nx_net_if_down(struct nx_net_if *ifp)
{
    if (!(ifp->flags & NX_NET_IFF_UP)) return;
    ifp->flags &= ~(NX_NET_IFF_UP | NX_NET_IFF_RUNNING);
    if (ifp->ops->down) ifp->ops->down(ifp);
    flush_queue(ifp);
    nx_net_if_changed(ifp);
}

enum nx_net_result nx_net_if_output(struct nx_net_if *ifp, struct nx_net_mbuf *m,
                                    const uint8_t dst[NX_NET_ETHER_ADDR], uint16_t type)
{
    enum nx_net_result result = NX_NET_OK;
    size_t length = nx_net_mbuf_length(m);
    if (!(m->flags & NX_NET_M_PKTHDR)) result = NX_NET_INVALID;
    else if (!(ifp->flags & NX_NET_IFF_RUNNING)) result = NX_NET_DOWN;
    else if (length > ifp->mtu) result = NX_NET_TOO_BIG;
    else if (!(ifp->flags & NX_NET_IFF_LOOPBACK) &&
             !nx_net_mbuf_prepend(&m, NX_NET_ETHER_HEADER)) result = NX_NET_NO_BUFFERS;
    else if ((ifp->flags & NX_NET_IFF_LOOPBACK) && !nx_net_mbuf_prepend(&m, 2)) result = NX_NET_NO_BUFFERS;
    if (result != NX_NET_OK) {
        ++ifp->stats.oerrors;
        nx_net_mbuf_drop(m);
        return result;
    }
    if (ifp->flags & NX_NET_IFF_LOOPBACK) {
        /* The type travels in front of the payload, as BSD prepends the
         * address family; delivery is deferred through the queue. */
        nx_net_put16(m->data, type);
        if (ifp->queue_length >= ifp->queue_limit) {
            ++ifp->stats.oerrors;
            ++ifp->stats.iqdrops;
            nx_net_mbuf_drop(m);
            return NX_NET_NO_BUFFERS;
        }
        m->nextpkt = NULL;
        if (ifp->queue_tail) ifp->queue_tail->nextpkt = m; else ifp->queue_head = m;
        ifp->queue_tail = m;
        ++ifp->queue_length;
        ++ifp->stats.opackets;
        ifp->stats.obytes += length;
        if (ifp->kick) ifp->kick(ifp->kick_argument);
        return NX_NET_OK;
    }
    uint8_t *header = m->data;
    for (unsigned i = 0; i < NX_NET_ETHER_ADDR; ++i) {
        header[i] = dst[i];
        header[NX_NET_ETHER_ADDR + i] = ifp->lladdr[i];
    }
    nx_net_put16(header + 12, type);
    if (!ifp->ops->transmit(ifp, m)) { ++ifp->stats.oerrors; return NX_NET_FULL; }
    ++ifp->stats.opackets;
    ifp->stats.obytes += length + NX_NET_ETHER_HEADER;
    return NX_NET_OK;
}

void nx_net_ether_init(struct nx_net_if *ifp, const char *name, const uint8_t lladdr[NX_NET_ETHER_ADDR],
                       const struct nx_net_if_ops *ops, void *driver)
{
    *ifp = (struct nx_net_if){ .flags = NX_NET_IFF_BROADCAST, .mtu = NX_NET_ETHER_MTU,
                               .ops = ops, .driver = driver };
    copy_name(ifp->name, name);
    for (unsigned i = 0; i < NX_NET_ETHER_ADDR; ++i) ifp->lladdr[i] = lladdr[i];
}
void nx_net_ether_input(struct nx_net_if *ifp, struct nx_net_mbuf *m)
{
    size_t length = nx_net_mbuf_length(m);
    if (!(ifp->flags & NX_NET_IFF_RUNNING)) {
        ++ifp->stats.iqdrops;
        nx_net_mbuf_drop(m);
        return;
    }
    if (!(m->flags & NX_NET_M_PKTHDR) || length < NX_NET_ETHER_HEADER ||
        length > NX_NET_ETHER_HEADER + ifp->mtu || !nx_net_mbuf_pullup(&m, NX_NET_ETHER_HEADER)) {
        ++ifp->stats.ierrors;
        nx_net_mbuf_drop(m);
        return;
    }
    const uint8_t *header = m->data;
    if (!same_address(header, ifp->lladdr) && !same_address(header, nx_net_ether_broadcast)) {
        ++ifp->stats.iqdrops;   /* Not for us: no promiscuous or multicast receive. */
        nx_net_mbuf_drop(m);
        return;
    }
    uint16_t type = nx_net_get16(header + 12);
    ++ifp->stats.ipackets;
    ifp->stats.ibytes += length;
    nx_net_mbuf_adj_head(m, NX_NET_ETHER_HEADER);
    demux(ifp, type, m);
}

static bool loop_transmit(struct nx_net_if *ifp, struct nx_net_mbuf *m)
{
    (void)ifp;
    nx_net_mbuf_drop(m);   /* never used: loopback output queues directly */
    return false;
}
static const struct nx_net_if_ops loop_attach_ops = { NULL, NULL, loop_transmit };
void nx_net_loop_init(struct nx_net_if *ifp, const char *name, void (*kick)(void *), void *argument)
{
    *ifp = (struct nx_net_if){ .flags = NX_NET_IFF_LOOPBACK, .mtu = NX_NET_LOOP_MTU,
                               .ops = &loop_attach_ops, .queue_limit = NX_NET_IFQ_MAXLEN,
                               .kick = kick, .kick_argument = argument };
    copy_name(ifp->name, name);
}
size_t nx_net_loop_service(struct nx_net_if *ifp, size_t budget)
{
    while (budget-- && ifp->queue_head) {
        struct nx_net_mbuf *m = ifp->queue_head;
        ifp->queue_head = m->nextpkt;
        if (!ifp->queue_head) ifp->queue_tail = NULL;
        --ifp->queue_length;
        m->nextpkt = NULL;
        uint16_t type = nx_net_get16(m->data);
        nx_net_mbuf_adj_head(m, 2);
        ++ifp->stats.ipackets;
        ifp->stats.ibytes += nx_net_mbuf_length(m);
        demux(ifp, type, m);
    }
    return ifp->queue_length;
}
