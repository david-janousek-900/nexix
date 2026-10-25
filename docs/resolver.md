# Name resolution

## DNS codec (task 447)

`<arpa/nameser.h>` supplies the RFC 1035/BIND constants and a tested 12-byte
x86_64 HEADER. Active parsing reads bytes, avoiding unaligned native overlays.
`dn_comp`, `dn_expand` and `dn_skipname` handle DNS labels and compression;
all message reads take an end pointer. Expansion rejects forward pointers,
cycles, reserved label types, truncated labels and names over 255 wire bytes.
Labels are limited to 63 bytes. Presentation names escape dots, backslashes
and non-printable octets; encoding supports those escapes and a final dot.
Compression tables contain only prior offsets below 16384; failed writes
roll back newly registered pointers. Empty names denote the DNS root.

`_getshort`, `_getlong`, `putshort` and `putlong` operate on complete 2/4-byte
fields; their callers must check available bytes. `res_mkquery` implements
QUERY and IQUERY with bounded output; obsolete update opcodes are rejected.
The global resolver state currently has Lite2's timeout/retry/option defaults;
configuration belongs to task 449. Query IDs now use getentropy (task 448).

`p_type`, `p_class` and `fp_nquery` supply debug output. Always use the
length-bearing `fp_nquery` for received messages. Historic `fp_query` cannot
infer allocation size and is restricted to a complete caller-owned, locally
built single-question QUERY with no resource records. Global `_res` and all
legacy database result storage are non-reentrant.

`make test-dns-codec` checks RFC wire vectors, root/trailing-dot/escaped names,
compression, limits, header bit order, malformed packets and 40,000 seeded
mutations under ASan/UBSan. Native header tests assert wire size and offsets;
the ring-3 libc regression encodes, expands and constructs a DNS query.

## Query IDs (task 448)

Every res_mkquery obtains a fresh 16-bit ID through getentropy; `_res.id` records
the result for compatibility and is not a sequence generator. Failure returns
-1 with the entropy error, before modifying the message. Native syscall 89
uses CPUID-gated RDRAND with bounded retry and no predictable fallback. QEMU
profiles enable RDRAND explicitly. This choice avoids relying on boot-cycle
counters or the weaker historical TCP sequence key; environments without the
capability fail query construction with EIO. DNS transaction IDs alone do not
authenticate DNS: transport also must validate source, ID and question (450).

`make test-entropy-unit test-entropy-guest` checks generation bounds, source
failure, atomic user-buffer behavior, user pointers, query-ID distribution,
repeated calls and fork independence at 16/128 MiB. Guests without RDRAND
verify EIO without altering destination bytes. Codec tests inject entropy to
remain deterministic and also check failure propagation.

## Configuration (task 449)

`res_init` reads optional `/etc/resolv.conf`: up to three IPv4 nameservers,
`domain`, or six search domains. Each directive must be a complete token.
Comments start with # or ; and overlong physical lines are consumed and
ignored. Invalid, unspecified, multicast and broadcast server addresses are
ignored. Later domain/search directives replace earlier ones. LOCALDOMAIN
uses its first word as the overriding domain, matching this Lite2 snapshot.
Absent a configured domain, use the suffix of the full native hostname.
Derived search tries at most three levels, stopping at two components.
An empty domain still provides one empty search suffix so single-label
lookups query a server. No configured server means 127.0.0.1:53. Defaults
are five seconds and four attempts; explicit nonzero retrans/retry fields
are preserved. Configuration and search pointers are global, non-reentrant
and invalidated by reinitialization.

`make test-res-init test-res-init-guest` exercises defaults, invalid/long
lines, server/search limits, overrides and full hostname handling under host
sanitizers and with actual files and syscalls at 16/128 MiB.

## Query transport (task 450)

res_send/res_query/res_search/res_querydomain and HOSTALIASES use blocking
native IPv4 sockets. UDP connects for a single server and otherwise uses
sendto/recvfrom. Every response must come from the queried address and port,
match ID, QR/opcode and the expanded question name/type/class. A fixed
monotonic deadline survives ignored replies and partial TCP reads. Timeout
backoff follows Lite2 (`retrans << attempt`, divided by server count after
the first attempt, minimum one second). Caller timeout/retry fields must
remain within 1–3600 seconds and 1–10 attempts; invalid state returns EINVAL.

Truncation retries on TCP; RES_USEVC also forces TCP. The length prefix and
query share one send buffer; partial I/O is completed within the deadline.
Oversized answers are fully drained, clipped to capacity and marked TC.
One ECONNRESET reconnect/requery is allowed. RES_STAYOPEN keeps the stream;
_res_close releases it, and a forked child opens its own stream on next use.
EINTR aborts immediately. SERVFAIL/NOTIMP/REFUSED skip that server. Only an
all-refused attempt set reports ECONNREFUSED; a mixed refusal/timeout cannot
trigger hosts fallback. NXDOMAIN, NODATA, temporary and permanent failures
map to HOST_NOT_FOUND, NO_DATA, TRY_AGAIN and NO_RECOVERY respectively.
Search follows Lite2's search-first order, trailing-dot handling and
single-label HOSTALIASES. Configuration remains global and non-reentrant.

`make test-resolver-transport` uses a scripted transport under ASan/UBSan,
covering bad IDs/sources/questions, failover, deadlines, reset, partial I/O,
connection reuse, oversized draining, error mapping and search/aliases.
`make test-resolver-guest` exercises the production code against an actual
UDP/TCP server bound to lo0, with disposable 16/128 MiB no-NIC guests.

