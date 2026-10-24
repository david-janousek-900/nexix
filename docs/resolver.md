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
configuration and unpredictable IDs belong to tasks 449 and 448.

`p_type`, `p_class` and `fp_nquery` supply debug output. Always use the
length-bearing `fp_nquery` for received messages. Historic `fp_query` cannot
infer allocation size and is restricted to a complete caller-owned, locally
built single-question QUERY with no resource records. Global `_res` and all
legacy database result storage are non-reentrant.

`make test-dns-codec` checks RFC wire vectors, root/trailing-dot/escaped names,
compression, limits, header bit order, malformed packets and 40,000 seeded
mutations under ASan/UBSan. Native header tests assert wire size and offsets;
the ring-3 libc regression encodes, expands and constructs a DNS query.
