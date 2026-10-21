#include "net/mbuf.h"

/* One record owns a cluster page; each half has its own reference count. */
struct nx_net_cluster {
    uint8_t *page;
    struct nx_net_cluster *prev, *next;   /* Partial list linkage. */
    uint32_t refs[2];
};

/* Volatile stores keep the compiler from emitting memcpy, which the
 * freestanding kernel does not provide. */
static void copy(void *to, const void *from, size_t length)
{
    volatile uint8_t *d = to;
    const uint8_t *s = from;
    while (length--) *d++ = *s++;
}
static uint8_t *base(const struct nx_net_mbuf *m)
{
    return m->ext ? m->ext->page + m->ext_half * NX_NET_MCLBYTES : (uint8_t *)m->internal;
}
static size_t capacity(const struct nx_net_mbuf *m)
{
    return m->ext ? NX_NET_MCLBYTES : NX_NET_MLEN;
}
/* A shared cluster's spare bytes may be claimed by another sharer. */
static bool writable(const struct nx_net_mbuf *m)
{
    return !m->ext || m->ext->refs[m->ext_half] == 1;
}
static size_t cost(const struct nx_net_mbuf *m)
{
    return NX_NET_MSIZE + (m->ext ? NX_NET_MCLBYTES : 0);
}

static void partial_remove(struct nx_net_mbuf_pool *pool, struct nx_net_cluster *c)
{
    if (c->prev) c->prev->next = c->next; else pool->partial = c->next;
    if (c->next) c->next->prev = c->prev;
    c->prev = c->next = NULL;
}
static void partial_push(struct nx_net_mbuf_pool *pool, struct nx_net_cluster *c)
{
    c->prev = NULL;
    c->next = pool->partial;
    if (c->next) c->next->prev = c;
    pool->partial = c;
}
static void cluster_unref(struct nx_net_mbuf_pool *pool, struct nx_net_cluster *c, uint32_t half)
{
    if (--c->refs[half]) return;
    --pool->stats.clusters;
    ++pool->stats.cluster_frees;
    if (c->refs[!half]) { partial_push(pool, c); return; }
    partial_remove(pool, c);
    nx_net_budget_release(pool->budget, pool->ops, c->page);
    nx_net_slab_free(&pool->objects, c);
}

bool nx_net_mbuf_pool_init(struct nx_net_mbuf_pool *pool, const struct nx_net_page_ops *ops,
                           struct nx_net_budget *budget, struct nx_net_mbuf_limits limits)
{
    size_t records = limits.clusters / 2 + (limits.clusters & 1);
    if (!limits.mbufs || limits.mbufs > SIZE_MAX - records) return false;
    *pool = (struct nx_net_mbuf_pool){ .ops = ops, .budget = budget, .limits = limits };
    return nx_net_slab_init(&pool->objects, ops, budget, NX_NET_MSIZE, limits.mbufs + records);
}
bool nx_net_mbuf_pool_destroy(struct nx_net_mbuf_pool *pool)
{
    if (pool->stats.mbufs || pool->stats.clusters) return false;
    return nx_net_slab_destroy(&pool->objects);
}

