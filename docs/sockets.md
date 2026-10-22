# Socket ABI and libc interfaces (task 424)

Task 424 defines the user-facing socket interface. It assigns the numbers,
fixes the record layouts and supplies the libc headers and wrappers, and it
validates every argument at the kernel boundary. Task 425 backs descriptors
with real sockets and implements connection setup and stream I/O; see
[Descriptors](#descriptors-task-425). Task 426 completes
`send`/`recv`/`sendto`/`recvfrom`, the options, datagram and raw I/O, flags,
timeouts and non-blocking use of [UDP](networking-udp.md),
[TCP](networking-tcp.md) and [raw ICMP](networking-icmp.md).

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
descriptor is. Outputs are preflighted for writability before the operation
runs.

- **socket:** a domain other than `AF_INET` is `EAFNOSUPPORT`; an unknown type
  is `EPROTOTYPE`. A protocol other than 0 or the type's own (TCP for stream,
  UDP for datagram) is `EPROTONOSUPPORT`; raw sockets require `IPPROTO_ICMP`.
  A raw socket from a caller whose effective uid is not 0 is `EACCES`, as in
  BSD `rip_usrreq`. A valid request returns a new descriptor.
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

## Descriptors (task 425)

**Objects.** A socket descriptor is an ordinary open file whose private
object (`kernel/net/socket.c`) wraps one TCP, UDP or raw ICMP control block.
Objects come from the 16-page socket budget (112 objects). The file has read
and write access and a native kind bit, so:

- `fork` and `dup` share it, and close-on-exec (`FD_CLOEXEC`, `FIOCLEX`)
  applies;
- the protocol block is released on the **final** close, whether by `close`,
  `exec` or process exit;
- `fstat` reports `S_IFSOCK` (0140000) with mode 0600; `lseek` is `ESPIPE`;
  ioctls other than close-on-exec are `ENOTTY`;
- `nx_kinfo` reports file kind `NX_KINFO_FILE_SOCKET` (10).

**The stack.** The first `socket` starts the network stack if nothing has. Its
worker becomes a child of the bootstrap process, so no user process ever waits
for it, and `lo0` gets 127.0.0.1/8. External interfaces stay unconfigured
until tasks 427–429.

**Waiting.** Every protocol operation runs on the network worker. A caller
that must wait sleeps interruptibly on its socket. Protocol events (data,
space, connection changes, errors) bump an event counter and wake it, and the
caller rechecks the counter before sleeping, so no wakeup is lost. A signal
returns `EINTR`, or the bytes already moved.

**Limits.**

| Limit | Error |
|---|---|
| 20 descriptors per process | `EMFILE` |
| 175 open files system-wide | `ENFILE` |
| 32 TCP, 32 UDP or 8 raw control blocks, or 112 socket objects | `ENOBUFS` |

`socket` and `accept` check the descriptor tables before allocating, and give
back what they allocated if a table filled while they slept.

**Calls implemented:**

- **bind:** a privileged reserved port (below 1024) needs effective uid 0
  (`EACCES`). An address that is not ours is `EADDRNOTAVAIL`, a busy port
  `EADDRINUSE`, a second bind `EINVAL`.
- **listen:** stream sockets only (`EOPNOTSUPP`). The backlog is clamped to
  1–8, and listening again changes it. A connected socket is `EINVAL`.
- **accept:** blocks until a connection is queued. It returns a new descriptor
  and publishes the peer address (truncated to the given length; the full
  length, 16, is reported). A non-listening socket is `EINVAL`.
- **connect** on a stream blocks until the handshake ends:
  - `ECONNREFUSED`, `ETIMEDOUT`, `ENETUNREACH` or `EHOSTUNREACH` on failure;
  - `EINTR` leaves the attempt running, and a repeated connect is then
    `EALREADY`, and `EISCONN` once connected;
  - on a datagram or raw socket it sets the peer, and `AF_UNSPEC` disconnects.
- **shutdown:** `SHUT_WR` sends FIN after buffered data. `SHUT_RD` discards
  what is buffered and what arrives. An unconnected stream is `ENOTCONN`.
- **getsockname / getpeername:** an unconnected peer is `ENOTCONN`.
- **read / write on a stream:**
  - a write blocks until everything is buffered; partial counts are returned
    if interrupted;
  - a read returns what has arrived, blocking only for the first byte, and 0
    at end of stream (FIN, `SHUT_RD`, or after a reset has been reported);
  - a pending reset is `ECONNRESET` once;
  - after that, or after `SHUT_WR`, a write is **`EPIPE` and raises
    `SIGPIPE`**;
  - an unconnected stream is `ENOTCONN`.
- **Datagram and raw** `read`/`write`, `send`, `recv`, `sendto`, `recvfrom`,
  `setsockopt` and `getsockopt` validate their arguments and then return
  `EOPNOTSUPP` (read/write) or `ENOSYS` until task 426.

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
- **`make test-socket-api`** (task 425) boots normally at 16 and 128 MiB and
  runs `tests/socket-api.c` as root over loopback:
  - **Exhaustion:** `EMFILE`, and `ENOBUFS` at exactly 32 TCP blocks held by
    three processes, over three rounds with equal totals, so exits return
    every block.
  - **Transfer:** a 200000-byte transfer from a forked child sharing the
    descriptor, ending in EOF when its final close sends FIN.
  - **Descriptors:** a `dup` survives closing the original; close-on-exec
    closes the socket across `exec` while another survives as `S_ISSOCK`;
    process exit closes sockets so the peer reads EOF.
  - **End of stream and pipes:** half-close in both directions; `EPIPE` with
    a caught `SIGPIPE`, and a default `SIGPIPE` killing a child; a close with
    unread data giving `ECONNRESET` once, then `EPIPE`.
  - **Errors:** refused connects, `EISCONN`, address and port errors, root
    reserved ports and raw sockets, and their `EACCES` for uid 42; `fstat`,
    `lseek` and ioctl; the calls deferred to task 426; datagram
    connect/disconnect and truncated names.
  - **Interruption:** `EINTR` from `alarm` in `accept` and `read`.
  - A final transfer still works after all of the above.

  The libc self-test keeps its boundary checks but never creates a socket,
  because the boot self-test starts and stops the stack itself.
