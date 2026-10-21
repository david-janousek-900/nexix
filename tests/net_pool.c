#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "net/mbuf.h"

/* Page source with exact accounting and injectable failure. */
struct source { size_t live, fail_after; bool limited; };
static void *page_alloc(void *context)
{
    struct source *s = context;
    if (s->limited && !s->fail_after--) return NULL;
    void *page = aligned_alloc(NX_NET_PAGE_SIZE, NX_NET_PAGE_SIZE);
    assert(page);
    memset(page, 0xa5, NX_NET_PAGE_SIZE);
    ++s->live;
    return page;
}
static void page_free(void *context, void *page)
{
    struct source *s = context;
    assert(s->live);
    --s->live;
    free(page);
}
static struct source source;
static const struct nx_net_page_ops ops = { page_alloc, page_free, &source };

static void pattern(uint8_t *bytes, size_t length, unsigned seed)
{
    for (size_t i = 0; i < length; ++i) bytes[i] = (uint8_t)(i * 31 + seed);
}
static bool chain_equals(const struct nx_net_mbuf *m, const uint8_t *expected, size_t length)
{
    static uint8_t out[16384];
    assert(length <= sizeof(out));
    return nx_net_mbuf_length(m) == length && nx_net_mbuf_copydata(m, 0, length, out) &&
        !memcmp(out, expected, length);
}

static void slab(void)
{
    struct nx_net_budget budget;
    struct nx_net_slab slab;
    nx_net_budget_init(&budget, 2);
    assert(!nx_net_slab_init(&slab, &ops, &budget, 0, 1));
    assert(!nx_net_slab_init(&slab, &ops, &budget, 4096, 1));
    assert(nx_net_slab_init(&slab, &ops, &budget, 100, 100));
    assert(slab.size == 128 && slab.per_page == 31);
    void *objects[62];
    for (size_t i = 0; i < 62; ++i) {
        assert((objects[i] = nx_net_slab_alloc(&slab)));
        memset(objects[i], (int)i, 128);   /* ASan: whole slot is usable. */
    }
    assert(!nx_net_slab_alloc(&slab) && budget.failures == 1 && budget.pages == 2);
    uint8_t *page = (uint8_t *)((uintptr_t)objects[0] & ~(uintptr_t)4095);
    assert(!nx_net_slab_free(&slab, page));              /* header slot */
    assert(!nx_net_slab_free(&slab, (uint8_t *)objects[0] + 8));
    assert(!nx_net_slab_destroy(&slab));
    assert(nx_net_slab_free(&slab, objects[5]) && !nx_net_slab_free(&slab, objects[5]));
    assert(nx_net_slab_alloc(&slab) == objects[5]);
    for (size_t i = 0; i < 62; ++i) assert(nx_net_slab_free(&slab, objects[i]));
    assert(slab.in_use == 0 && slab.peak == 62 && nx_net_slab_destroy(&slab));
    assert(budget.pages == 0 && budget.peak == 2 && source.live == 0);

    /* Explicit object limit, and page-source failure. */
    nx_net_budget_init(&budget, 10);
    assert(nx_net_slab_init(&slab, &ops, &budget, 64, 3));
    void *a = nx_net_slab_alloc(&slab), *b = nx_net_slab_alloc(&slab), *c = nx_net_slab_alloc(&slab);
    assert(a && b && c && !nx_net_slab_alloc(&slab) && slab.failures == 1 && budget.pages == 1);
    assert(nx_net_slab_free(&slab, a) && nx_net_slab_free(&slab, b) && nx_net_slab_free(&slab, c));
    assert(nx_net_slab_destroy(&slab));
    source = (struct source){ .limited = true, .fail_after = 0 };
    assert(nx_net_slab_init(&slab, &ops, &budget, 64, 3) && !nx_net_slab_alloc(&slab));
    assert(budget.pages == 0 && budget.failures == 1 && nx_net_slab_destroy(&slab));
    source = (struct source){ 0 };
}

