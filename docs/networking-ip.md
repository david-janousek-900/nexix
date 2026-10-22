# IPv4 input, output and fragments (task 420)

`kernel/include/net/ip.h` and `kernel/net/ip.c` adapt 4.4BSD
`netinet/ip_input.c` and `ip_output.c` as fresh native code under Nexix's root
BSD-3-Clause license. They sit on [static routing and ARP](networking-routing.md),
run in [network-worker](networking-worker.md) context, never sleep, and **every
function that takes an mbuf chain consumes it**.

## Transport registration

Up to 4 transport protocols register one input routine each by IP protocol
number (duplicates are refused). The routine receives the whole validated,
reassembled datagram with its header (including options) contiguous, and the
header length. A datagram for an unregistered protocol is counted in
`no_protocol` and passed to the hook set by `nx_net_ip_set_unknown_hook`, which
[ICMP](networking-icmp.md) uses to send protocol unreachable (dropped when no
hook is set).

## Input

`nx_net_ip_input` is registered for ethertype `0x0800` and checks, in order:

- a packet header and at least 20 bytes (`too_short`), pulled up if split;
- version 4 (`bad_version`);
- a header length of 20..60 bytes within the packet (`bad_header`);
- the header checksum (`bad_checksum`);
- a total length between the header length and the received length
  (`bad_length`); link-layer padding beyond it is trimmed;
- a source that is not multicast, reserved or `255.255.255.255` (`bad_source`);
- the reserved fragment flag clear (`bad_header`).

**Forwarding is disabled.** A datagram is accepted only for one of our
configured addresses, `255.255.255.255`, the receiving interface's subnet
broadcast, or any `127/8` address arriving on loopback. Everything else is
counted in `not_for_us` and dropped. Options are length-checked and passed up,
not interpreted (no source routing or record-route).

## Reassembly

A datagram with MF set or a nonzero offset is a fragment, keyed by source,
destination, identification and protocol.

- **Lengths.** Non-final fragments must carry a nonzero multiple of 8 bytes,
  and no fragment may end past 65535 bytes (`bad_length`).
- **Duplicates** with the same offset and length are dropped alone.
- **Overlaps.** Any partial overlap, a second final fragment with a different
  end, or data past a known end discards the whole datagram (`frag_overlaps`).
  This follows RFC 5722's IPv6 rule instead of BSD's trimming, so overlapping
  data can never replace earlier bytes.
- **Bounds.** At most 8 datagrams under reassembly, 16 fragments per datagram
  (a 17th discards it), and 64 fragments held in total; beyond those, the new
  fragment is dropped (`frag_dropped`).
- **Expiry.** A datagram not completed within 30 seconds (BSD `IPFRAGTTL`) is
  freed by `nx_net_ip_timer`, which runs every 500 ms (`frag_timeouts`).
- **Interfaces.** A down, detached or readdressed interface discards the
  datagrams it last fed.
- **Completion.** Once the first and final fragments are present and the
  pieces cover the whole length, they are chained in offset order behind the
  first fragment's header. The total length is rewritten, the fragment field
  cleared and the checksum recomputed before delivery.

## Output

`nx_net_ip_output(payload, &send)` prepends a 20-byte header: version 4, the
caller's TOS and TTL (default 64), an incrementing identification, DF when
asked, protocol, addresses and checksum. It refuses:

- a chain without a packet header, or a source that is not one of our
  addresses (`INVALID`);
- more than 65515 payload bytes (`TOO_BIG`);
- a destination without a route (the route's result).

A zero source uses the route's preferred address. A datagram that fits the
route's MTU is sent through the [route](networking-routing.md). A larger one is
`TOO_BIG` with DF; otherwise it is split into pieces of `(MTU − 20) & ~7` bytes
(1480 on Ethernet), sharing clusters with the payload. **Every fragment is built
before any is sent**, so an allocation failure sends nothing and returns
`NO_BUFFERS`.

## Live stack

`net_start` initializes IPv4 with the stack clock, registers ethertype `0x0800`
and arms the 500 ms reassembly timer. The interface change hook now discards
both ARP state and partial datagrams. `net_stop` cancels the timer, flushes
reassembly and unregisters IPv4.

## Validation

`make test-net-ip` runs ASan/UBSan fixtures with `lo0`, a fake Ethernet driver
and a manual clock. They cover:

- registration limits;
- output header fields, identification, TTL/TOS/DF, every refusal, exact-MTU
  and 1480/1480/1040 fragmentation, DF refusal, and allocation failure part way
  through fragmenting;
- input acceptance (own, broadcast, loopback) and non-forwarding, padding
  trimming, options, split headers, and each malformed-header counter;
- in-order, reversed, duplicate, zero-length-final and interleaved reassembly,
  a full 65535-byte datagram, both overlap directions, inconsistent ends, bad
  fragment lengths;
- per-datagram, datagram-count and total-fragment bounds, 30-second expiry and
  discard on interface down;
- a 40000-byte datagram fragmented over `lo0` and reassembled.

Every chain must be freed exactly once, with zero live pages at the end.

`make test-net-ip-guest` (the same boots as `test-pci-guest`) configures `lo0`
in every start/stop round and sends 0-, 32- and 20000-byte datagrams to
`127.0.0.1` through a test protocol. The largest is fragmented over the
16384-byte MTU and reassembled. DF above the MTU is refused, and every mbuf and
cluster is returned. The boot reports `NET IP OK`. The routing checks now send
real IPv4 datagrams to our own address and through QEMU's gateway.
