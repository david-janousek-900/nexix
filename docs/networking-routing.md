# Static IPv4 routing and ARP (task 419)

`kernel/include/net/route.h` with `kernel/net/route.c`, and
`kernel/include/net/arp.h` with `kernel/net/arp.c`, adapt the static parts of
4.4BSD `netinet/in.c`, `net/route.c` and `netinet/if_ether.c`. They are fresh
native implementations under Nexix's root BSD-3-Clause license. Like the
[interfaces](networking-interfaces.md) they build on, everything runs in
[network-worker](networking-worker.md) context, never sleeps, and **every
function that takes an mbuf chain consumes it**. Addresses are host-order
`nx_net_addr_t` values.

## Addresses

Each interface holds at most one IPv4 address and netmask (`inet_addr`,
`inet_mask`, `inet_configured` in `struct nx_net_if`), the counterpart of a
single BSD `in_ifaddr`. `nx_net_inet_set` validates before changing anything:

- the interface must be attached;
- the netmask must be contiguous and nonzero (`/1`–`/32`);
- the address must be unicast: not `0.0.0.0/8`, multicast `224/4` or reserved
  `240/4` (which includes `255.255.255.255`);
- below `/31`, the subnet's network and broadcast addresses are refused;
- `127.0.0.0/8` is allowed only on a loopback interface, and a loopback
  interface accepts nothing else;
- a subnet overlapping another interface's (compared under the shorter mask) is
  refused with `NX_NET_EXISTS`.

A refused change leaves the previous address in place. A successful change, a
clear, `down` and `detach` all call the interface change hook, which discards
that interface's ARP entries and held packets. Detach also clears the address.

## Routes

The table is implied rather than stored: each configured interface contributes
its connected subnet, and `nx_net_route_set_default` adds one default gateway.
The gateway must be unicast, not one of our addresses, and inside a configured
non-loopback subnet when set (otherwise `NX_NET_UNREACHABLE`). It is rechecked
at every lookup, so clearing or downing its interface makes off-link
destinations unreachable without deleting it. There is no radix tree, host
route, redirect or dynamic routing.

`nx_net_route_lookup` considers only interfaces that are configured and
running, in this order:

1. **Local.** One of our addresses goes through the first running loopback
   interface (`local`, source = destination), as BSD's loopback host routes do.
2. **Connected.** The longest matching subnet. Its broadcast address (below
   `/31`) is flagged `broadcast`.
3. **Default.** Through the gateway (`gateway`, next hop = gateway).

`0.0.0.0`, multicast and reserved destinations are `NX_NET_INVALID`. Limited
broadcast `255.255.255.255` is `NX_NET_UNREACHABLE` until DHCP-style use needs
it. The route also reports the preferred source address for task 420.

`nx_net_route_output(packet, dst)` sends an already built IPv4 packet:
loopback routes queue it on `lo0`, directed broadcasts go to the Ethernet
broadcast address, and everything else goes through ARP to the next hop.

## ARP

A fixed table of 16 entries replaces BSD's routing-table `llinfo_arp`. Entries
are unresolved, resolved, or negative.

- **Output.** `nx_net_arp_output` requires a running, configured Ethernet
  interface and an on-link next hop other than our own address. A resolved
  entry sends at once. Otherwise the packet is held and a broadcast request is
  sent on first use.
- **Held packets.** At most 4 per entry and 16 in total. A full entry drops its
  oldest packet for the newest, as BSD keeps only the latest; when the total
  is reached and the entry holds nothing, the new packet is dropped
  (`NO_BUFFERS`). Held packets are released in order when the reply arrives.
- **Retries.** `nx_net_arp_timer` runs every 250 ms. Requests repeat each
  second; after 5 (BSD `arp_maxtries`) the held packets are dropped and the
  entry turns negative for 20 s (`arpt_down`), during which output fails with
  `NX_NET_HOST_UNREACHABLE`.
- **Expiry.** Resolved entries last 20 minutes (`arpt_keep`), then the next
  packet asks again.
- **Table full.** A new peer evicts the least recently used resolved or
  negative entry. Unresolved entries are never evicted, so held memory stays
  bounded; when all 16 are unresolved, new peers are refused with
  `NX_NET_NO_BUFFERS`.
- **Request allocation failure** leaves the packet held; the next retry asks
  again.

`nx_net_arp_input` handles ethertype `0x0806`:

- it drops (as `invalid`) messages shorter than 28 bytes, not Ethernet/IPv4
  with 6/4-byte addresses, with an operation other than request or reply, or
  with a group (including broadcast) sender address;
- it ignores messages on loopback or unconfigured interfaces and our own echoed
  broadcasts, and counts a sender claiming our address as a `conflict`;
- it updates a cached sender from any message, and creates an entry only when
  the message targets our address; senders must be on-link unicast hosts, not
  the subnet broadcast. A probe from `0.0.0.0` is answered but not learned;
- it answers a request for our address in place, trimmed of Ethernet padding.

## Live stack

`net_start` initializes ARP with a millisecond clock from `timer_ticks()`,
installs the change hook, registers the ARP ethertype and arms the periodic
timer. Interfaces start unaddressed. `net_stop` cancels the timer, clears the
default route, detaches interfaces, flushes ARP and unregisters it. Socket and
tool configuration belongs to task 427; [IPv4 input/output](networking-ip.md)
(task 420) builds on `nx_net_route_send`.

## Validation

`make test-net-route` runs ASan/UBSan fixtures with `lo0`, two fake Ethernet
interfaces and a manual clock. They cover:

- mask, unicast, network/broadcast, loopback, `/31`/`/32`, overlap and
  unattached-interface checks;
- default-gateway refusal (own, off-link, loopback-only, non-unicast);
- lookup order, longest prefix, broadcast, local, invalid and limited-broadcast
  destinations, and routes vanishing with a down or unaddressed interface;
- the exact request layout, in-order release of held packets, cached sends,
  per-entry and global hold limits, retries at exact one-second boundaries,
  timeout to a negative entry and its expiry, resolved-entry expiry;
- malformed input, in-place replies, learning rules, probes, conflicts and
  echoes; flushing on readdress and down;
- a table full of unresolved entries, LRU eviction and request allocation
  failure.

Every chain must be freed exactly once, with zero live pages at the end.

`make test-net-route-guest` (the same boots as `test-pci-guest`) configures
`lo0` 127.0.0.1/8 and `em0` 10.0.2.15/24 with gateway 10.0.2.2 under QEMU user
networking. A packet to our own address arrives through `lo0`; a packet to an
off-link address resolves the gateway to `52:55:0a:00:02:02` and is released;
in the first round the absent peer 10.0.2.77 times out after five requests and
is then refused. Downing `em0` flushes its entries. The boot reports
`NET ROUTE OK`.