static struct nx_net_budget budget;
static struct nx_net_mbuf_pool pool;
static void pool_init(size_t pages, size_t mbufs, size_t clusters)
{
    nx_net_budget_init(&budget, pages);
    assert(nx_net_mbuf_pool_init(&pool, &ops, &budget,
                                 (struct nx_net_mbuf_limits){ mbufs, clusters }));
}
static void pool_done(void)
{
    assert(pool.stats.mbufs == 0 && pool.stats.clusters == 0);
    assert(pool.stats.mbuf_allocs == pool.stats.mbuf_frees);
    assert(pool.stats.cluster_allocs == pool.stats.cluster_frees);
    assert(nx_net_mbuf_pool_destroy(&pool));
    assert(budget.pages == 0 && source.live == 0);
}

static void clusters(void)
{
    pool_init(8, 64, 5);
    struct nx_net_mbuf *m[6];
    for (int i = 0; i < 5; ++i) {
        assert((m[i] = nx_net_mbuf_get(&pool, false)) && nx_net_mbuf_clget(m[i]));
        memset(m[i]->data, i, NX_NET_MCLBYTES);
        assert(nx_net_mbuf_trailing(m[i]) == NX_NET_MCLBYTES);
    }
    /* Two clusters per page: 5 clusters use 3 cluster pages plus 1 object page. */
    assert(budget.pages == 4 && pool.stats.clusters == 5);
    assert((m[5] = nx_net_mbuf_get(&pool, false)) && !nx_net_mbuf_clget(m[5]));
    assert(!nx_net_mbuf_clget(m[0]));                         /* already attached */
    assert(!nx_net_mbuf_pool_destroy(&pool));
    nx_net_mbuf_free(m[1]);   /* half of a page frees, page stays */
    assert(budget.pages == 4);
    nx_net_mbuf_free(m[0]);   /* whole page returns */
    assert(budget.pages == 3);
    assert(nx_net_mbuf_clget(m[5]) && budget.pages == 3);   /* reuses C's free half */
    for (int i = 2; i < 6; ++i) nx_net_mbuf_free(m[i]);
    assert(budget.pages == 1 && pool.stats.peak_clusters == 5);
    pool_done();

    /* Cluster page failure rolls back its record. */
    pool_init(1, 8, 8);
    struct nx_net_mbuf *x = nx_net_mbuf_get(&pool, true);
    size_t objects = pool.objects.in_use;
    assert(x && !nx_net_mbuf_clget(x) && pool.objects.in_use == objects && !x->ext);
    assert(pool.stats.failures == 1 && pool.stats.clusters == 0);
    nx_net_mbuf_free(x);
    pool_done();
}

