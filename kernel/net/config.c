#include "net/config.h"
#include <nexix/errno.h>

static uint32_t be32(uint32_t v) { return __builtin_bswap32(v); }
/* A name must be NUL-terminated within its field. */
static bool name_ok(const char name[NX_IFNAMSIZ])
{
    for (unsigned i = 0; i < NX_IFNAMSIZ; ++i) if (!name[i]) return i > 0;
    return false;
}
static void copy_name(char out[NX_IFNAMSIZ], const char *in)
{
    unsigned i = 0;
    for (; i < NX_IFNAMSIZ - 1 && in[i]; ++i) out[i] = in[i];
    for (; i < NX_IFNAMSIZ; ++i) out[i] = 0;
}

int nx_net_config_ifinfo(struct nx_ifinfo *r)
{
    struct nx_net_if *ifp;
    if (r->name[0]) {
        if (!name_ok(r->name)) return -NX_EINVAL;
        if (!(ifp = nx_net_if_find(r->name))) return -NX_ENXIO;
    } else if (r->index >= nx_net_if_count() || !(ifp = nx_net_if_at(r->index))) {
        return -NX_ENXIO;
    }
    uint32_t index = 0;
    while (nx_net_if_at(index) != ifp) ++index;
    *r = (struct nx_ifinfo){ .index = index, .flags = ifp->flags, .mtu = (uint32_t)ifp->mtu,
        .ipackets = ifp->stats.ipackets, .ierrors = ifp->stats.ierrors, .opackets = ifp->stats.opackets,
        .oerrors = ifp->stats.oerrors, .ibytes = ifp->stats.ibytes, .obytes = ifp->stats.obytes,
        .iqdrops = ifp->stats.iqdrops };
    copy_name(r->name, ifp->name);
    for (unsigned i = 0; i < NX_NET_ETHER_ADDR; ++i) r->lladdr[i] = ifp->lladdr[i];
    if (ifp->inet_configured) {
        r->addressed = 1;
        r->addr = be32(ifp->inet_addr);
        r->mask = be32(ifp->inet_mask);
        /* Only broadcast-capable interfaces have one, and not below /31. */
        if ((ifp->flags & NX_NET_IFF_BROADCAST) && ~ifp->inet_mask > 1)
            r->broadcast = be32(ifp->inet_addr | ~ifp->inet_mask);
    }
    return 0;
}

int nx_net_config_apply(const struct nx_ifconfig *c)
{
    uint32_t change = c->change;
    if (!change || (change & ~(uint32_t)(NX_IFC_ADDRESS | NX_IFC_CLEAR | NX_IFC_UP | NX_IFC_DOWN)) ||
        (change & NX_IFC_ADDRESS && change & NX_IFC_CLEAR) || (change & NX_IFC_UP && change & NX_IFC_DOWN) ||
        c->reserved || !name_ok(c->name))
        return -NX_EINVAL;
    struct nx_net_if *ifp = nx_net_if_find(c->name);
    if (!ifp) return -NX_ENXIO;
    bool had = ifp->inet_configured;
    nx_net_addr_t old_addr = ifp->inet_addr, old_mask = ifp->inet_mask;
    if (change & NX_IFC_ADDRESS) {
        enum nx_net_result r = nx_net_inet_set(ifp, be32(c->addr), be32(c->mask));
        if (r == NX_NET_EXISTS) return -NX_EEXIST;   /* overlaps another interface's subnet */
        if (r != NX_NET_OK) return -NX_EINVAL;
    }
    if (change & NX_IFC_CLEAR) nx_net_inet_clear(ifp);
    if (change & NX_IFC_UP && nx_net_if_up(ifp) != NX_NET_OK) {
        if (change & (NX_IFC_ADDRESS | NX_IFC_CLEAR)) {   /* roll back */
            if (had) nx_net_inet_set(ifp, old_addr, old_mask);
            else nx_net_inet_clear(ifp);
        }
        return -NX_EIO;
    }
    if (change & NX_IFC_DOWN) nx_net_if_down(ifp);
    return 0;
}

int nx_net_config_get_route(struct nx_route *r)
{
    nx_net_addr_t gateway;
    bool set = nx_net_route_get_default(&gateway);
    *r = (struct nx_route){ .gateway = set ? be32(gateway) : 0, .flags = set ? NX_RT_DEFAULT : 0 };
    return 0;
}
int nx_net_config_set_route(const struct nx_route *r)
{
    if (r->flags) return -NX_EINVAL;
    if (!r->gateway) { nx_net_route_clear_default(); return 0; }
    switch (nx_net_route_set_default(be32(r->gateway))) {
    case NX_NET_OK: return 0;
    case NX_NET_UNREACHABLE: return -NX_ENETUNREACH;   /* not inside a configured subnet */
    default: return -NX_EINVAL;
    }
}
int nx_net_config_lookup(struct nx_route_lookup *r)
{
    struct nx_net_route route;
    switch (nx_net_route_lookup(be32(r->dst), &route)) {
    case NX_NET_OK: break;
    case NX_NET_INVALID: return -NX_EINVAL;
    default: return -NX_ENETUNREACH;
    }
    copy_name(r->ifname, route.ifp->name);
    r->next_hop = be32(route.next_hop);
    r->source = be32(route.source);
    r->flags = (route.local ? NX_RT_LOCAL : 0) | (route.broadcast ? NX_RT_BROADCAST : 0) |
               (route.gateway ? NX_RT_GATEWAY : 0);
    return 0;
}
