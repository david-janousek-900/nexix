#ifndef NEXIX_NET_MBUF_H
#define NEXIX_NET_MBUF_H
#include "net/pool.h"

/* Native mbufs adapted from 4.4BSD uipc_mbuf.c. Fixed 256-byte headers with
 * internal storage, and 2048-byte reference-counted clusters, two per page.
 * Every chain has exactly one owner. A function that consumes a chain says so;
 * all others leave ownership with the caller and roll back on failure.
 */
#define NX_NET_MSIZE 256u
#define NX_NET_MLEN (NX_NET_MSIZE - 64u)
#define NX_NET_MCLBYTES 2048u
#define NX_NET_M_PKTHDR 1u

struct nx_net_cluster;
struct nx_net_mbuf_pool;
struct nx_net_mbuf {
    struct nx_net_mbuf *next;      /* Next buffer in this packet. */
    struct nx_net_mbuf *nextpkt;   /* Next packet in a queue (head only). */
    uint8_t *data;
    size_t len;
    size_t pkt_len;                /* Packet total; valid with NX_NET_M_PKTHDR. */
    struct nx_net_cluster *ext;    /* NULL: data lives in internal[]. */
    struct nx_net_mbuf_pool *pool;
    uint32_t flags, ext_half;
    uint8_t internal[NX_NET_MLEN];
};
_Static_assert(sizeof(struct nx_net_mbuf) == NX_NET_MSIZE, "mbuf header geometry");

struct nx_net_mbuf_limits { size_t mbufs, clusters; };
struct nx_net_mbuf_stats {
    uint64_t mbuf_allocs, mbuf_frees, cluster_allocs, cluster_frees;
    uint64_t failures, drops;
    size_t mbufs, clusters, peak_mbufs, peak_clusters;
};
struct nx_net_mbuf_pool {
    const struct nx_net_page_ops *ops;
    struct nx_net_budget *budget;
    struct nx_net_slab objects;          /* mbufs and cluster records */
    struct nx_net_cluster *partial;      /* Records with one free half. */
    struct nx_net_mbuf_limits limits;
    struct nx_net_mbuf_stats stats;
};

bool nx_net_mbuf_pool_init(struct nx_net_mbuf_pool *, const struct nx_net_page_ops *,
                           struct nx_net_budget *, struct nx_net_mbuf_limits);
/* Fails without freeing anything while mbufs or clusters remain allocated. */
bool nx_net_mbuf_pool_destroy(struct nx_net_mbuf_pool *);

struct nx_net_mbuf *nx_net_mbuf_get(struct nx_net_mbuf_pool *, bool pkthdr);
/* Attach a cluster to an empty, cluster-less mbuf. */
bool nx_net_mbuf_clget(struct nx_net_mbuf *);
/* Free one buffer and return its successor. */
struct nx_net_mbuf *nx_net_mbuf_free(struct nx_net_mbuf *);
void nx_net_mbuf_freem(struct nx_net_mbuf *);
/* Consume a chain that could not be delivered and count it as a drop. */
void nx_net_mbuf_drop(struct nx_net_mbuf *);

size_t nx_net_mbuf_leading(const struct nx_net_mbuf *);
size_t nx_net_mbuf_trailing(const struct nx_net_mbuf *);
size_t nx_net_mbuf_length(const struct nx_net_mbuf *);
/* Append bytes, adding mbufs/clusters as needed. Failure restores the chain. */
bool nx_net_mbuf_append(struct nx_net_mbuf *, const void *data, size_t length);
bool nx_net_mbuf_copydata(const struct nx_net_mbuf *, size_t offset, size_t length, void *out);
/* New chain for [offset, offset+length); clusters are shared, not copied. */
struct nx_net_mbuf *nx_net_mbuf_copym(const struct nx_net_mbuf *, size_t offset, size_t length);
/* Trim bytes from the head (emptied non-head buffers are freed) or tail. */
bool nx_net_mbuf_adj_head(struct nx_net_mbuf *, size_t length);
bool nx_net_mbuf_adj_tail(struct nx_net_mbuf *, size_t length);
/* Make the first length (<= NX_NET_MLEN) bytes contiguous in *chain. */
bool nx_net_mbuf_pullup(struct nx_net_mbuf **chain, size_t length);
/* Reserve length (<= NX_NET_MLEN) bytes before the data of *chain. */
bool nx_net_mbuf_prepend(struct nx_net_mbuf **chain, size_t length);
/* Append chain b to chain a; consumes b. Packet headers merge into a. */
void nx_net_mbuf_cat(struct nx_net_mbuf *a, struct nx_net_mbuf *b);

/* Socket buffer accounting from BSD sb_cc/sb_mbcnt with hard limits. Records
 * are linked by nextpkt; a stream append extends the last record.
 */
struct nx_net_sockbuf {
    struct nx_net_mbuf *head, *last;
    size_t cc, mbcnt, hiwat, mbmax;
    uint64_t drops;
};
void nx_net_sockbuf_init(struct nx_net_sockbuf *, size_t hiwat, size_t mbmax);
size_t nx_net_sockbuf_space(const struct nx_net_sockbuf *);
/* Both consume m: on refusal it is dropped and counted, and false returned. */
bool nx_net_sockbuf_append_stream(struct nx_net_sockbuf *, struct nx_net_mbuf *m);
bool nx_net_sockbuf_append_record(struct nx_net_sockbuf *, struct nx_net_mbuf *m);
/* Consume length bytes from the front; length must not exceed cc. */
bool nx_net_sockbuf_consume(struct nx_net_sockbuf *, size_t length);
/* Detach and return the first record, ownership passing to the caller. */
struct nx_net_mbuf *nx_net_sockbuf_take_record(struct nx_net_sockbuf *);
void nx_net_sockbuf_flush(struct nx_net_sockbuf *);
#endif
