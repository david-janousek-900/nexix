# ping and nc (task 428)

`/bin/ping` and `/bin/nc` are small native network commands over the
[socket interface](sockets.md). Addresses are numeric only: there is no
resolver. Configure interfaces first with [`ifconfig` and `route`](netconfig.md);
loopback (127.0.0.1) works as soon as the stack has started.

## ping

```
ping [-c count] [-i seconds] [-W seconds] [-s size] address
```

`ping` sends ICMP echo requests from a raw socket and prints each reply:

```
PING 10.0.2.2: 56 data bytes
64 bytes from 10.0.2.2: icmp_seq=0 ttl=255 time=0 ms
64 bytes from 10.0.2.2: icmp_seq=1 ttl=255 time=10 ms
--- 10.0.2.2 ping statistics ---
2 packets transmitted, 2 packets received, 0.0% packet loss
round-trip min/avg/max = 0/5/10 ms
```

**Options:**

| Option | Range | Default | Meaning |
|---|---|---|---|
| `-c` | 1–65535 | without it, until interrupted | Number of requests |
| `-i` | 1–60 s | 1 s | Interval between requests |
| `-W` | 1–60 s | 1 s | How long to wait for replies after the last request |
| `-s` | 0–1472 bytes | 56 | Data bytes per request; 1472 fills one Ethernet frame |

**Replies and errors:**
- Replies are matched by the process's identifier.
- Late replies are still counted, and duplicates are marked `(DUP!)`.
- Destination-unreachable and time-exceeded errors that quote one of our
  requests are printed as `From address icmp_seq=n reason`.
- Times are in milliseconds at the kernel's 10 ms clock resolution, so
  loopback replies usually show 0.

**Interrupting:** Ctrl-C (`SIGINT`) stops `ping` and prints the statistics.

**Privilege:** raw sockets are root's. `ping` is installed setuid root, as in
BSD. It opens its socket first and immediately gives up root (setting its
uid to the real one), before parsing any reply.

**Exit status:**

| Status | Meaning |
|---|---|
| 0 | At least one reply |
| 1 | No reply |
| 2 | A usage error, a bad address, or a failed send, such as `ping: sendto: Network is unreachable` without a route |

## nc

```
nc [-u] [-w seconds] address port       connect
nc -l [-u] [-w seconds] port            listen on every address, for one peer
```

`nc` copies standard input to one TCP (or, with `-u`, UDP) peer, and the
peer's data to standard output. There is no `select` system call, so a child
process copies standard input while the parent copies the socket.

**TCP:**
- **End of input** half-closes the connection, so the peer reads end of stream.
- **When the peer closes:**
  - finite input (a file or pipe) is still sent in full;
  - an interactive terminal's input is stopped, so `nc` exits once both
    directions are done.
- **Listening:** `nc -l` sets `SO_REUSEADDR`, accepts one connection and
  closes the listener.

**UDP:**
- Each read of standard input becomes one datagram.
- `nc -u -l` replies to the sender of the first datagram it receives.
- UDP has no end of stream, so without `-w` it runs until interrupted.

**`-w seconds`** (1–3600) bounds three things:
- connecting (`nc: connect: timed out`);
- the wait for a connection or first datagram (`nc: accept: timed out`,
  `nc: recvfrom: timed out`);
- silence on an established connection, after which `nc` exits quietly with
  status 0.

**Signals:** `SIGINT`, `SIGTERM` and `SIGHUP` stop both processes, and `nc`
then dies of that signal. Its socket closes, so the port is free again at
once.

**Errors:** usage errors and malformed addresses or ports exit with status 2.
Network failures print `nc: operation: message` and exit 1, for example
`nc: connect: Connection refused`, or `nc: bind: Permission denied` for an
ordinary user on a port below 1024.

## Validation

`make test-nettools` boots disposable guests at 16 and 128 MiB with an e1000.
As root, and as uid 42 where noted, it runs the installed commands and checks
their output and status:

- **ping over loopback:**
  - three replies in the exact format, with the header, statistics and
    round-trip lines;
  - 1400-byte and zero-byte payloads;
  - a successful setuid ping as uid 42;
  - `Network is unreachable` without a route; usage and range errors; a
    malformed address;
  - Ctrl-C ending an unbounded ping with its statistics.
- **ping through `em0`:** after configuring it, QEMU's gateway 10.0.2.2
  answers both requests, and an absent on-link host gives 100% loss and
  status 1.
- **nc:**
  - TCP in both directions with end of stream, and the shell binary sent
    through and compared with `cmp`;
  - UDP both ways, including the listener replying to its first sender;
  - a refused connect, accept and connect timeouts, and usage, port and
    address errors;
  - a reserved-port bind as uid 42;
  - a listener killed by `SIGTERM` (shell status 143) whose port is free for
    the next listener.