struct nx_net_mbuf *nx_net_mbuf_get(struct nx_net_mbuf_pool *pool, bool pkthdr)
{
    struct nx_net_mbuf *m = pool->stats.mbufs < pool->limits.mbufs ?
        nx_net_slab_alloc(&pool->objects) : NULL;
    if (!m) { ++pool->stats.failures; return NULL; }
    *m = (struct nx_net_mbuf){ .pool = pool, .flags = pkthdr ? NX_NET_M_PKTHDR : 0 };
    m->data = m->internal;
    if (++pool->stats.mbufs > pool->stats.peak_mbufs) pool->stats.peak_mbufs = pool->stats.mbufs;
    ++pool->stats.mbuf_allocs;
    return m;
}
bool nx_net_mbuf_clget(struct nx_net_mbuf *m)
{
    struct nx_net_mbuf_pool *pool = m->pool;
    if (m->ext || m->len || pool->stats.clusters >= pool->limits.clusters) goto fail;
    struct nx_net_cluster *c = pool->partial;
    uint32_t half;
    if (c) {
        half = c->refs[0] ? 1 : 0;
        partial_remove(pool, c);
    } else {
        if (!(c = nx_net_slab_alloc(&pool->objects))) goto fail;
        if (!(c->page = nx_net_budget_page(pool->budget, pool->ops))) {
            nx_net_slab_free(&pool->objects, c);
            goto fail;
        }
        c->prev = c->next = NULL;
        c->refs[0] = c->refs[1] = 0;
        half = 0;
        partial_push(pool, c);
    }
    c->refs[half] = 1;
    m->ext = c;
    m->ext_half = half;
    m->data = base(m);
    if (++pool->stats.clusters > pool->stats.peak_clusters)
        pool->stats.peak_clusters = pool->stats.clusters;
    ++pool->stats.cluster_allocs;
    return true;
fail:
    ++pool->stats.failures;
    return false;
}
struct nx_net_mbuf *nx_net_mbuf_free(struct nx_net_mbuf *m)
{
    struct nx_net_mbuf_pool *pool = m->pool;
    struct nx_net_mbuf *next = m->next;
    if (m->ext) cluster_unref(pool, m->ext, m->ext_half);
    nx_net_slab_free(&pool->objects, m);
    --pool->stats.mbufs;
    ++pool->stats.mbuf_frees;
    return next;
}
void nx_net_mbuf_freem(struct nx_net_mbuf *m)
{
    while (m) m = nx_net_mbuf_free(m);
}
void nx_net_mbuf_drop(struct nx_net_mbuf *m)
{
    if (!m) return;
    ++m->pool->stats.drops;
    nx_net_mbuf_freem(m);
}

size_t nx_net_mbuf_leading(const struct nx_net_mbuf *m)
{
    return writable(m) ? (size_t)(m->data - base(m)) : 0;
}
size_t nx_net_mbuf_trailing(const struct nx_net_mbuf *m)
{
    return writable(m) ? capacity(m) - (size_t)(m->data - base(m)) - m->len : 0;
}
size_t nx_net_mbuf_length(const struct nx_net_mbuf *m)
{
    size_t length = 0;
    for (; m; m = m->next) length += m->len;
    return length;
}

