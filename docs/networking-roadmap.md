# Nexix 0.2.0 networking roadmap

Agreed direction: directly adapt **4.4BSD-Lite2** networking to Nexix. This
replaces the previous lwIP proposal. All milestones below are **To do**;
this document records planned work, not implemented capabilities. They are
future-release milestones outside the numbered 0.1.0 inventory task totals in
[tasks.md](tasks.md). Each milestone requires implementation, focused tests,
affected regressions, documentation and a commit before completion.

## Implementation milestones

Implement in dependency order; driver work can proceed alongside stack adaptation.

- **Source baseline and compatibility layer:** pin the upstream source snapshot,
  origin and hashes; preserve an untouched reference copy and keep active ports
  outside it. Retain file-specific licenses and document the
  [Berkeley license amendment](https://www.freebsd.org/copyright/license/).
  Audit integer/pointer widths, alignment, wire layouts and byte order for LP64.
- **Packet memory and execution:** adapt mbufs and socket buffers using bounded,
  page-backed pools with explicit allocation failures and drop counters. Serialize
  stack operations using Nexix's single-core interrupt discipline. Interrupt
  handlers acknowledge hardware, queue bounded work and wake a schedulable kernel
  worker; adapt BSD protocol timers without sleeping in interrupt callbacks.
- **PCI and e1000:** discover QEMU's e1000 device; implement MMIO mappings, DMA
  rings, interrupts, link state and bounded receive/transmit queues. Boot normally
  when no supported NIC is present.
- **IPv4 protocols and routing:** adapt Ethernet, loopback, ARP, ICMP echo/errors,
  IPv4 fragmentation/reassembly, UDP and TCP, including protocol control blocks
  and retransmission timers. Support static addresses/netmasks, connected routes
  and one configurable default gateway.
- **Socket descriptors:** integrate BSD socket state with Nexix file descriptors
  and interruptible sleep/wakeup. Preserve fork/dup sharing, close-on-exec,
  final-close cleanup, partial transfers, EOF, half-close and SIGPIPE behavior.
- **Configuration and tools:** add root-controlled socket ioctls, `ifconfig`,
  `route`, numeric-address `ping` and a small TCP/UDP `nc`. Raw ICMP sockets
  require root. Read optional `/etc/netstart` during startup; its absence leaves
  external networking unconfigured. Install commands, headers and documentation
  through the existing manifest and staging system.
- **QEMU integration and acceptance:** provide an explicit e1000 user-network
  profile with configurable host forwarding. Document setup, limits and test
  commands; complete the acceptance scenarios below before declaring support.

## Public interfaces

Expose libc headers and wrappers for `socket`, `bind`, `listen`, `accept`,
`connect`, `shutdown`, `send`, `recv`, `sendto`, `recvfrom`, `getsockname`,
`getpeername`, `setsockopt` and `getsockopt`. Support `AF_INET`, `SOCK_STREAM`
and `SOCK_DGRAM`, plus ordinary descriptor `read`, `write` and `close`.

Append syscall numbers and networking errno values without changing existing
assignments. Define fixed-width Nexix socket address records and translate at
the BSD boundary; do not expose upstream kernel structures as the native ABI.
Document layouts and argument validation in the syscall ABI documentation.
Support `SO_REUSEADDR`, socket buffer sizes, `SO_ERROR` and send/receive timeouts;
reject unsupported families, options and flags explicitly.

## Acceptance criteria

- Host tests cover buffer ownership, allocation exhaustion, checksums,
  malformed/truncated packets, fragment bounds, timer expiry and address
  conversion. Run applicable sanitizer checks.
- Disposable QEMU guests at **16 and 128 MiB** cover loopback, static interface
  setup, ARP, ping, UDP, outbound TCP and inbound TCP through host forwarding.
  Use isolated host services without public Internet dependencies.
- Exercise loss/retransmission, fragmentation, refusal, timeout, half-close,
  reset, signals interrupting blocking calls, descriptor exhaustion, fork/dup
  sharing and process-exit cleanup. Repeated transfers return allocations to
  baseline after protocol teardown and timer expiry.
- Check boot without a NIC and run affected build, inventory, syscall, process,
  file, signal and console regressions. Use disposable disks for guest tests.

## Initial scope and defaults

Target one e1000 interface on single-core QEMU i440fx, with the existing IDE
and VGA/serial baseline. DHCP, DNS resolution, IPv6, nonblocking sockets,
select/poll, physical hardware and remote login services are deferred.
TCP/IP remains a 0.2.0 feature; V7 serial communications and UUCP remain in 0.1.0.
