# TCP/IP networking in Nexix 0.2.0

Nexix 0.2.0 has an IPv4 network stack adapted from **4.4BSD-Lite2**:
- an Intel e1000 driver (QEMU's default card) and a loopback interface;
- ARP, static routing, IPv4 with fragmentation, ICMP, UDP and TCP;
- BSD sockets on ordinary file descriptors;
- the `ifconfig`, `route`, `ping` and `nc` commands.

This page is the guide: setup, use, interfaces, limits, installed files and
evidence. The [roadmap](networking-roadmap.md) links each component's own
contract.

## Quick start

```sh
make run-serial                          # an e1000 on QEMU user networking
make run-serial HOSTFWD="tcp:8080:7000"  # also forward host port 8080 to guest port 7000
```

Log in as `root` (password `nexix`), then configure the card:

```sh
/sbin/ifconfig em0 10.0.2.15/24 up
/sbin/route add default 10.0.2.2
ping -c 3 10.0.2.2
```

To configure it at every boot, run `cp /etc/netstart.qemu /etc/netstart`; see
[network startup](netstart.md). QEMU user networking puts the guest at
10.0.2.15, the host (and gateway) at 10.0.2.2, and the host's DNS at
10.0.2.3. Guest connections leave through QEMU's NAT; the host reaches guest
services only through `HOSTFWD` forwards. `make run-serial NIC=none` boots
without a card.

Some things to try:

```sh
ping -c 2 127.0.0.1                       # loopback, which always works
echo hello | nc 10.0.2.2 7777             # TCP to a host service on port 7777
echo hi | nc -u -w 2 10.0.2.2 7778        # UDP
nc -l 7000                                # with HOSTFWD="tcp:8080:7000": host runs nc localhost 8080
```

The administration commands are in `/sbin`, on root's `PATH`
(`/sbin:/bin:/usr/sbin:/usr/bin`), so plain `ifconfig` and `route` work; disks
upgraded from before task 432 keep their old profile, so add `/sbin` there
yourself. Addresses are numeric; there
is no resolver.

## What is supported

| Area | Supported | Contract |
|---|---|---|
| Devices | QEMU e1000 (82540EM) as `em0`, loopback `lo0` | [PCI](networking-pci.md), [e1000](networking-e1000.md), [interfaces](networking-interfaces.md) |
| Addressing | One static IPv4 address/netmask per interface, connected routes, one default gateway, ARP | [routing and ARP](networking-routing.md), [configuration](netconfig.md) |
| IPv4 | Validation, fragmentation, bounded reassembly; no forwarding, options passed up uninterpreted | [IPv4](networking-ip.md) |
| ICMP | Echo replies, unreachable errors, error propagation, root-only raw ICMP sockets | [ICMP](networking-icmp.md) |
| UDP | Datagrams up to 9216 bytes, broadcast receive, port unreachable | [UDP](networking-udp.md) |
| TCP | RFC 793 states, MSS, Nagle, RFC 6298 timers, Reno, persist probes, RFC 5961 checks, TIME_WAIT | [TCP](networking-tcp.md) |
| Sockets | `AF_INET` stream, datagram and raw sockets on descriptors, with fork/dup sharing, close-on-exec, `SIGPIPE`, `O_NONBLOCK`, flags, options and timeouts | [socket ABI](sockets.md) |
| Commands | `ifconfig`, `route`, `ping` (setuid root), `nc` | [configuration](netconfig.md), [ping/nc](nettools.md) |
| Startup | Optional `/etc/netstart`; a make profile for the card and host forwards | [startup](netstart.md) |

**Not supported:**
- IPv6, multicast, IP forwarding, DHCP, DNS or a resolver;
- `select`/`poll`;
- TCP window scaling, timestamps, SACK, urgent data and keepalive;
- dynamic routing, or more than one gateway;
- network cards other than the e1000.

## Programming interface

C programs use the usual BSD headers and calls:

- `<sys/socket.h>`, `<netinet/in.h>`, `<netinet/tcp.h>`, `<arpa/inet.h>` and
  `<sys/time.h>`;
- `socket`, `bind`, `listen`, `accept`, `connect`, `shutdown`, `send`,
  `recv`, `sendto`, `recvfrom`, `getsockname`, `getpeername`, `setsockopt`
  and `getsockopt`;
- plain `read`, `write` and `close`, with `fcntl(F_SETFL, O_NONBLOCK)` for
  non-blocking use.

The native ABI is documented in [sockets](sockets.md):
- syscalls 77–88 and errno 36–57, appended without renumbering anything;
- fixed 16-byte `sockaddr_in` records;
- the exact validation order of every argument.

The configuration ioctls are in [netconfig](netconfig.md). A minimal TCP
client:

```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
int main(void)
{
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(7777) };
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    inet_pton(AF_INET, "10.0.2.2", &a.sin_addr);
    if (fd < 0 || connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0) return 1;
    write(fd, "hello\n", 6);
    return close(fd);
}
```

## Limits

| Resource | Limit | When exceeded |
|---|---|---|
| Packet memory | one eighth of RAM, 32–2048 pages (128 KiB–8 MiB), in 256-byte mbufs and 2 KiB clusters | drops, counted |
| Socket objects | 112 (a 16-page budget of its own) | `ENOBUFS` |
| TCP / UDP / raw control blocks | 32 / 32 / 8 | `ENOBUFS` |
| TCP buffers | 16 KiB send and receive; `SO_SNDBUF`/`SO_RCVBUF` 512 B–256 KiB; windows up to 65535 | |
| TCP listen backlog | 8 | SYNs dropped |
| UDP datagram / receive queue | 9216 bytes / 41600 bytes | `EMSGSIZE` / drops |
| Raw message / receive queue | 8192 bytes / 8192 bytes | `EMSGSIZE` / drops |
| Interfaces | 4 (`lo0` MTU 16384, `em0` MTU 1500) | |
| ARP | 16 entries, 4 held packets each, 16 in all; retries 1 s × 5, then a 20 s negative entry; 20 min expiry | `EHOSTUNREACH` |
| Reassembly | 8 datagrams, 16 fragments each, 64 in all, 30 s | drops, counted |
| Loopback queue | 50 packets | drops, counted |
| Descriptors | 20 per process, 175 open files system-wide | `EMFILE` / `ENFILE` |
| Ephemeral ports | 49152–65535 | `EADDRNOTAVAIL` |
| Timers | TCP 100 ms, IPv4 500 ms, ARP 250 ms; ping/socket timeouts at the 10 ms clock | |

The stack starts when the first socket is opened, with `lo0` at
127.0.0.1/8. External interfaces are configured only by root, or by
`/etc/netstart` at boot.

## Installed files

All of these are in the [installation manifest](native-install.json), so
fresh disks and upgrades get them:

| Path | Mode | Purpose |
|---|---|---|
| `/sbin/ifconfig`, `/sbin/route` | 0755 | Interface and route configuration |
| `/bin/ping` | 4755 (setuid root) | ICMP echo; it gives up root after opening its socket |
| `/bin/nc` | 0755 | TCP/UDP client and listener |
| `/etc/netstart.qemu` | 0644 | Sample startup script; copy it to `/etc/netstart` |
| `/usr/include/sys/socket.h`, `netinet/in.h`, `netinet/tcp.h`, `arpa/inet.h`, `sys/time.h`, `nexix/socket.h` | 0644 | Development headers |
| `libc.a` | | Socket wrappers, byte order and address conversions |

`/etc/netstart` itself is never installed and never replaced by an
[upgrade](disk-upgrades.md).

## Evidence

`make test-tcpip` runs the whole acceptance set below; each target can also
run alone.

| Target | What it shows |
|---|---|
| `make test-net-host` | Every network sanitizer test (ASan/UBSan): compatibility, pools, worker, interfaces, routing/ARP, IPv4, ICMP, UDP, TCP (including lossy and reordered transfers), configuration, address conversions, and the [whole-stack robustness test](networking-robustness.md) over 32 seeds |
| `make test-tcpip-acceptance` | Disposable 16 and 128 MiB guests configured by `/etc/netstart` (see below) |
| `make test-socket-api` | Descriptor sharing across fork/dup, close-on-exec, exit cleanup, EOF, `SIGPIPE`, resets, `EINTR`, exhaustion, every operation, flag, option and timeout (16/128 MiB) |
| `make test-netconfig` | `ifconfig`/`route` output, errors and root-only changes, with and without a NIC |
| `make test-nettools` | `ping`/`nc` behaviour, errors, signals and setuid use |
| `make test-netstart` | Cold boots with and without `/etc/netstart`, with no NIC, and with host forwarding |
| `make test-pci-guest` | Driver DMA, the stack self-tests (loopback, IPv4, ICMP, UDP, TCP, routing) and no-NIC boots at 16/128 MiB |
| `make test-libc`, `test-syscall`, `test-syscall-policy`, `test-process`, `test-files`, `test-signals`, `test-console`, `test-inventory` | The affected build, ABI, process, file, signal, console and inventory regressions |

The acceptance guests cover these scenarios:

- **Loopback:** ping, and TCP through `nc`.
- **Static IPv4 and routing:** the address from `/etc/netstart`, and a route
  lookup through the gateway.
- **ARP and ping:** the gateway resolves and answers; an absent neighbour
  gives 100% loss.
- **Host services:** UDP and TCP echo services that the test starts on the
  host's 127.0.0.1, on random ports:
  - UDP to the host echo service and back;
  - outbound TCP, including `/bin/sh` echoed back and compared with `cmp`;
  - a refused connect.
- **Inbound TCP:** through the host forward, in both directions.
- **Counters:** no errors or drops on `em0` afterwards.

No test uses the public Internet.
