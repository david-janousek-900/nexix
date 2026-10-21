#include "net/kernel.h"
#include "console.h"
#include "interrupts.h"
#include "memory.h"
#define REQUIRE(c) do { if (!(c)) panic("network pool self-test: " #c); } while (0)

_Static_assert(NX_NET_PAGE_SIZE == PAGE_SIZE, "network pool page size");

static void *kernel_page(void *context)
{
    (void)context;
    return (void *)page_alloc();
}
static void kernel_page_free(void *context, void *page)
{
    (void)context;
    if (!page_free((uintptr_t)page)) panic("network pool freed a foreign page");
}
const struct nx_net_page_ops nx_net_kernel_pages = { kernel_page, kernel_page_free, NULL };

struct nx_net_kernel_limits nx_net_kernel_limits(size_t total_pages)
{
    size_t pages = total_pages / 8;
    if (pages < 32) pages = 32;
    if (pages > 2048) pages = 2048;
    /* Clusters may fill the whole budget; headers alone may use half of it. */
    return (struct nx_net_kernel_limits){ .packet_pages = pages,
        .socket_objects = NX_NET_SOCKET_PAGES * (NX_NET_PAGE_SIZE / NX_NET_SOCKET_OBJECT_SIZE - 1),
        .mbuf = { .mbufs = pages / 2 * (NX_NET_PAGE_SIZE / NX_NET_MSIZE - 1), .clusters = pages * 2 } };
}

static struct nx_net_budget packets, sockets;
static struct nx_net_mbuf_pool pool;
static struct nx_net_slab socket_slab;

/* Exhaust, verify the reported limit, then release everything. */
static size_t exhaust_clusters(void)
{
    struct nx_net_mbuf *list = NULL, *m;
    size_t count = 0;
    while ((m = nx_net_mbuf_get(&pool, true))) {
        if (!nx_net_mbuf_clget(m)) { nx_net_mbuf_free(m); break; }
        m->data[0] = 0x5a;
        m->data[NX_NET_MCLBYTES - 1] = 0xa5;
        m->nextpkt = list;
        list = m;
        ++count;
    }
    while ((m = list)) { list = m->nextpkt; nx_net_mbuf_free(m); }
    return count;
}
static size_t exhaust_mbufs(void)
{
    struct nx_net_mbuf *list = NULL, *m;
    size_t count = 0;
    while ((m = nx_net_mbuf_get(&pool, false))) { m->nextpkt = list; list = m; ++count; }
    while ((m = list)) { list = m->nextpkt; nx_net_mbuf_free(m); }
    return count;
}

void net_pool_selftest(void)
{
    uint32_t before = page_free_count();
    struct nx_net_kernel_limits limits = nx_net_kernel_limits(page_total_count());
    for (unsigned pass = 0; pass < 2; ++pass) {
        nx_net_budget_init(&packets, limits.packet_pages);
        nx_net_budget_init(&sockets, NX_NET_SOCKET_PAGES);
        REQUIRE(nx_net_mbuf_pool_init(&pool, &nx_net_kernel_pages, &packets, limits.mbuf));
        REQUIRE(nx_net_slab_init(&socket_slab, &nx_net_kernel_pages, &sockets,
                                 NX_NET_SOCKET_OBJECT_SIZE, limits.socket_objects));

        /* The page budget, not the cluster count, bounds a cluster flood. */
        size_t clusters = exhaust_clusters();
        REQUIRE(clusters && clusters < limits.mbuf.clusters);
        REQUIRE(packets.peak == limits.packet_pages && packets.failures);
        REQUIRE(pool.stats.mbufs == 0 && pool.stats.clusters == 0 && pool.stats.failures);
        REQUIRE(exhaust_clusters() == clusters);   /* repeatable, no leak */
        size_t mbufs = exhaust_mbufs();
        REQUIRE(mbufs == limits.mbuf.mbufs);

        /* Socket objects come from their own budget even when packets are full. */
        void *list = NULL, *object;
        size_t count = 0;
        struct nx_net_mbuf *hog = NULL, *m;
        while ((m = nx_net_mbuf_get(&pool, false))) { m->nextpkt = hog; hog = m; }
        while ((object = nx_net_slab_alloc(&socket_slab))) { *(void **)object = list; list = object; ++count; }
        REQUIRE(count == limits.socket_objects && sockets.pages == NX_NET_SOCKET_PAGES);
        while ((object = list)) { list = *(void **)object; REQUIRE(nx_net_slab_free(&socket_slab, object)); }
        while ((m = hog)) { hog = m->nextpkt; nx_net_mbuf_free(m); }

        /* Chain operations and socket-buffer accounting on real pages. */
        static uint8_t bytes[5000], out[5000];
        for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)(i * 7 + pass);
        m = nx_net_mbuf_get(&pool, true);
        REQUIRE(m && nx_net_mbuf_append(m, bytes, sizeof(bytes)) && m->pkt_len == sizeof(bytes));
        struct nx_net_mbuf *copy = nx_net_mbuf_copym(m, 100, 4000);
        REQUIRE(copy && nx_net_mbuf_pullup(&copy, 128) && copy->len >= 128);
        REQUIRE(nx_net_mbuf_copydata(copy, 0, 4000, out));
        for (size_t i = 0; i < 4000; ++i) REQUIRE(out[i] == bytes[100 + i]);
        struct nx_net_sockbuf sb;
        nx_net_sockbuf_init(&sb, 4096, 8 * NX_NET_MCLBYTES);
        REQUIRE(nx_net_sockbuf_append_stream(&sb, copy) && sb.cc == 4000);
        REQUIRE(!nx_net_sockbuf_append_record(&sb, m) && sb.drops == 1 && pool.stats.drops == 1);
        REQUIRE(nx_net_sockbuf_consume(&sb, 1234) && sb.cc == 2766);
        nx_net_sockbuf_flush(&sb);
        REQUIRE(sb.cc == 0 && sb.mbcnt == 0);

        /* Teardown returns every page. */
        REQUIRE(pool.stats.mbufs == 0 && pool.stats.clusters == 0);
        REQUIRE(nx_net_mbuf_pool_destroy(&pool) && nx_net_slab_destroy(&socket_slab));
        REQUIRE(packets.pages == 0 && sockets.pages == 0 && page_free_count() == before);
        if (pass) continue;
        console_puts("net pools pages="); console_hex(limits.packet_pages);
        console_puts(" clusters="); console_hex(clusters);
        console_puts(" mbufs="); console_hex(mbufs);
        console_puts(" sockets="); console_hex(count);
        console_puts("\r\n");
    }
    console_puts("NET POOLS OK\r\n");
}
