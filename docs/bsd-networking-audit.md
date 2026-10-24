# 4.4BSD-Lite2 networking baseline (task 412)

The reference is a **selected source subset**, not a complete BSD distribution
or a working Nexix port. It is retained under `reference/bsd-lite2/source/`;
active adaptations belong outside that tree. No BSD code is linked into Nexix
by this task. V7 reference bytes and inventory component counts are unchanged.

## Origin and integrity

Upstream mirror: [sergev/4.4BSD-Lite2](https://github.com/sergev/4.4BSD-Lite2),
pinned to commit `50587b00e922225c62f1706266587f435898126d`.
This is a mirror snapshot, not a claim of independently authenticated CSRG media.
The archive URL names that immutable commit; its SHA-256 is
`5a851582c4c3a5053ef2f4f41df347996969a95c3c7236b0b9e99319ff02c3ef`.
The full downloaded archive is not committed.

`reference/bsd-lite2/provenance.json` records the archive prefix, URL/hash,
commit, and every selected member's path, byte count, SHA-256, original mode
and type. All **167 files** are regular archive members copied byte for byte
(102 for networking, 12 layout files and 53 [resolver inputs](resolver-audit.md)):
39 `netinet` files, 17 Ethernet/loopback/routing/raw-socket `net` files, six
`kern/uipc_*` files, 36 supporting `sys` headers, three i386 headers and the
distribution `COPYRIGHT`. The explicit selection is also pinned in the checker.
There are no imported symlinks or binaries.
Narrow Git attributes disable newline conversion and whitespace diagnostics for
the pristine source and license copies; historical whitespace is part of their
verified bytes. Native tools and adaptations retain normal whitespace checks.

Offline verification requires no download:

```sh
make test-bsd-reference
make test-inventory
```

To independently compare the committed subset and index against the archive:

```sh
curl -fL https://codeload.github.com/sergev/4.4BSD-Lite2/tar.gz/50587b00e922225c62f1706266587f435898126d -o /tmp/bsd-lite2.tar.gz
python3 tools/bsd_reference.py /tmp/bsd-lite2.tar.gz
```

The tool verifies the full archive hash before parsing, rejects unsafe/duplicate
paths and non-regular selected entries, and compares the derived index with the
committed index. `--import-reference` fills missing reference files but refuses
to replace differing bytes, including provenance. `--write-audit` regenerates
the lexical include ledger after reference verification. Neither option is used
by tests. Original ownership/timestamps are not needed to build this source-only
subset; file modes/types are provenance, not host checkout requirements.

## Dependency and adaptation decisions

`reference/bsd-lite2/audit.json` enumerates every retained C/header file, its
owning/supporting task IDs, licensing disposition and literal includes. Includes
are classified as retained references, native adapters or deferred features.
This is a lexical audit including conditional branches, not a claim of a
compilable or transitive dependency closure. The retained BSD headers describe
dependencies; they must not replace Nexix kernel or public headers wholesale.

| Subsystem | Dependency / porting decision | Tasks |
|---|---|---|
| Types and machine headers | Replace BSD `sys/types.h`, `param.h` and `machine/*` assumptions with private native compatibility types; no BSD CPU/VM implementation is imported. Missing i386 ansi/endian/limits headers are upstream cross-architecture symlinks and will be replaced by native definitions. | 413 |
| Packet memory | `uipc_mbuf.c` uses `mb_map`, `kmem_malloc`, cluster reference counts and `splimp`; replace VM allocation with bounded Nexix pools and retain explicit ownership. | 414 |
| Execution and timers | Replace `splnet`/`splimp`/`splx`, soft network interrupts, BSD sleep/wakeup and protocol fast/slow timers with serialized worker processing, native waits and timer adaptation. Preserve caller interrupt state. | 415 |
| Interfaces and routing | Adapt `ifnet`, interface queues, radix routing, route references and ARP callouts; e1000 is a new native driver, not historical BSD hardware code. | 416–420, 427 |
| Protocols | Adapt `in_proto` domain/protosw registration, checksums, PCB lookup, ICMP/raw, UDP and TCP. The retained IGMP/multicast-routing sources support reference study; multicast/IGMP and multicast routing are deferred. | 419–423 |
| Socket/file layer | `uipc_socket*` and `uipc_syscalls` depend on BSD proc/ucred, filedesc, uio, copyin/copyout, signals and privilege checks. Use Nexix descriptors, checked user access and credentials; translate errno, ioctl and socket constants. | 424–427 |
| Configuration and observability | Replace sysctl/kernel globals, logging, wall-clock assumptions and statistics export with appropriate native facilities; configuration uses the planned privileged ioctls. | 413, 415, 427 |
| Disabled dependencies | Exclude NS, ISO, CCITT/LLC, IMP/HY, BPF, BSD ktrace/compat ioctls and their configuration-generated headers. Retained routing sockets and local-domain definitions are reference dependencies, not promised public families. | 413, 418, 424 |

Additional missing includes (`vm/vm.h`, `libkern/libkern.h`, BSD buf/mount/stat,
tty ioctl and syscallargs headers, plus `stdlib.h` in non-kernel radix code)
must be removed, replaced or narrowly adapted when their consumers are ported.
Do not import the BSD filesystem, VM, scheduler or syscall table to satisfy them.
The per-file ledger lists the exact include names and consumers.

## LP64 and wire-format audit

These are the baseline adaptation points. Task 413 supplies the private
[type, wire and checksum primitives](networking-compat.md); subsequent ports
must adopt them in their consumers. Mbuf and protocol overlays remain reference
code until their owning tasks implement native replacements:

- `netinet/in.h` uses `u_long` for IPv4 addresses and `tcp.h` uses it for
  `tcp_seq`; make protocol addresses and sequence arithmetic explicitly 32-bit.
  Audit TCP window/timestamp arithmetic separately from native counters.
- `ip_var.h` embeds native pointers in `ipovly` and `ipasfrag`; `tcpip.h` and
  `tcp_var.h` rely on packet overlays and reassembly links sharing offsets.
  Separate queue metadata from wire headers; widening pointers cannot preserve
  historical overlays. Check pseudoheader lengths and fragment bounds.
- `netinet/in_cksum.c` assumes a four-byte `long` in its reduction union and
  casts pointers to `int` for alignment. Replace with fixed-width accumulation
  and `uintptr_t`, testing odd lengths, unaligned data and split mbuf words.
- `sys/mbuf.h` uses pointer masking in `dtom`, cluster address arithmetic and
  `sizeof(long)` alignment. Recompute native mbuf geometry, alignment and pool
  ownership; do not retain historic constants without layout assertions.
- IP/TCP bitfields depend on byte-order macros and compiler layout. Encode and
  decode wire bytes explicitly or prove exact field offsets/sizes with tests;
  never serialize a native pointer-bearing structure.
- Socket address lengths/families, `socklen_t`, timeval, iovec, ioctl encoding,
  buffer accounting and errno values require explicit translation at the ABI
  boundary. Audit narrowing to `int`/`short` and overflow before allocation.
- Replace old-style declarations and implicit prototype assumptions in active
  ports; audit varargs, pointer casts, checksum aliasing and signed overflow.
  Validate TCP sequence wrap, timer wrap and resource-failure paths in later
  protocol tests, not by merely compiling the reference sources.

## Licensing review

Every retained C/header file begins with a UC Regents redistribution notice;
the offline test verifies that the notice remains present and all bytes match
the recorded source. The historical `COPYRIGHT` is retained untouched and copied
verbatim to `LICENSES/BSD-4.4-Lite2.txt`. File-specific conditions and disclaimers
remain authoritative; the Nexix root license does not relicense these files.
`sys/lock.h` credits Carnegie-Mellon lineage in its UC notice; no separate CMU
grant or additional copyright header appears in this selected file.

The [FreeBSD-hosted Berkeley copyright page](https://www.freebsd.org/copyright/license/)
reproduces the July 22, 1999 UC Berkeley amendment removing the Berkeley
advertising acknowledgement requirement. A hash-pinned copy is preserved as
`reference/bsd-lite2/berkeley-license.html`, with its URL/hash in provenance.
Keep source notices and binary redistribution materials, disclaimers and
non-endorsement conditions. The amendment does not remove unrelated contributors'
conditions or the separate Caldera terms covering Nexix's V7 portions.

No new reference file is installed into guest images by task 412. Later active
ports must carry their source notices and package applicable license materials
before distributing binaries; importing inputs does not complete those ports.