bool nx_net_mbuf_append(struct nx_net_mbuf *head, const void *data, size_t length)
{
    const uint8_t *bytes = data;
    struct nx_net_mbuf *tail = head;
    while (tail->next) tail = tail->next;
    size_t room = nx_net_mbuf_trailing(tail), first = room < length ? room : length;
    struct nx_net_mbuf *added = NULL, **link = &added;
    for (size_t left = length - first; left;) {
        struct nx_net_mbuf *m = nx_net_mbuf_get(head->pool, false);
        if (!m || (left > NX_NET_MLEN && !nx_net_mbuf_clget(m))) {
            if (m) nx_net_mbuf_free(m);
            nx_net_mbuf_freem(added);
            return false;
        }
        m->len = left < capacity(m) ? left : capacity(m);
        left -= m->len;
        *link = m;
        link = &m->next;
    }
    copy(tail->data + tail->len, bytes, first);
    tail->len += first;
    bytes += first;
    for (struct nx_net_mbuf *m = added; m; m = m->next) {
        copy(m->data, bytes, m->len);
        bytes += m->len;
    }
    tail->next = added;
    if (head->flags & NX_NET_M_PKTHDR) head->pkt_len += length;
    return true;
}
bool nx_net_mbuf_copydata(const struct nx_net_mbuf *m, size_t offset, size_t length, void *out)
{
    uint8_t *to = out;
    for (; m && offset >= m->len; m = m->next) offset -= m->len;
    for (; m && length; m = m->next, offset = 0) {
        size_t take = m->len - offset < length ? m->len - offset : length;
        copy(to, m->data + offset, take);
        to += take;
        length -= take;
    }
    return !length;
}
struct nx_net_mbuf *nx_net_mbuf_copym(const struct nx_net_mbuf *source, size_t offset, size_t length)
{
    size_t total = nx_net_mbuf_length(source);
    if (offset > total || length > total - offset) return NULL;
    struct nx_net_mbuf *result = NULL, **link = &result;
    const struct nx_net_mbuf *m = source;
    bool pkthdr = source->flags & NX_NET_M_PKTHDR;
    do {   /* A zero-length copy still yields one empty buffer. */
        while (length && offset >= m->len) { offset -= m->len; m = m->next; }
        size_t take = length && m->len - offset < length ? m->len - offset : length;
        struct nx_net_mbuf *n = nx_net_mbuf_get(source->pool, pkthdr);
        if (!n) { nx_net_mbuf_freem(result); return NULL; }
        if (pkthdr) n->pkt_len = length;
        pkthdr = false;
        if (take && m->ext) {
            n->ext = m->ext;
            n->ext_half = m->ext_half;
            ++n->ext->refs[n->ext_half];
            n->data = m->data + offset;
        } else if (take) {
            copy(n->data, m->data + offset, take);
        }
        n->len = take;
        *link = n;
        link = &n->next;
        length -= take;
        offset += take;
    } while (length);
    return result;
}
bool nx_net_mbuf_adj_head(struct nx_net_mbuf *head, size_t length)
{
    if (length > nx_net_mbuf_length(head)) return false;
    if (head->flags & NX_NET_M_PKTHDR) head->pkt_len -= length;
    size_t take = head->len < length ? head->len : length;
    head->data += take;
    head->len -= take;
    length -= take;
    while (length) {
        struct nx_net_mbuf *m = head->next;
        if (m->len <= length) {
            length -= m->len;
            head->next = nx_net_mbuf_free(m);
        } else {
            m->data += length;
            m->len -= length;
            length = 0;
        }
    }
    return true;
}
bool nx_net_mbuf_adj_tail(struct nx_net_mbuf *head, size_t length)
{
    size_t total = nx_net_mbuf_length(head);
    if (length > total) return false;
    size_t keep = total - length;
    if (head->flags & NX_NET_M_PKTHDR) head->pkt_len -= length;
    struct nx_net_mbuf *m = head;
    while (keep > m->len) { keep -= m->len; m = m->next; }
    m->len = keep;
    nx_net_mbuf_freem(m->next);
    m->next = NULL;
    return true;
}
bool nx_net_mbuf_pullup(struct nx_net_mbuf **chain, size_t length)
{
    struct nx_net_mbuf *head = *chain;
    if (head->len >= length) return true;
    if (length > NX_NET_MLEN || length > nx_net_mbuf_length(head)) return false;
    bool pkthdr = head->flags & NX_NET_M_PKTHDR;
    struct nx_net_mbuf *n = nx_net_mbuf_get(head->pool, pkthdr);
    if (!n) return false;
    nx_net_mbuf_copydata(head, 0, length, n->data);
    n->len = length;
    n->pkt_len = head->pkt_len;
    n->nextpkt = head->nextpkt;
    head->nextpkt = NULL;
    head->flags &= ~NX_NET_M_PKTHDR;
    nx_net_mbuf_adj_head(head, length);
    n->next = head->len ? head : nx_net_mbuf_free(head);
    *chain = n;
    return true;
}
bool nx_net_mbuf_prepend(struct nx_net_mbuf **chain, size_t length)
{
    struct nx_net_mbuf *head = *chain;
    if (length > NX_NET_MLEN) return false;
    if (nx_net_mbuf_leading(head) >= length) {
        head->data -= length;
        head->len += length;
        if (head->flags & NX_NET_M_PKTHDR) head->pkt_len += length;
        return true;
    }
    bool pkthdr = head->flags & NX_NET_M_PKTHDR;
    struct nx_net_mbuf *n = nx_net_mbuf_get(head->pool, pkthdr);
    if (!n) return false;
    n->data = n->internal + NX_NET_MLEN - length;
    n->len = length;
    n->pkt_len = head->pkt_len + length;
    n->nextpkt = head->nextpkt;
    head->nextpkt = NULL;
    head->flags &= ~NX_NET_M_PKTHDR;
    n->next = head;
    *chain = n;
    return true;
}
void nx_net_mbuf_cat(struct nx_net_mbuf *a, struct nx_net_mbuf *b)
{
    if (!b) return;
    struct nx_net_mbuf *tail = a;
    while (tail->next) tail = tail->next;
    if (a->flags & NX_NET_M_PKTHDR) a->pkt_len += nx_net_mbuf_length(b);
    b->flags &= ~NX_NET_M_PKTHDR;
    b->nextpkt = NULL;
    tail->next = b;
}

