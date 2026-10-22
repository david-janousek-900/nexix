# Socket ABI and libc interfaces (task 424)

Task 424 defines the user-facing socket interface. It assigns the numbers,
fixes the record layouts and supplies the libc headers and wrappers, and it
validates every argument at the kernel boundary. Socket descriptors themselves
arrive with task 425, and the blocking operations and options that drive
[UDP](networking-udp.md), [TCP](networking-tcp.md) and
[raw ICMP](networking-icmp.md) arrive with task 426. Until then, any call whose
arguments are valid ends with `ENOTSOCK`/`EBADF` (or `ENOSYS` for `socket`).

## Numbers

**Syscalls 77–88** are appended after `timedpause` (76). No existing number
changes, and 89 upward stays `EINVAL`.

| Number | Call | Arguments |
|---|---|---|
| 77 | socket | domain, type, protocol |
| 78 | bind | fd, readable address, length |
| 79 | listen | fd, backlog |
| 80 | accept | fd, writable address or NULL, value-result length |
| 81 | connect | fd, readable address, length |
| 82 | shutdown | fd, how |
| 83 | sendto | fd, readable buffer, length, flags, address or NULL, address length |
| 84 | recvfrom | fd, writable buffer, length, flags, address or NULL, value-result length |
| 85, 86 | getsockname, getpeername | fd, writable address, value-result length |
| 87 | setsockopt | fd, level, name, readable value, length |
| 88 | getsockopt | fd, level, name, writable value, value-result length |

`send` and `recv` are libc wrappers over `sendto`/`recvfrom` with no address,
as in 4.4BSD. Integer arguments use the low 32 bits of their 64-bit slot, as C
`int` does; lengths, pointers and counts use the full slot.

**Errors 36–57** follow `ENOSYS` (35), in 4.4BSD's order, with BSD's
messages in `sys_errlist`. The table now has 58 entries. `EWOULDBLOCK` is
`EAGAIN`, as in BSD.

| Value | Name | Value | Name |
|---|---|---|---|
| 36 | ENOTSOCK | 47 | ENETUNREACH |
| 37 | EDESTADDRREQ | 48 | ECONNABORTED |
| 38 | EMSGSIZE | 49 | ECONNRESET |
| 39 | EPROTOTYPE | 50 | ENOBUFS |
| 40 | ENOPROTOOPT | 51 | EISCONN |
| 41 | EPROTONOSUPPORT | 52 | ENOTCONN |
| 42 | EOPNOTSUPP | 53 | ETIMEDOUT |
| 43 | EAFNOSUPPORT | 54 | ECONNREFUSED |
| 44 | EADDRINUSE | 55 | EHOSTUNREACH |
| 45 | EADDRNOTAVAIL | 56 | EALREADY |
| 46 | ENETDOWN | 57 | EINPROGRESS |

## Records and constants

`<nexix/socket.h>` is the shared kernel/libc contract. It uses fixed-width
native records, never BSD kernel structures (which carry a length byte). Ports
and addresses inside records are in network byte order.

| Record | Size | Layout |
|---|---|---|
| `nx_sockaddr` / `sockaddr` | 16 | family (u16), 14 data bytes |
| `nx_sockaddr_in` / `sockaddr_in` | 16 | family (u16) at 0, port (u16) at 2, address (u32) at 4, 8 zero bytes |
| `nx_sockaddr_storage` / `sockaddr_storage` | 128 | family (u16), then padding; 4-byte aligned |
| `nx_timeval` / `timeval` | 16 | seconds (i64), microseconds (i64) |
| `socklen_t` | 4 | unsigned |

Constant values are 4.4BSD's:

- **Families, types and protocols:** `AF_INET`=2; `SOCK_STREAM`=1,
  `SOCK_DGRAM`=2, `SOCK_RAW`=3; `IPPROTO_ICMP`=1, `TCP`=6, `UDP`=17.
- **Options:** `SOL_SOCKET`=0xffff with `SO_REUSEADDR`=4, `SO_SNDBUF`=0x1001,
  `SO_RCVBUF`=0x1002, `SO_SNDTIMEO`=0x1005, `SO_RCVTIMEO`=0x1006, and the
  read-only `SO_ERROR`=0x1007 and `SO_TYPE`=0x1008; `TCP_NODELAY`=1 at
  `IPPROTO_TCP`.
