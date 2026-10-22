# TCP (task 423)

`kernel/include/net/tcp.h` and `kernel/net/tcp.c` adapt 4.4BSD
`netinet/tcp_input.c`, `tcp_output.c`, `tcp_timer.c`, `tcp_subr.c` and
`tcp_usrreq.c` as fresh native code under Nexix's root BSD-3-Clause license.
TCP is IP protocol 6 on [IPv4](networking-ip.md) and takes error notices from
[ICMP](networking-icmp.md). It runs in [network-worker](networking-worker.md)
context, never sleeps, and **every function that takes an mbuf chain consumes
it**.

## Scope

The state machine is RFC 793's eleven states:

- **Options.** Only MSS is sent and parsed. There is no window scaling,
  timestamps, SACK, urgent data or keepalive.
- **Windows** are therefore at most 65535 bytes, and every received segment is
  acknowledged at once (no delayed ACK).
- **Sockets.** User-space access (`socket`, blocking waits, options) arrives
  with tasks 424–426.

## Control blocks

A fixed table of 32 blocks holds every connection. That includes listeners,
embryonic and queued children, and connections still closing after their
owner let go. An owner keeps its block until it calls `close` or `abort`. A
connection that fails stays allocated as `CLOSED`, with its error pending.

- **Bind** follows [UDP](networking-udp.md): once only, our address or any,
  privileged ports below 1024, `EXISTS` on overlap unless both sockets set
  reuse. Closing connections (for example in `TIME_WAIT`) keep their port, as
  in BSD.
- **Ephemeral ports** are round-robin from 49152–65535.
- **Initial sequence numbers** follow RFC 6528: a 4 µs clock plus a keyed
  64-bit hash of the addresses and ports. The secret comes from the cycle
  counter and timer at `net_start`.
- **Buffers.** Send and receive buffers hold 16384 bytes each, with mbuf
  accounting capped at four times that.

## Opening

- **Active.** `connect` needs a unicast address and nonzero port. It binds
  an ephemeral port when the socket is unbound, takes the route's source
  address, and refuses a duplicate connection (`EXISTS`). Then it sends a SYN
  with MSS = route MTU − 40. The SYN is retried after 1, 2, 4, 8, 16 and
  32 s, and the connect fails `TIMED_OUT` 64 s later. Inside that window, a
  reset, or ICMP port or protocol unreachable, fails it as `REFUSED`. An
  unacceptable SYN-ACK is reset. Simultaneous open is handled.
- **Passive.** `listen` takes a backlog clamped to 1–8. Each SYN creates an
  embryonic child that answers with a SYN-ACK. A repeated SYN resends it. A
  SYN beyond the backlog, or with the table full, is dropped silently
  (`listen_drops`), and the peer retries. A child becomes acceptable once its
  handshake completes, and the listener's event fires. An embryonic child
  that never completes is freed after its SYN-ACK retries. Closing a listener
  resets its pending children.
- **MSS** is the lesser of the peer's option (536 without one) and our route
  MTU − 40, never below 64.

## Sending

`write` copies what fits into the send buffer:

- it returns `AGAIN` when the buffer is full and `NOT_CONNECTED` before
  establishment;
- after `shutdown` it is `INVALID`, and a pending error is returned instead.

Output follows `tcp_output`:

- the usable window is the lesser of the peer's window and the congestion
  window;
- full segments go at once;
- **Nagle** holds a short tail while data is unacknowledged, unless no-delay
  is set (`TCP_NODELAY` arrives in task 426);
- a FIN follows the last data byte after `shutdown`.

**Retransmission** uses the RFC 6298 estimator, with Karn's rule: one timed
segment at a time, never a retransmitted one.

- The RTO starts at 1 s and is held between 200 ms and 64 s, doubling on each
  timeout.
- After 12 retransmissions (BSD `TCP_MAXRXTSHIFT`) the connection is reset
  and fails `TIMED_OUT`. A soft ICMP error seen meanwhile is reported instead
  (for example `HOST_UNREACHABLE`).

**Congestion control** is Reno (RFC 5681):

- the initial window is min(4·MSS, max(2·MSS, 4380));
- slow start runs to the slow-start threshold, then congestion avoidance;
- three duplicate ACKs retransmit at once and enter fast recovery;
- a timeout collapses the window to one segment.

**Zero windows** are probed with the first unacknowledged byte after 5 s, then
at doubling intervals up to 60 s (BSD `TCPTV_PERSMIN`/`PERSMAX`), until the
window opens. The send window is taken from segments by RFC 793's
`SND.WL1`/`SND.WL2` rule, which also lets the peer shrink it.

## Receiving

A segment must have a 20–60 byte header within the datagram (`too_short`,
`bad_offset`) and a valid checksum over the pseudoheader (`bad_checksum`). Its
source must be unicast and its destination not a broadcast. Then:

