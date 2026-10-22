#ifndef NEXIX_NET_ROUTE_H
#define NEXIX_NET_ROUTE_H
#include "net/if.h"

/* Static IPv4 addressing and routing adapted from 4.4BSD netinet/in.c and
 * net/route.c (task 419). Each interface holds at most one address and
 * netmask, which implies its connected route; one default gateway completes
 * the table. There is no radix tree, redirect or dynamic route. Addresses are
 * host order. Everything runs in network-worker context.
 */

/* Contiguous, nonzero netmask. */
bool nx_net_inet_mask_valid(nx_net_addr_t mask);
/* Usable unicast host: not 0.0.0.0/8, multicast, reserved or broadcast. */
bool nx_net_inet_unicast(nx_net_addr_t);

/* Configure an attached interface. Refuses (INVALID) a bad mask, a non-unicast
 * address, the network or broadcast address of a subnet wider than /31,
 * 127.0.0.0/8 off loopback or other addresses on it; refuses (EXISTS) a subnet
 * overlapping another interface's. Success replaces any previous address and
 * reports the change. */
enum nx_net_result nx_net_inet_set(struct nx_net_if *, nx_net_addr_t addr, nx_net_addr_t mask);
void nx_net_inet_clear(struct nx_net_if *);

/* The gateway must be a unicast address, not one of ours, inside a configured
 * non-loopback subnet when set (else UNREACHABLE); later lookups recheck it. */
enum nx_net_result nx_net_route_set_default(nx_net_addr_t gateway);
void nx_net_route_clear_default(void);
bool nx_net_route_get_default(nx_net_addr_t *gateway);

struct nx_net_route {
    struct nx_net_if *ifp;
    nx_net_addr_t next_hop;   /* destination on-link, else the gateway */
    nx_net_addr_t source;     /* preferred local address */
    bool local, broadcast, gateway;
};
/* Only running, configured interfaces route. Our own addresses go through the
 * first running loopback interface; then the longest matching connected
 * subnet; then the default gateway. 0.0.0.0, multicast and reserved
 * destinations are INVALID; limited broadcast and no match are UNREACHABLE. */
enum nx_net_result nx_net_route_lookup(nx_net_addr_t dst, struct nx_net_route *);

/* Send an IPv4 packet (header already built by the caller) toward dst:
 * loopback, Ethernet broadcast for a directed subnet broadcast, else through
 * ARP to the next hop. Consumes the packet. NX_NET_OK includes packets held
 * awaiting ARP resolution. */
enum nx_net_result nx_net_route_output(struct nx_net_mbuf *, nx_net_addr_t dst);
#endif