static void chains(void)
{
    static uint8_t data[9000], out[9000];
    pattern(data, sizeof(data), 7);
    pool_init(64, 256, 32);
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    size_t pos = 0;
    const size_t pieces[] = {0, 1, 100, 91, 192, 2047, 3000, 1, 3568};
    for (size_t i = 0; i < sizeof(pieces) / sizeof(pieces[0]); ++i) {
        assert(nx_net_mbuf_append(m, data + pos, pieces[i]));
        pos += pieces[i];
        assert(m->pkt_len == pos && chain_equals(m, data, pos));
    }
    assert(pos == sizeof(data));
    for (size_t off = 0; off < pos; off += 997)
        for (size_t len = 0; off + len <= pos && len < 4000; len += 613) {
            assert(nx_net_mbuf_copydata(m, off, len, out) && !memcmp(out, data + off, len));
            struct nx_net_mbuf *c = nx_net_mbuf_copym(m, off, len);
            assert(c && (c->flags & NX_NET_M_PKTHDR) && c->pkt_len == len);
            assert(chain_equals(c, data + off, len));
            nx_net_mbuf_freem(c);
        }
    assert(!nx_net_mbuf_copydata(m, pos - 1, 2, out) && !nx_net_mbuf_copym(m, pos, 1));
    assert(!nx_net_mbuf_copym(m, SIZE_MAX, 2) && !nx_net_mbuf_copym(m, 1, SIZE_MAX));

    /* Shared clusters: the copy stays intact and writes cannot reach spare space. */
    size_t clusters_before = pool.stats.clusters;
    struct nx_net_mbuf *shared = nx_net_mbuf_copym(m, 0, pos);
    assert(shared && pool.stats.clusters == clusters_before);
    for (struct nx_net_mbuf *n = m; n; n = n->next)
        if (n->ext) assert(nx_net_mbuf_trailing(n) == 0 && nx_net_mbuf_leading(n) == 0);
    assert(nx_net_mbuf_adj_head(m, 300) && nx_net_mbuf_adj_tail(m, 5000));
    assert(m->pkt_len == pos - 5300 && chain_equals(m, data + 300, pos - 5300));
    assert(chain_equals(shared, data, pos));
    assert(!nx_net_mbuf_adj_head(m, pos) && !nx_net_mbuf_adj_tail(m, pos));
    nx_net_mbuf_freem(shared);
    assert(pool.stats.clusters <= clusters_before);

    /* Pullup and prepend preserve bytes and packet headers. */
    assert(nx_net_mbuf_adj_head(m, 150));                 /* head is now short */
    const uint8_t *now = data + 450;
    size_t left = pos - 5450;
    assert(nx_net_mbuf_pullup(&m, 60) && m->len >= 60 && (m->flags & NX_NET_M_PKTHDR));
    assert(m->pkt_len == left && chain_equals(m, now, left));
    assert(!nx_net_mbuf_pullup(&m, NX_NET_MLEN + 1) && chain_equals(m, now, left));
    assert(nx_net_mbuf_prepend(&m, 40) && m->pkt_len == left + 40);
    memcpy(m->data, "0123456789012345678901234567890123456789", 40);
    assert(nx_net_mbuf_prepend(&m, NX_NET_MLEN) && m->pkt_len == left + 40 + NX_NET_MLEN);
    assert(!nx_net_mbuf_prepend(&m, NX_NET_MLEN + 1));
    assert(nx_net_mbuf_adj_head(m, NX_NET_MLEN + 40) && chain_equals(m, now, left));

    struct nx_net_mbuf *tail = nx_net_mbuf_get(&pool, true);
    assert(nx_net_mbuf_append(tail, data, 10));
    nx_net_mbuf_cat(m, tail);
    assert(m->pkt_len == left + 10 && !(tail->flags & NX_NET_M_PKTHDR));
    assert(nx_net_mbuf_copydata(m, left, 10, out) && !memcmp(out, data, 10));
    nx_net_mbuf_freem(m);
    pool_done();
}

/* Every allocation point fails once: the chain must be unchanged and nothing leak. */
static void rollback(void)
{
    static uint8_t data[6000];
    pattern(data, sizeof(data), 3);
    for (size_t limit = 1; limit < 12; ++limit) {
        pool_init(64, limit, 64);
        struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
        assert(nx_net_mbuf_append(m, data, 100));
        uint64_t failures = pool.stats.failures;
        bool ok = nx_net_mbuf_append(m, data + 100, 5000);
        assert(ok == (limit >= 4));
        assert(chain_equals(m, data, ok ? 5100 : 100) && m->pkt_len == (ok ? 5100u : 100u));
        assert(ok || pool.stats.failures > failures);
        struct nx_net_mbuf *c = nx_net_mbuf_copym(m, 50, ok ? 4000 : 50);
        if (c) nx_net_mbuf_freem(c);
        assert(pool.stats.mbufs == 1 + (ok ? 3 : 0));
        if (limit == 1) {
            assert(!nx_net_mbuf_prepend(&m, NX_NET_MLEN) && chain_equals(m, data, 100));
            assert(nx_net_mbuf_adj_head(m, 99) && chain_equals(m, data + 99, 1));
        }
        nx_net_mbuf_freem(m);
        pool_done();
    }
    /* Budget exhaustion through page-source failure during append. */
    for (size_t pages = 1; pages < 6; ++pages) {
        pool_init(pages, 100, 100);
        struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, false);
        bool ok = nx_net_mbuf_append(m, data, sizeof(data));
        assert(ok == (pages >= 3) && chain_equals(m, data, ok ? sizeof(data) : 0));
        nx_net_mbuf_freem(m);
        pool_done();
    }
}