- **Resets.** A segment for no connection is answered with a reset (RFC 793
  rules), unless it is itself a reset.
- **RFC 5961.** A reset is accepted only at exactly `rcv_nxt`. A reset
  elsewhere in the window, or any SYN on a synchronized connection, draws a
  challenge ACK instead (`challenge_acks`). An accepted reset fails the
  connection with `RESET`, or `REFUSED` during the handshake.
- **Window trimming.** Data already received is trimmed (`duplicates`), as is
  data beyond the advertised window (`out_of_window`). A segment entirely
  outside is dropped and acknowledged; that is also how zero-window probes
  are answered.
- **Reordering.** Out-of-order segments are held, trimmed against each other,
  up to 16 per connection (`reordered`, `reass_drops`). The bytes they hold
  shrink the advertised window. They are delivered as soon as the gap fills.
- **Window.** The window advertised is the free receive space and never
  shrinks. Reading sends an update once it has opened by two segments or half
  the buffer.
- **`read`** returns buffered bytes; then OK with 0 bytes at end of stream;
  then the pending error. Before anything arrives it returns `AGAIN`.

## Closing

- **`shutdown`** sends FIN after buffered data. The connection goes to
  `FIN_WAIT_1` (or `LAST_ACK` from `CLOSE_WAIT`), and reading continues: it
  is a half-close.
- **`TIME_WAIT`** lasts 60 s (2 × BSD's 30 s MSL) and restarts when a
  retransmitted FIN arrives. `CLOSING` covers simultaneous close.
  `LAST_ACK` frees the block once our FIN is acknowledged.
- **`close`** gives the block up:
  - unread received data (RFC 2525), a listener, or an unfinished connect is
    reset at once;
  - otherwise the connection finishes in the background, and data that
    arrives after close resets the peer (BSD `SS_NOFDREF`);
  - a peer that never sends its FIN is dropped from `FIN_WAIT_2` after 60 s.
- **`abort`** resets at once.

## ICMP errors

UDP-style notices reach the connection that sent the quoted segment, once its
sequence number checks against the unacknowledged range (RFC 5927):

- **Fragmentation needed** lowers the MSS to MTU − 40 and retransmits.
- **Port or protocol unreachable** during connect fails it with `REFUSED`.
- **Other errors** are soft, as RFC 1122 requires: they are only reported if
  the connection later times out.

## Live stack

`net_start` initializes TCP with the stack clock and secret. It registers
protocol 6 and its ICMP notice handler, and arms the 100 ms timer that drives
retransmission, probes and the wait states. `net_stop` cancels the timer,
frees every connection and unregisters both.

## Validation

`make test-net-tcp` runs ASan/UBSan fixtures with a fake Ethernet peer, `lo0`
and a manual clock. They cover:

- **Opening:** SYN contents and MSS negotiation; refused, unacceptable and
  unanswered connects (each retry interval, then `TIMED_OUT`); passive opens,
  a lost SYN-ACK, backlog drops, embryonic expiry, and resets for listeners
  and closed ports; listener close.
- **Data:** Nagle and no-delay; retransmission backoff and stop; fast
  retransmit; zero-window persist probes and recovery.
- **Receiving:** in-order, reordered, overlapping and duplicate segments; a
  full receive buffer closing the window, and reads reopening it; the send
  buffer bound; bad checksums, offsets and lengths; RFC 5961 challenges and an
  exact reset.
- **Closing:**
  - active close with a lost FIN, half-close reading, `TIME_WAIT` expiry and
    restart;
  - passive close with data after the peer's FIN, `LAST_ACK` and freeing;
  - simultaneous close, a reset for unread data, the `FIN_WAIT_2` timeout,
    and a reset for data after close;
  - established-connection timeout.
- **ICMP:** a refused connect, an MSS cut by needfrag, and a soft error
  reported on timeout.
- **Limits:** the 32-block table and `bind`/`connect` validation.
- **Initial sequence numbers:** they differ across connections and advance
  with the clock.
- **Loopback transfers:** 150000 bytes are sent each way and echoed, through
  16344-byte and then 1460-byte segments, ending in `TIME_WAIT`. The runs are
  clean, with every 7th packet dropped, with every 3rd packet delayed past
  its successor, and with both (every 5th dropped, every 4th delayed). The
  lossy runs needed 40–51 retransmissions and the reordered ones queued
  46–63 out-of-order segments, and all data arrived intact.

Every chain must be freed exactly once, with zero live pages at the end.

`make test-net-tcp-guest` (the same boots as `test-pci-guest`) runs over `lo0`
in every start/stop round:

- an unprivileged bind of port 7 is denied;
- a client sends 64 KiB to an accepted connection, which echoes it back;
- both sides shut down, and the client ends in `TIME_WAIT` holding no buffers.

Every mbuf and cluster is returned (`NET TCP OK`).