- **Message flags:** `MSG_PEEK`=2, `MSG_WAITALL`=0x40, `MSG_DONTWAIT`=0x80,
  and `MSG_NOSIGNAL`=0x400 (FreeBSD's later value).
- **Shutdown:** `SHUT_RD`/`WR`/`RDWR`=0/1/2.
- **Backlog:** `SOMAXCONN`=8, the TCP backlog limit.

Anything else is unsupported, and the call says so explicitly (see below).

## Libc headers

| Header | Contents |
|---|---|
| `<sys/socket.h>` | `socklen_t`, `sa_family_t`, `sockaddr`, `sockaddr_storage`, the constants above and the 14 calls |
| `<netinet/in.h>` | `in_port_t`, `in_addr_t`, `in_addr`, `sockaddr_in`, `IPPROTO_*`, host-order `INADDR_ANY`/`LOOPBACK`/`BROADCAST`/`NONE`, `INET_ADDRSTRLEN`, `htonl`/`htons`/`ntohl`/`ntohs` |
| `<netinet/tcp.h>` | `TCP_NODELAY` |
| `<arpa/inet.h>` | `inet_addr`, `inet_aton`, `inet_ntoa`, `inet_pton`, `inet_ntop` |
| `<sys/time.h>` | `struct timeval` only |

There is no resolver: these are numeric IPv4 conversions only.

- **`inet_aton`/`inet_addr`** accept BSD's one-to-four-part forms, each part
  decimal, octal (leading `0`) or hex (`0x`), with the last part filling the
  remaining bytes (`127.1`, `10.0.513`). Trailing whitespace ends the address.
  `inet_addr` returns `INADDR_NONE` on failure.
- **`inet_pton`** accepts strict dotted decimal only: four parts of 0–255
  without leading zeros. Other families fail with `EAFNOSUPPORT`.
- **`inet_ntop`** needs room for the terminator (`ENOSPC`).
- **`inet_ntoa`** uses a static buffer.

**`connect` and the V7 multiplexor.** V7's `<sys/mx.h>` also had a
`connect(fd, channel, side)`. Its library function is now `mpx_connect`, and
`<sys/mx.h>` maps the three-argument `connect` to it with a macro, so V7
sources build unchanged. `<sys/socket.h>` removes that macro before declaring
the socket `connect`. A single source file uses one or the other; whichever
header is included last decides. The parenthesized-declarator alternative is
avoided because the retained PCC front end cannot parse it.

## Argument validation

Every argument is checked, and every input copied in, **before the descriptor
is looked up**. A malformed call therefore fails the same way whatever the
descriptor is. Outputs are preflighted for writability; nothing is written
while descriptors cannot be sockets.

- **socket:** a domain other than `AF_INET` is `EAFNOSUPPORT`; an unknown type
  is `EPROTOTYPE`. A protocol other than 0 or the type's own (TCP for stream,
  UDP for datagram) is `EPROTONOSUPPORT`; raw sockets require `IPPROTO_ICMP`.
  A raw socket from a caller whose effective uid is not 0 is `EACCES`, as in
  BSD `rip_usrreq`. A valid request is `ENOSYS` until task 425.
- **Input addresses** (bind, connect, sendto):
  - the length must be 16..128 (`EINVAL`);
  - the whole length must be readable (`EFAULT`);
  - the family must be `AF_INET` (`EAFNOSUPPORT`). `connect` also accepts
    `AF_UNSPEC`, which disconnects a datagram socket.
  - `sendto` checks its address only when one is given.
- **Output addresses** (accept and recvfrom with an address, getsockname,
  getpeername; the address is mandatory for the last two):
  - the value-result length must be readable and writable (`EFAULT`) and at
    most 128 (`EINVAL`);
  - the address must be writable for the lesser of that length and 16 bytes
    (`EFAULT`). A zero length writes nothing.
- **Data:** flags outside the four supported are `EOPNOTSUPP`. A length above
  `SSIZE_MAX` is `EINVAL`. The buffer must be readable (send) or writable
  (receive) for its whole length (`EFAULT`); a zero-length transfer touches
  nothing.
- **shutdown:** `how` outside 0–2 is `EINVAL`.
- **setsockopt:**
  - an unknown level/name pair, or the read-only `SO_ERROR`/`SO_TYPE`, is
    `ENOPROTOOPT`;
  - a length shorter than the value (4 bytes, or 16 for timeouts) or above 128
    is `EINVAL`;
  - an unreadable value is `EFAULT`;
  - a timeout with negative seconds or microseconds outside 0–999999 is
    `EDOM`, as in BSD;
  - a buffer size of zero or less is `EINVAL`.
- **getsockopt:** an unknown pair is `ENOPROTOOPT`. The value-result length
  must be readable and writable (`EFAULT`) and at least the value's size
  (`EINVAL`), and the value must be writable (`EFAULT`).
- **Descriptors:** a negative, out-of-range or closed descriptor is `EBADF`;
  any other open descriptor is `ENOTSOCK`.

Invalid pointers include NULL, the low supervisor-only identity mapping,
unmapped and noncanonical addresses, and read-only data for outputs. They are
rejected by the existing [user-copy helpers](syscall-abi.md#safe-user-access)
without CPU faults.

## Validation

- **`make test-header-abi`** checks every syscall and errno number, every
  family, type, protocol, option, flag and shutdown constant, and the size and
  offset of every record. `tests/header_coverage.py` compiles the new headers
  in isolation under C11 and gnu89, and
  `tools/check_libc_interfaces.py` requires an export for all 271 public
  declarations.
- **`make test-inet-unit`** (ASan/UBSan) covers the byte-order helpers:
  - every BSD `inet_aton` form, and 16 malformed inputs that leave the output
    untouched;
  - strict `inet_pton` rejections;
  - unsupported families and `inet_ntop` bounds;
  - round trips of every byte value.
- **`make test-libc`** runs ring-3 checks inside the libc self-test, as a
  non-root user:
  - every `socket` error, including the raw-socket `EACCES`;
  - high argument bits being ignored;
  - `EBADF`/`ENOTSOCK` for each call;
  - each address, length, flag, option and range error;
  - `EFAULT` for NULL, kernel, unmapped, noncanonical and read-only pointers,
    and zero-length cases that touch nothing;
  - the error table, and the libc conversions;
  - number 89 staying `EINVAL`.
- **`make test-perror-unit`** and **`make test-syscall-policy-unit`** cover the
  58-entry error table and the moved end of the assigned range.
