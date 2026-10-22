# ICMP and raw echo sockets (task 421)

`kernel/include/net/icmp.h` and `kernel/net/icmp.c` adapt 4.4BSD
`netinet/ip_icmp.c` and `raw_ip.c` as fresh native code under Nexix's root
BSD-3-Clause license. They sit on [IPv4](networking-ip.md) as protocol 1, run
in [network-worker](networking-worker.md) context, never sleep, and **every
function that takes an mbuf chain consumes it**.

## Input

`nx_net_icmp_input` receives validated, reassembled datagrams from IPv4. It
drops, and counts:

- messages shorter than the 8-byte ICMP header (`too_short`); a header split
  across buffers is pulled up first;
- a bad checksum over the whole message (`bad_checksum`).

It then handles the message by type:

- **Echo request.** Answered with an echo reply carrying the same
  identifier, sequence and data. The reply comes from the address the request
  was sent to (or the route's source for other `127/8` addresses), keeps the
  request's TOS and does not reflect IP options. Requests sent to a broadcast
  or network address are ignored (`broadcast_echoes`), so the host cannot
  amplify a smurf attack.
- **Errors** (destination unreachable, time exceeded, parameter problem). The
  quoted datagram must be IPv4 with a 20–60 byte header followed by at least
  8 bytes, and its source must be one of our addresses. A known code is
  mapped, as BSD `inetctlerrmap` does, to a result:

  | Message | Result |
  |---|---|
  | unreachable net, unknown net, net prohibited, net TOS | `UNREACHABLE` |
  | unreachable host, source route failed, unknown host, host prohibited, host TOS, time exceeded | `HOST_UNREACHABLE` |
  | unreachable protocol or port | `REFUSED` |
  | fragmentation needed (with the next-hop MTU) | `TOO_BIG` |
  | parameter problem | `INVALID` |

  The notice goes to the transport that registered for the quoted protocol
  (up to 4; [UDP](networking-udp.md) and [TCP](networking-tcp.md) register). Anything else is
  `bad_errors` and dropped.
- **Everything else** (echo replies, redirects, source quench, timestamp and
  mask messages, unknown types) is not acted on. Static routing ignores
  redirects, and RFC 6633 deprecates source quench.

Every valid message the kernel does not answer itself (that is, everything
except echo requests) is then offered to raw sockets.

## Errors sent

`nx_net_icmp_error` sends an error back to a datagram's source. It quotes the
IP header and up to 8 payload bytes, as BSD does. The source is the address
the datagram was sent to, when that is ours. Fragmentation-needed carries the
MTU. IPv4 calls it, through `nx_net_ip_set_unknown_hook`, for protocol
unreachable; [UDP](networking-udp.md) uses it for port unreachable. Following RFC 1122 and BSD,
no error is sent about:

- a non-first fragment;
- a datagram sent to a broadcast, network or multicast address;
- a source that is not unicast (`0/8`, multicast, reserved);
- an ICMP error, or an ICMP message too short to tell.

These are counted in `errors_suppressed`. An allocation failure sends nothing
(`output_errors`). There is no rate limit; 4.4BSD has none either.

## Raw ICMP sockets

Raw sockets carry ICMP only, adapted from BSD `rip_*`:

- **Privilege.** `nx_net_raw_open(privileged, …)` returns `DENIED` unless the
  caller is privileged. The socket layer (tasks 424–426) passes effective uid
  0 here. At most 8 raw sockets are open at once (`FULL`).
- **Addresses.** `bind` accepts 0 or one of our addresses (else `INVALID`).
  `connect` accepts 0, which disconnects, or a unicast address.
- **Send.** One ICMP message (header and data, without an IP header) of
  8..65515 bytes goes to the given address, or to the connected one. Naming a
  destination while connected is `EXISTS`, and naming none while unconnected
  is `INVALID`. **The kernel computes the ICMP checksum.** The source is the
  bound address or the route's.
- **Receive.** Each matching socket gets its own copy of the whole datagram,
  IP header included. A socket matches when its bound address is 0 or the
  datagram's destination, and its connected address is 0 or the datagram's
  source. Clusters are shared rather than copied. The receive queue holds
  8192 bytes (BSD `RIPRCVQ`), with mbuf accounting capped at four times that.
  Overflow, or a failed copy, is dropped and counted in the socket's
  `receive.drops`. An optional `readable` callback runs after each queued
  datagram, for the blocking waits of task 426.
- **Close** frees anything queued. `nx_net_raw_flush` (called by `net_stop`)
  empties every queue but leaves sockets open.

User-space access through `socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)` arrives
with tasks 424–426, and `ping` with task 428.

## Live stack

`net_start` initializes ICMP, registers it as IP protocol 1 and installs the
protocol-unreachable hook. `net_stop` flushes raw sockets, removes the hook and
unregisters ICMP.

## Validation

`make test-net-icmp` runs ASan/UBSan fixtures with `lo0`, a fake Ethernet
driver and a resolved gateway. They cover:

- **Echo:** replies with options stripped, an odd length, a request split
  across buffers, and every reply checksum; ignored broadcast and
  subnet-broadcast requests; short and bad-checksum messages.
- **Errors sent:** protocol unreachable quoting header plus 8 bytes (or fewer
  when the datagram is shorter), the needfrag MTU, errors about ICMP queries,
  each of the six suppression cases, and allocation failure.
- **Errors received:** each result mapping and the needfrag MTU. Protocols
  without a handler reach raw sockets only. Malformed errors are dropped
  unseen: a short quote, a quote that is not our datagram, unknown codes, the
  wrong quoted version, a missing quoted header. Redirects and source quench
  reach raw sockets only.
- **Raw sockets:** unprivileged denial, the 8-socket limit, bind and connect
  validation, loopback echo with a kernel-computed checksum, the readable
  callback, local/foreign filtering, refusal of every bad send, a message
  split across buffers, queue overflow drops, flush and close.

Every chain must be freed exactly once, with zero live pages at the end.

`make test-net-icmp-guest` (the same boots as `test-pci-guest`) opens a raw
socket in every start/stop round. An unprivileged open is denied. It pings
`127.0.0.1` and checks a protocol-unreachable error for an unregistered
protocol, verifying every checksum and that all mbufs are returned (`NET ICMP
OK`). With an e1000, it also pings QEMU's gateway `10.0.2.2` through `em0` and
ARP.
