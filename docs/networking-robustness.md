# Networking host robustness (task 430)

`tests/net_robust.c` runs the whole protocol stack on the host, under
AddressSanitizer and UndefinedBehaviorSanitizer:

- ARP, IPv4 with fragmentation, ICMP, UDP, TCP and raw ICMP sockets;
- a fake Ethernet card and `lo0` (with a 1500-byte MTU, for many segments);
- a manual clock.

It complements the per-protocol tests (`test-net-if` through `test-net-tcp`).
Those prove each rule; this one checks the layers together against malformed
input, exhaustion, time and teardown.

Each phase builds the stack on its own pool and must end at its **baseline**:
- no mbufs or clusters in use;
- no datagram under reassembly;
- no TCP control block, UDP socket or raw socket left.

The peaks of mbufs, clusters and pages must also stay within the configured
limits. Packets held by ARP are mbufs, so the pool counters cover them too.
Finally, the pool must be destroyable with no page left.

## Phases

1. **Mutation.** There are ten valid seed frames:
   - an ARP reply and an ARP request;
   - an ICMP echo request;
   - UDP to a bound port, and UDP to a closed one;
   - a TCP SYN to a listener;
   - TCP data on an established connection;
   - the first, a middle and the last fragment of one datagram.

   40000 frames are derived from them. Seven in eight get one to four
   mutations: bit flips, random header bytes, truncation, extension, or
   random length, id and fragment fields. The IP and transport checksums are
   usually repaired, so mutated frames reach the deeper layers. Frames
   arrive on `em0` and sometimes on `lo0`, and the clock and every protocol
   timer advance as they go. An application side drains the sockets,
   accepts and closes connections, and writes into them.

   Then the sockets are closed and the clock runs **three hours** at 500 ms
   steps with the timers. Without any flush, ARP holds, reassembly, SYN-ACK
   retries, retransmissions, `FIN_WAIT_2` and `TIME_WAIT` must all expire
   back to baseline.
2. **Exhaustion.** A pool of 48 mbufs and 12 clusters on a 24-page budget.
   - A 60000-byte loopback TCP transfer competes with a UDP flood to a socket
     that never reads.
   - Allocation failures happen and peaks reach the limits, yet the transfer
     arrives intact through retransmission.
   - 300 incomplete fragmented datagrams then stay within the 8 reassembly
     queues, with drops counted.
   - Closing and expiry return to baseline.
3. **Teardown.** With traffic in flight, the stack is taken down in
   `net_stop`'s order: interfaces first, then flushes. The traffic in flight
   is:
   - datagrams held for an absent neighbour;
   - partial reassembly;
   - queued UDP and raw datagrams;
   - an unanswered connect;
   - a loopback connection mid-handshake.

Every run prints its counters (bad checksums, bad lengths, fragments and
overlaps, segments, resets, retransmissions) and the peaks it reached.

## Seeds

The generator is a fixed xorshift64. `make test-net-robust` runs the default
seed and seeds 1–31. `NET_ROBUST_SEED=<n> ./build/net-robust-test` explores
others: 2000 consecutive seeds (1000–2999) ran clean when the test was
added. A failure prints its seed for reproduction.

## Running everything

`make test-net-host` runs every network sanitizer test: compatibility, pools,
scheduler, interfaces, routing and ARP, IPv4, ICMP, UDP, TCP, configuration,
this robustness test and the address conversions. All of them are also part of
`make test-unit`.
