#include "net/route.h"
#include "net/arp.h"

static nx_net_addr_t default_gateway;
static bool has_default;

bool nx_net_inet_mask_valid(nx_net_addr_t mask)
{
    nx_net_addr_t host = ~mask;
    return mask && !(host & (host + 1));
}
bool nx_net_inet_unicast(nx_net_addr_t a)
{
    return (a >> 24) != 0 && (a >> 28) < 0xe;   /* not 0/8, 224/4 or 240/4 */
}
static bool wide(nx_net_addr_t mask) { return ~mask > 1; }   /* shorter than /31 */
static bool loopback_net(nx_net_addr_t a) { return (a >> 24) == 127; }

enum nx_net_result nx_net_inet_set(struct nx_net_if *ifp, nx_net_addr_t addr, nx_net_addr_t mask)
{
    if (!ifp->attached || !nx_net_inet_mask_valid(mask) || !nx_net_inet_unicast(addr) ||
        loopback_net(addr) != !!(ifp->flags & NX_NET_IFF_LOOPBACK)) return NX_NET_INVALID;
    if (wide(mask) && ((addr & ~mask) == 0 || (addr & ~mask) == ~mask)) return NX_NET_INVALID;
    for (size_t i = 0; i < nx_net_if_count(); ++i) {
        const struct nx_net_if *other = nx_net_if_at(i);
        nx_net_addr_t common = mask & other->inet_mask;
        if (other != ifp && other->inet_configured &&
            (addr & common) == (other->inet_addr & common)) return NX_NET_EXISTS;
    }
    ifp->inet_addr = addr;
    ifp->inet_mask = mask;
    ifp->inet_configured = true;
    nx_net_if_changed(ifp);
    return NX_NET_OK;
}
void nx_net_inet_clear(struct nx_net_if *ifp)
{
    if (!ifp->inet_configured) return;
    ifp->inet_configured = false;
    ifp->inet_addr = ifp->inet_mask = 0;
    nx_net_if_changed(ifp);
}

static bool on_link(const struct nx_net_if *ifp, nx_net_addr_t a)
{
    return ifp->inet_configured && ((a ^ ifp->inet_addr) & ifp->inet_mask) == 0;
}
static bool routable(const struct nx_net_if *ifp)
{
    return ifp->inet_configured && (ifp->flags & NX_NET_IFF_RUNNING);
}
/* Longest connected prefix holding a, among running interfaces. */
static struct nx_net_if *connected(nx_net_addr_t a, bool loopback)
{
    struct nx_net_if *best = NULL;
    for (size_t i = 0; i < nx_net_if_count(); ++i) {
        struct nx_net_if *ifp = nx_net_if_at(i);
        if (routable(ifp) && on_link(ifp, a) && (loopback || !(ifp->flags & NX_NET_IFF_LOOPBACK)) &&
            (!best || ifp->inet_mask > best->inet_mask)) best = ifp;
    }
    return best;
}
static bool local_address(nx_net_addr_t a, bool running)
{
    for (size_t i = 0; i < nx_net_if_count(); ++i) {
        const struct nx_net_if *ifp = nx_net_if_at(i);
        if (ifp->inet_configured && ifp->inet_addr == a && (!running || routable(ifp))) return true;
    }
    return false;
}

enum nx_net_result nx_net_route_set_default(nx_net_addr_t gateway)
{
    if (!nx_net_inet_unicast(gateway) || local_address(gateway, false)) return NX_NET_INVALID;
    bool found = false;
    for (size_t i = 0; i < nx_net_if_count(); ++i) {
        const struct nx_net_if *ifp = nx_net_if_at(i);
        found |= !(ifp->flags & NX_NET_IFF_LOOPBACK) && on_link(ifp, gateway);
    }
    if (!found) return NX_NET_UNREACHABLE;
    default_gateway = gateway;
    has_default = true;
    return NX_NET_OK;
}
void nx_net_route_clear_default(void) { has_default = false; default_gateway = 0; }
bool nx_net_route_get_default(nx_net_addr_t *gateway)
{
    if (has_default) *gateway = default_gateway;
    return has_default;
}

enum nx_net_result nx_net_route_lookup(nx_net_addr_t dst, struct nx_net_route *route)
{
    *route = (struct nx_net_route){0};
    if (dst == UINT32_MAX) return NX_NET_UNREACHABLE;   /* limited broadcast: deferred */
    if (!nx_net_inet_unicast(dst) && !loopback_net(dst)) return NX_NET_INVALID;
    if (local_address(dst, true)) {
        for (size_t i = 0; i < nx_net_if_count(); ++i) {
            struct nx_net_if *ifp = nx_net_if_at(i);
            if ((ifp->flags & NX_NET_IFF_LOOPBACK) && (ifp->flags & NX_NET_IFF_RUNNING)) {
                *route = (struct nx_net_route){ ifp, dst, dst, true, false, false };
                return NX_NET_OK;
            }
        }
        return NX_NET_UNREACHABLE;
    }
    struct nx_net_if *ifp = connected(dst, true);
    if (ifp) {
        bool broadcast = wide(ifp->inet_mask) && (dst & ~ifp->inet_mask) == ~ifp->inet_mask;
        *route = (struct nx_net_route){ ifp, dst, ifp->inet_addr, false, broadcast, false };
        return NX_NET_OK;
    }
    if (has_default && (ifp = connected(default_gateway, false)) && !local_address(default_gateway, false)) {
        *route = (struct nx_net_route){ ifp, default_gateway, ifp->inet_addr, false, false, true };
        return NX_NET_OK;
    }
    return NX_NET_UNREACHABLE;
}

enum nx_net_result nx_net_route_output(struct nx_net_mbuf *m, nx_net_addr_t dst)
{
    struct nx_net_route route;
    enum nx_net_result result = nx_net_route_lookup(dst, &route);
    if (result != NX_NET_OK) {
        nx_net_mbuf_drop(m);
        return result;
    }
    if (route.ifp->flags & NX_NET_IFF_LOOPBACK)
        return nx_net_if_output(route.ifp, m, NULL, NX_NET_ETHERTYPE_IP);
    if (route.broadcast)
        return nx_net_if_output(route.ifp, m, nx_net_ether_broadcast, NX_NET_ETHERTYPE_IP);
    return nx_net_arp_output(route.ifp, route.next_hop, m);
}