void nx_net_sockbuf_init(struct nx_net_sockbuf *sb, size_t hiwat, size_t mbmax)
{
    *sb = (struct nx_net_sockbuf){ .hiwat = hiwat, .mbmax = mbmax };
}
size_t nx_net_sockbuf_space(const struct nx_net_sockbuf *sb)
{
    size_t bytes = sb->hiwat - sb->cc, buffers = sb->mbmax - sb->mbcnt;
    return bytes < buffers ? bytes : buffers;
}
static bool admit(struct nx_net_sockbuf *sb, struct nx_net_mbuf *m, size_t *bytes, size_t *charge)
{
    *bytes = *charge = 0;
    for (const struct nx_net_mbuf *n = m; n; n = n->next) {
        *bytes += n->len;
        *charge += cost(n);
    }
    if (*bytes > sb->hiwat - sb->cc || *charge > sb->mbmax - sb->mbcnt) {
        ++sb->drops;
        nx_net_mbuf_drop(m);
        return false;
    }
    sb->cc += *bytes;
    sb->mbcnt += *charge;
    m->nextpkt = NULL;
    return true;
}
bool nx_net_sockbuf_append_stream(struct nx_net_sockbuf *sb, struct nx_net_mbuf *m)
{
    size_t bytes, charge;
    if (!m) return true;
    if (!admit(sb, m, &bytes, &charge)) return false;
    if (sb->last) nx_net_mbuf_cat(sb->last, m);
    else sb->head = sb->last = m;
    return true;
}
bool nx_net_sockbuf_append_record(struct nx_net_sockbuf *sb, struct nx_net_mbuf *m)
{
    size_t bytes, charge;
    if (!m) return true;
    if (!admit(sb, m, &bytes, &charge)) return false;
    if (sb->last) sb->last->nextpkt = m;
    else sb->head = m;
    sb->last = m;
    return true;
}
bool nx_net_sockbuf_consume(struct nx_net_sockbuf *sb, size_t length)
{
    if (length > sb->cc) return false;
    while (sb->head && (length || !sb->head->len)) {
        struct nx_net_mbuf *m = sb->head, *after = m->nextpkt;
        if (m->len > length) {
            m->data += length;
            m->len -= length;
            if (m->flags & NX_NET_M_PKTHDR) m->pkt_len -= length;
            sb->cc -= length;
            return true;
        }
        length -= m->len;
        sb->cc -= m->len;
        sb->mbcnt -= cost(m);
        struct nx_net_mbuf *next = m->next;
        if (next) {
            /* The successor becomes the record head. */
            if (m->flags & NX_NET_M_PKTHDR) {
                next->flags |= NX_NET_M_PKTHDR;
                next->pkt_len = m->pkt_len - m->len;
            }
            next->nextpkt = after;
            if (sb->last == m) sb->last = next;
            sb->head = next;
        } else {
            sb->head = after;
            if (sb->last == m) sb->last = NULL;
        }
        m->next = NULL;
        nx_net_mbuf_free(m);
    }
    return true;
}
struct nx_net_mbuf *nx_net_sockbuf_take_record(struct nx_net_sockbuf *sb)
{
    struct nx_net_mbuf *record = sb->head;
    if (!record) return NULL;
    sb->head = record->nextpkt;
    if (!sb->head) sb->last = NULL;
    record->nextpkt = NULL;
    for (const struct nx_net_mbuf *m = record; m; m = m->next) {
        sb->cc -= m->len;
        sb->mbcnt -= cost(m);
    }
    return record;
}
void nx_net_sockbuf_flush(struct nx_net_sockbuf *sb)
{
    while (sb->head) nx_net_mbuf_freem(nx_net_sockbuf_take_record(sb));
}