## Host lookup (task 451)

gethostbyname/gethostbyaddr supply IPv4 A/PTR lookup with CNAME aliases.
The parser checks every record and supported embedded name against the
message end before returning a result, follows up to 35 CNAME links regardless
of record order, rejects loops and ignores unrelated address records. Returned
addresses use typed, aligned in_addr storage. Results expose at most 35
addresses/aliases and use static non-reentrant storage. Numeric digit/dot
names without a final dot use inet_aton directly; invalid numeric names fail
without DNS. PTR results retain the caller's original four address bytes,
including when the caller's input was unaligned.

Lookup is DNS first; only ECONNREFUSED permits hosts fallback. NXDOMAIN,
NODATA, malformed responses and timeout do not. sethostent(1) combines hosts
iteration with RES_USEVC/RES_STAYOPEN; endhostent closes both the hosts stream
and TCP connection and clears these options.

`make test-host-answers` covers CNAME order/loops, PTR, address caps/alignment,
numeric handling, fallback policy and 40,000 seeded answer mutations under
ASan/UBSan. The resolver guest server also exercises multi-address CNAME, PTR,
malformed lengths and real no-server hosts fallback at 16/128 MiB.

The V7 filesystem uses `gethost.3` for the host manual; the man command
maps the five host API names to it, avoiding the 14-byte filename limit.

## nslookup (task 453)

`/usr/sbin/nslookup` adapts BIND 4.9.2's tools/nslookup command interface.
The retained command grammar is `user/nslookup/commands.l`, adapted only for
native declarations and a bounded 2048-byte scanner buffer. The checked-in
`commands.c` is produced by the native Nexix lex implementation, and guest
acceptance regenerates it with the installed native lex and compares bytes.
Native adapters replace the BSD-specific socket, signal and result layers;
no historical select or writev dependency remains.

Command-line and interactive modes support A/PTR host lookup, general DNS
type/class queries, server/lserver, type/class/domain/search/defname/recurse,
retry/timeout/vc/debug options, help and output redirection. The native port
option permits a test/service port. Server selection uses the current or
initial resolver respectively. ls requests AXFR over TCP, prints records and
stops after the closing SOA, validating framing and message bounds. Transfers
are bounded to 4096 messages; reads use SO_RCVTIMEO. SIGINT interrupts input
or a query and returns to the prompt. Legacy finger/view are explicitly
unavailable in this resolver-client scope; help documents the supported set.
The installed help is `/usr/lib/nslookup.help`, and its manual is nslookup(8).

`make test-nslookup` runs an isolated host DNS stub bound only to 127.0.0.1
(reached from the guest as 10.0.2.2). At 16/128 MiB it verifies lexer
reproduction, multi-address CLI lookup, PTR, forced TCP, NXDOMAIN, interactive
settings/server selection, AXFR and a successful query after SIGINT.

## Acceptance and regressions (task 455)

`make test-resolver-acceptance` checks the complete production resolver from
disposable 16/128 MiB guests against two host-loopback DNS fixtures. It covers
multi-address A, out-of-order CNAME chains, PTR, NXDOMAIN/NODATA even when a
hosts entry exists, SERVFAIL/REFUSED, truncation to TCP, wrong ID/source/
question, a silent server with exactly two configured attempts, failover,
search suffixes and both forward/reverse hosts fallback without a server.
The fixtures never use external DNS, bind only 127.0.0.1 and are reached as
10.0.2.2. Host-side assertions inspect actual query sequences.

`make test-resolver-robust` includes 40,000 mutations each for the codec,
answer parser, and full gethostbyname path through res_search, query creation,
reply matching, transport error handling and result parsing. Mutated replies
can be truncated and can corrupt headers, compression and RDATA. The tests
run under ASan/UBSan with deterministic seeds; separate known-answer cases
assert behavior before mutation. `make test-tcpip` includes these suites and
all database/configuration/entropy/guest/nslookup acceptance, alongside the
existing stack and affected kernel/userland regressions.

Native scope adaptations: `/etc/resolv.qemu` replaces the planned 16-byte
sample name; `gethost.3` plus man aliases replaces the overlong manual
filename. Unpredictable query IDs require RDRAND (enabled in QEMU profiles);
no entropy source means EIO. Legacy BIND finger/view commands remain outside
the requested IPv4 name-resolution client scope. Resolver state and result
buffers remain non-reentrant, as in the pinned baseline.

The native host lookup answer buffer is 4096 bytes; a larger TCP answer is
safely drained/truncated and rejected by the host parser. General nslookup
queries and AXFR accept the DNS TCP length range. Interactive lexer input
uses an interruptible native read adapter because ordinary stdio retries
EINTR. Its SIGINT path discards partial scanner input using the kernel's
supported user signal-context/longjmp contract, and leaves resolver resource
cleanup on the normal query-return path. Acceptance verifies interrupts both
at the input prompt and during a query, followed by successful lookups.

Debug/nslookup printers decode MX/AFSDB preferences, SOA fields, quoted
TXT/HINFO strings, related domain-name records, WKS bitmaps and legacy
32-bit identifiers. Known-type RDATA lengths are checked before printing;
unknown types retain their numeric type and bounded byte count. Native
acceptance asserts MX, TXT and SOA values in addition to A/PTR results.

The exposed native LP64 resolver state is 392 bytes: options at offset 8,
server addresses at 20, ID at 68, search pointers at 72, default domain at
128 and print flags at 384. Header ABI checks pin this layout.
