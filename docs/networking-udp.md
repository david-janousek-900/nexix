# UDP (task 422)

`kernel/include/net/udp.h` and `kernel/net/udp.c` adapt 4.4BSD
`netinet/udp_usrreq.c` and the UDP uses of `in_pcb.c` as fresh native code
under Nexix's root BSD-3-Clause license. UDP is IP protocol 17 on
[IPv4](networking-ip.md) and takes error notices from
[ICMP](networking-icmp.md). It runs in [network-worker](networking-worker.md)
context, never sleeps, and **every function that takes an mbuf chain consumes
it**.

## Protocol control blocks

There is a fixed table of 32 sockets. Each holds a local address and port, a
connected peer, a reuse flag, a pending error (BSD `so_error`) and a receive
queue. An optional `readable` callback runs when a datagram or an error
arrives, for the blocking waits of task 426.

- **Bind** happens once (a second bind is `INVALID`). The address must be 0
  (any) or one of ours (`INVALID`). Ports below 1024 need privilege
  (`DENIED`); the socket layer passes effective uid 0 here.
- **Conflicts** follow `in_pcbbind`. A port already bound by another socket
  whose address is the same, or where either address is a wildcard, is `EXISTS`.
  The exception is when both sockets set reuse (`SO_REUSEADDR`, exposed in
  task 426).
- **Ephemeral ports** come from 49152–65535 (IANA dynamic ports, not BSD's
  1024–5000). They are taken round-robin, skip any port in use, wrap around,
  and are `FULL` only when every port is taken.
- **Connect** needs a unicast address and nonzero port. It binds an
  ephemeral port when the socket is unbound and clears any pending error.
  Address 0 disconnects.

## Output

`nx_net_udp_send` sends 0..9216 bytes (BSD `udp_sendspace`; larger is
`TOO_BIG`) to the given address and port, or to the connected peer. Naming a
destination while connected is `EXISTS`. Naming none while unconnected, or
port 0, is `INVALID`. An unbound socket gets an ephemeral port. The source is
the bound address or the route's preferred one. The checksum always covers
the pseudoheader, header and data; a computed zero is sent as `0xffff`.
Datagrams above the MTU are fragmented by IPv4.

## Input

`nx_net_udp_input` drops, and counts:

- datagrams too short for the 8-byte header (`too_short`);
- a UDP length below 8 or beyond the IP payload, that is, truncated
  (`bad_length`);
- a nonzero checksum that does not verify (`bad_checksum`). A zero checksum
  means none was sent and is accepted (RFC 768).

IP bytes beyond the UDP length are trimmed. A socket matches a datagram when
its local port is the destination port, its local address is 0 or the
destination, and its peer is unset or is the datagram's source address and
port.

- **Unicast** goes to the match with the fewest wildcards (BSD
  `in_pcblookup`), so a connected socket wins over a bound one, and a bound
  one over a wildcard. With no match, ICMP port unreachable quotes the
  datagram (`no_port`).
- **Broadcasts** (`255.255.255.255` or the interface's subnet broadcast)
  are copied to every matching socket, clusters shared. With no match they
  are dropped silently (`broadcast_drops`).

A delivered datagram is queued as one record holding its source and its
payload, which may be empty. `nx_net_udp_receive` returns the oldest payload
with its source address and port.

**Bounds.** Each receive queue holds 41600 bytes (BSD `udp_recvspace`, 40 ×
(1024 + 16), counting 8 bytes per datagram for the source), with mbuf
accounting capped at four times that. So floods of tiny datagrams are bounded
too. Overflow is dropped and counted in `queue_drops` and the socket's
`receive.drops`.

## ICMP errors

UDP registers with ICMP for protocol 17. As in BSD `udp_ctlinput`, an error
quoting one of our datagrams is recorded on every **connected** socket with
the quoted local port and peer address and port (and a matching local
address, when bound):

| Error | Result |
|---|---|
| port or protocol unreachable | `REFUSED` |
| network or host unreachable, time exceeded | `UNREACHABLE` or `HOST_UNREACHABLE` |
| fragmentation needed | `TOO_BIG` |

Unconnected sockets are never told. `nx_net_udp_take_error` returns and clears
the pending error.

## Live stack

`net_start` initializes UDP, registers protocol 17 and its ICMP notice
handler. `net_stop` flushes every receive queue and unregisters both.

## Validation

`make test-net-udp` runs ASan/UBSan fixtures with `lo0`, a fake Ethernet
driver, ICMP and a resolved gateway. They cover:

- **Binding:** the 32-socket limit, address validation, privileged ports,
  rebinding, wildcard/specific conflicts in both directions, reuse needing
  both sockets, and ephemeral allocation (order, skipping used ports,
  wrapping across the whole range).
- **Connect:** validation, implicit binding and disconnect.
- **Output:** header fields and pseudoheader checksums, zero-length and
  9216-byte (fragmented) datagrams, every refusal, connected sends and a bound
  source address.
- **Input:**
  - delivery with the source, zero checksums, zero-length datagrams and
    trimmed IP padding;
  - bad checksums, truncated and undersized lengths, and missing headers;
  - fewest-wildcard demultiplexing with connected filtering;
  - broadcast copies to several sockets, and silent unmatched broadcasts;
  - port unreachable quoting the datagram;
  - byte-limit and mbuf-limit queue exhaustion.
- **Errors:** port unreachable and needfrag reach only the connected sender,
  and reconnecting clears them.
- **Loopback:** a 9216-byte round trip, a reply to the ephemeral port, and a
  refused send after the server closes.

Every chain must be freed exactly once, with zero live pages at the end.

`make test-net-udp-guest` (the same boots as `test-pci-guest`) runs over `lo0`
in every start/stop round:

- an unprivileged bind of port 7 is denied;
- a 9216-byte datagram is sent from an ephemeral port, fragmented and
  reassembled, and the server replies to that port;
- after the server closes, the client's next send leaves a `REFUSED` error.

Every mbuf and cluster is returned (`NET UDP OK`).
