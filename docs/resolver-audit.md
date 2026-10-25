# Resolver reference audit (task 444)

The immutable snapshot and archive hash in [the BSD audit](bsd-networking-audit.md)
initially covered 167 selected files: the earlier 114 plus 53 resolver, database,
public header, manual, configuration and BIND 4.9.2 nslookup inputs. The complete
pinned mirror is now retained; this audit still covers the selected port inputs. Per-file bytes and
metadata are recorded in `reference/bsd-lite2/provenance.json`; the include
ledger includes libc, headers and nslookup. References remain unmodified.

The active port must address these findings:

- `gethostnamadr.c` aligns `bp` through `(u_int32_t)bp`, truncating LP64
  pointers. Use typed address storage or uintptr_t alignment. Its `getanswer`
  advances `cp + n` and RDATA lengths without complete end-of-message checks.
  Bound every field, name expansion and RDATA before accessing it.
- `nameser.h` HEADER bit fields need little-endian x86_64 order. Long-sized
  integer assumptions must become explicit 16/32-bit wire operations.
- `res_mkquery.c` uses sequential `++_res.id`. Replace this with native entropy;
  neither process IDs nor timestamps alone provide unpredictable query IDs.
- `res_send.c` accepts UDP replies without validating source address/port and
  matching the question. Validate these plus ID and response/opcode before
  consuming replies. Replace select with socket receive deadlines and writev
  with a contiguous TCP length-prefixed write; retain partial-I/O handling.
- `netdb.h` declares `gethostent` but no definition is supplied. Implement it
  over the hosts reader. Keep DNS-first lookup with hosts fallback restricted
  to ECONNREFUSED; default the absent nameserver to loopback.
- Database and resolver results use static buffers and global state. Document
  non-reentrancy and invalidation by subsequent calls; bound aliases, records,
  input lines and configuration arrays. Reject invalid hosts addresses rather
  than treating INADDR_NONE as a parsed address.
- nslookup shares resolver parsing/printing and assumes BSD signal, socket,
  select, lex and filesystem interfaces. Adapt these in active sources; never
  compile historical headers over native public ABI definitions.

Each retained source keeps its own notice. In addition to UC Berkeley, resolver
inputs contain Digital Equipment Corporation permission/disclaimer text;
nslookup includes its contributors' notices. The Berkeley amendment affects
Berkeley's advertising clause only. Preserve all applicable notices in active
adaptations and binary distribution materials. No new binary is built by 444.

Verification: `make test-bsd-reference test-inventory` checks hashes, reference
selection, include ownership and malformed archive/import rejection offline.