static void sockbufs(void)
{
    static uint8_t data[5000];
    pattern(data, sizeof(data), 11);
    pool_init(64, 128, 16);
    struct nx_net_sockbuf sb;
    nx_net_sockbuf_init(&sb, 4096, 4 * NX_NET_MSIZE + 2 * NX_NET_MCLBYTES);
    assert(nx_net_sockbuf_space(&sb) == 4096);
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    assert(nx_net_mbuf_append(m, data, 3000));
    size_t charged = 0;
    for (struct nx_net_mbuf *n = m; n; n = n->next)
        charged += NX_NET_MSIZE + (n->ext ? NX_NET_MCLBYTES : 0);
    assert(nx_net_sockbuf_append_stream(&sb, m) && sb.cc == 3000 && sb.mbcnt == charged);
    struct nx_net_mbuf *big = nx_net_mbuf_get(&pool, true);
    assert(nx_net_mbuf_append(big, data, 1200));
    assert(!nx_net_sockbuf_append_stream(&sb, big));       /* exceeds hiwat: dropped */
    assert(sb.drops == 1 && pool.stats.drops == 1 && sb.cc == 3000);
    struct nx_net_mbuf *small = nx_net_mbuf_get(&pool, false);
    assert(nx_net_mbuf_append(small, data + 3000, 96));
    assert(nx_net_sockbuf_append_stream(&sb, small) && sb.head->pkt_len == 3096);
    assert(chain_equals(sb.head, data, 3096));
    for (size_t step = 1; sb.cc; step = step * 3 + 1) {
        size_t before = sb.cc, take = step < sb.cc ? step : sb.cc;
        size_t done = 3096 - before;
        assert(nx_net_sockbuf_consume(&sb, take) && sb.cc == before - take);
        if (sb.cc) assert(chain_equals(sb.head, data + done + take, sb.cc) &&
                          sb.head->pkt_len == sb.cc);
    }
    assert(!sb.head && !sb.last && sb.mbcnt == 0 && !nx_net_sockbuf_consume(&sb, 1));

    /* Records keep boundaries; the mbuf-count limit drops independently of hiwat. */
    for (unsigned i = 0; i < 3; ++i) {
        struct nx_net_mbuf *r = nx_net_mbuf_get(&pool, true);
        assert(nx_net_mbuf_append(r, data + i, 10 + i));
        assert(nx_net_sockbuf_append_record(&sb, r));
    }
    struct nx_net_mbuf *many = nx_net_mbuf_get(&pool, true);
    for (int i = 0; i < 20; ++i) {
        struct nx_net_mbuf *e = nx_net_mbuf_get(&pool, false);
        nx_net_mbuf_cat(many, e);
    }
    assert(!nx_net_sockbuf_append_record(&sb, many) && sb.drops == 2);
    assert(nx_net_sockbuf_consume(&sb, 11) && sb.cc == 22);   /* crosses a record */
    struct nx_net_mbuf *r = nx_net_sockbuf_take_record(&sb);
    assert(chain_equals(r, data + 2, 10) && sb.cc == 12 && sb.head == sb.last);
    nx_net_mbuf_freem(r);
    assert(nx_net_sockbuf_append_record(&sb, nx_net_mbuf_get(&pool, true)));
    nx_net_sockbuf_flush(&sb);
    assert(sb.cc == 0 && sb.mbcnt == 0 && !sb.head);
    pool_done();
}

int main(void)
{
    slab();
    clusters();
    chains();
    rollback();
    sockbufs();
    return 0;
}
