# Private networking compatibility foundation (task 413)

`kernel/include/net/` and `kernel/net/compat.c` provide the first active
foundation for the BSD networking port. They are fresh native implementations,
licensed under Nexix's root BSD-3-Clause license. The pinned BSD reference tree
remains unchanged. These helpers compile into the kernel but do not initialize
a stack, attach hardware or expose sockets.

## Types, byte order and wire records

All interfaces use the `nx_net_` prefix. There are no global historical
`u_long`, `tcp_seq`, `sys/types.h` or machine-header replacements. A ported
consumer includes these private headers and changes its historical dependencies
explicitly rather than putting the BSD headers on the normal include path.

| Historical assumption | Native contract |
|---|---|
| `u_long` IPv4 address or TCP sequence | `nx_net_addr_t` / `nx_net_seq_t`, exactly 32 bits, host order |
| Port numbers | `nx_net_port_t`, 16 bits, host order |
| Native length / pointer arithmetic | `size_t` / `uintptr_t`, 64 bits on supported x86_64 |
| Long-lived counters and time bookkeeping | Explicit `uint64_t`; protocol-specific wrap/timer logic remains with its owning task |
| IP/TCP pointer overlays and bitfields | Pointer-free byte-array wire records; queue metadata lives in separate native structures |
| BSD socket records, errno and ioctl numbers | Explicit future ABI translation in tasks 424–427; no aliases to upstream numeric values |

`net/types.h` asserts the supported LP64 widths. `net/wire.h` describes Ethernet,
Ethernet/IPv4 ARP, base IPv4/TCP/UDP/ICMP and IPv4 transport pseudoheaders.
Wire records have byte alignment and asserted sizes/critical offsets, with no
packing attributes, bitfields or native pointer overlays. IPv4/TCP options remain
separately bounded bytes after the base header.

`nx_net_get16/get32` and `nx_net_put16/put32` decode/encode big-endian octets
without unaligned scalar loads or strict-aliasing assumptions. Callers must
validate buffer lengths first; these helpers are not packet parsers. Private
`htons/htonl/ntohs/ntohl` helpers convert scalar storage on the supported
little-endian target. Neither wire records nor private types define a user ABI.

## Arithmetic and checksum primitives

Sequence comparisons use unsigned 32-bit modular distances, avoiding signed
overflow and out-of-range signed conversions. Ordering is meaningful within a
window smaller than half the sequence space. Values exactly `2^31` apart are
unordered in both directions; equal values satisfy `le` and `ge`. Future TCP
callers must enforce the half-range window invariant.

Checked size addition, multiplication and power-of-two alignment return false
on overflow or invalid alignment and leave the output unchanged. Output pointers
must be valid. These functions support later pool geometry and packet-length
checks without silently narrowing native sizes.

The streaming one's-complement checksum carries odd bytes across chunk
boundaries, folds carries before its accumulator can overflow, and accepts
unaligned data. Empty chunks may have a null pointer. Initialization is required;
callers must not modify accumulator internals. `finish` is non-destructive and
returns a host-order checksum to store with `nx_net_put16`; verifying a packet
including its checksum returns zero. The pseudoheader helper takes host-order
IPv4 addresses, a protocol octet and a 16-bit transport length. It appends bytes
to the same stream; normally append it before the transport header/payload.
UDP's zero-checksum semantics and zero-to-`ffff` encoding belong to task 422.

## Remaining dependencies and validation

The [task 412 audit](bsd-networking-audit.md) remains the source/dependency
baseline. Task 413 resolves the primitive type, byte-order, checksum and wire
representation hazards. BSD VM allocation, mbuf geometry and queue ownership
are replaced by the [task 414 pools](networking-pools.md); interrupt levels, waits and timers remain task 415. Protocol
control blocks and reassembly metadata will use these types in tasks 418–423.
Nexix credentials, copyin/copyout, descriptors and errors will be adapted when
their socket consumers are implemented. No permissive allocator, locking,
privilege, random-number or user-copy stubs are supplied.

`make test-net-compat` compiles each private header independently with kernel
flags and no host system headers, compiles the kernel object, and runs host
fixtures under AddressSanitizer and UndefinedBehaviorSanitizer. It is included
in `test-unit`. Fixtures cover known checksum vectors, exact IPv4 encoding,
byte-order storage, sequence wrap/half-range boundaries, size overflow, every
split of short odd/even packets at multiple alignments, byte-at-a-time streams,
large carry-heavy inputs, pseudoheaders and corrupt packets. A separate wide
whole-buffer checksum oracle checks streaming results; exact packet allocations
make boundary overreads visible to ASan.
