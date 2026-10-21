# Bounded packet and socket pools (task 414)

`kernel/include/net/pool.h`, `net/mbuf.h` and `kernel/net/{pool,mbuf}.c` replace
4.4BSD `uipc_mbuf.c` VM allocation (`mb_map`, `kmem_malloc`, `splimp`) with
fresh native pools, licensed under Nexix's root BSD-3-Clause license.
`net/kernel.h` and `kernel/net/kernel.c` bind them to the physical page
allocator. No network stack is started yet. Tasks 415 and later create the live
pools and serialize access from the network worker.

## Pages, budgets and slabs

A page source supplies page-aligned 4 KiB pages. The kernel source wraps
`page_alloc`/`page_free`. A **budget** puts a hard limit on the pages that one or
more pools may hold. It records current, peak and refused pages. Refusals by the
limit and by the page source count the same way. Pools never sleep and never
fall back to another allocator.

A **slab** hands out fixed objects of 64–2048 bytes (rounded to 64). Slot zero
of each page holds an owner/magic/used-bitmap header. A second limit,
`max_objects`, is enforced independently of pages. Frees reject null, foreign,
misaligned, header-slot and already-free addresses. Pages stay attached to the
slab until `destroy`, which fails without changing anything while objects are
still allocated.

## Mbufs and clusters

| Object | Geometry | Limit |
|---|---|---|
| mbuf | 256 bytes: 64-byte header, 192 bytes internal data | `limits.mbufs` |
| cluster | 2048 bytes, two per page, per-half reference count | `limits.clusters` and budget |
| cluster record | slab object owning one cluster page | derived from `limits.clusters` |

Freeing the last reference to both halves of a cluster page returns the page to
its budget immediately. Every chain has a single owner. `nx_net_mbuf_drop`,
`cat` (its second argument) and socket-buffer appends consume their chain. Every
other operation leaves ownership with the caller and leaves the chain unchanged
if it fails:

- `append` allocates every needed buffer before copying.
- `copym` shares clusters by reference and copies internal data.
- `pullup` and `prepend` allocate before relinking.
- `adj_head` and `adj_tail` free buffers that become empty, but always keep the
  head.

Spare space in a shared cluster is never writable, so two sharers cannot overwrite
each other. Packet headers (`NX_NET_M_PKTHDR`, `pkt_len`) move to whichever buffer
becomes the new head. Pool statistics record allocations, frees, failures, drops,
the current number of mbufs and clusters, and their peaks.

## Socket buffers and socket objects

`nx_net_sockbuf` follows BSD `sb_cc`/`sb_mbcnt` accounting. Each buffer is
charged 256 bytes, plus 2048 if it has a cluster. Both `hiwat` and `mbmax` are
hard limits. A chain that would exceed either limit is dropped whole and counted
in both the socket-buffer and pool drop counters. Records are linked by
`nextpkt`, and a stream append extends the last record. `consume` can cross
record boundaries. `take_record` hands one record to the caller, and `flush`
releases everything.

The kernel limits come from installed memory. Packet pages are one eighth of all
pages, clamped to 32–2048 (128 KiB–8 MiB). Mbuf headers may use half of that
budget, and clusters may use all of it. Socket objects (512-byte placeholders
until task 425 defines the socket structure) have their own 16-page budget of
112 objects, so a packet flood cannot prevent sockets from being created.

## Validation

`make test-net-pool` runs host fixtures under ASan/UBSan with an exact-accounting
page source that can inject failures. They cover:

- slab geometry, limits and invalid frees;
- the cluster half/page life cycle;
- every chain operation over cluster and internal boundaries;
- shared-cluster isolation;
- rollback at every mbuf limit and page budget during append, copy and prepend;
- socket-buffer byte and mbuf limits, drops, record boundaries and consumption
  in arbitrary steps;
- zero live pages after every teardown.

`make test-net-pools-guest` boots the self-test image with 16 MiB and 128 MiB of
RAM. Against the real page allocator, it twice fills clusters until the page
budget runs out, fills mbufs and socket objects up to their limits (sockets while
packets are full), runs chain and socket-buffer operations, and then requires
every page to be back in the allocator. Both passes must print `NET POOLS OK`.
Observed limits are 119 pages, 198 clusters and 885 mbufs at 16 MiB, and 2048
pages, 3412 clusters and 15360 mbufs at 128 MiB.

Remaining limits: callers serialize pool access by running on the
[network worker](networking-worker.md) (task 415). Slab pages
are not returned until teardown. A free looks up the page among the slab's
pages, which takes time proportional to the number of pages.
