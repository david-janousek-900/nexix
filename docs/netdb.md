# Network databases (task 445)

`<netdb.h>` supplies services, protocols and network entry iteration, lookup,
rewind and close functions, plus h_errno/herror/hstrerror. `/etc/services`,
`/etc/protocols` and `/etc/networks` retain the pinned Lite2 distribution files.
These databases work without networking or a DNS server. Names and aliases
are case-sensitive, as in Lite2. Service ports are network-order; protocol
numbers and 32-bit network numbers are host-order, including on LP64.

The native parser bounds physical lines to 1023 bytes, consumes rejected
lines completely, skips malformed records and allows 35 aliases. Numeric
ports must be 0–65535 and protocols 0–255. This deliberately rejects the
historic atoi wrapping of invalid values. Network components must fit a byte.
Results use independent static storage per database, valid until its next
call; the APIs are not reentrant. set*ent rewinds and ORs stayopen, lookup
closes unless stayopen is set, and end*ent closes and clears that flag.
Missing files and I/O errors return NULL with errno from the underlying I/O.

`inet_network`, `inet_makeaddr`, `inet_lnaof` and `inet_netof` provide classful
IPv4 helpers using fixed-width values. `<netdb.h>` also reserves the host
lookup declarations, owned by tasks 446 and 451 until implemented.

`make test-netdb-unit` checks production code with ASan/UBSan and a scripted
stdio fixture; `make test-header-abi` asserts LP64 record layouts. The libc
ring-3 suite checks errors and conversions without starting the network stack.

`make test-netdb-guest` reads the installed BSD databases in disposable
16/128 MiB guests with no NIC, through the production libc.
