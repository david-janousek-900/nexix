#include "net/kernel.h"
#include "net/if.h"
#include "net/e1000.h"
#include "net/worker.h"
#include "net/wire.h"
#include "console.h"
#include "memory.h"
#include "process.h"
#define REQUIRE(c) do { if (!(c)) panic("network stack self-test: " #c); } while (0)

/* Temporary protocol sinks: IPv4 counts loopback packets and checks their
 * bytes; ARP records replies from QEMU's user-mode gateway. */
static volatile unsigned ip_packets, arp_replies;
static struct nx_net_if *ip_ifp;
static void ip_sink(struct nx_net_if *ifp, struct nx_net_mbuf *m)
{
    REQUIRE(net_worker_context() && (m->flags & NX_NET_M_PKTHDR));
    size_t length = nx_net_mbuf_length(m);
    uint8_t bytes[64];
    REQUIRE(length >= 2 && length <= sizeof(bytes) && nx_net_mbuf_copydata(m, 0, length, bytes));
    for (size_t i = 2; i < length; ++i) REQUIRE(bytes[i] == (uint8_t)(bytes[0] + i));
    nx_net_mbuf_freem(m);
    ip_ifp = ifp;
    ++ip_packets;
}
static void arp_sink(struct nx_net_if *ifp, struct nx_net_mbuf *m)
{
    uint8_t arp[28];
    if (nx_net_mbuf_length(m) >= 28 && nx_net_mbuf_copydata(m, 0, 28, arp) &&
        nx_net_get16(arp + 6) == 2 && arp[14] == 10 && arp[17] == 2 &&
        ifp->name[0] == 'e' && ifp->name[1] == 'm' && ifp->name[2] == '0') ++arp_replies;
    nx_net_mbuf_freem(m);
}

static struct nx_net_mbuf *packet(size_t length, uint8_t seed)
{
    uint8_t bytes[64];
    for (size_t i = 0; i < length; ++i) bytes[i] = (uint8_t)(seed + i);
    bytes[0] = seed;
    struct nx_net_mbuf *m = nx_net_mbuf_get(net_pool(), true);
    REQUIRE(m && nx_net_mbuf_append(m, bytes, length));
    return m;
}

static unsigned accepted;
static void register_sinks(void *a)
{
    (void)a;
    REQUIRE(nx_net_proto_register(NX_NET_ETHERTYPE_IP, ip_sink) == NX_NET_OK);
    REQUIRE(nx_net_proto_register(NX_NET_ETHERTYPE_ARP, arp_sink) == NX_NET_OK);
}
static void unregister_sinks(void *a)
{
    (void)a;
    nx_net_proto_unregister(NX_NET_ETHERTYPE_IP);
    nx_net_proto_unregister(NX_NET_ETHERTYPE_ARP);
}
/* Queue more than lo0 holds within one worker pass: nothing is delivered
 * until this call returns, and the overflow is dropped and counted. */
static void loop_burst(void *a)
{
    (void)a;
    struct nx_net_if *lo = nx_net_if_find("lo0");
    accepted = 0;
    for (unsigned i = 0; i < NX_NET_IFQ_MAXLEN + 5; ++i)
        accepted += nx_net_if_output(lo, packet(20 + i % 40, (uint8_t)i), NULL, NX_NET_ETHERTYPE_IP) == NX_NET_OK;
    REQUIRE(ip_packets == 0 && lo->queue_length == NX_NET_IFQ_MAXLEN);
}
static bool em_ok;
static void em_up(void *a) { (void)a; em_ok = nx_net_if_up(nx_net_if_find("em0")) == NX_NET_OK; }
static void em_down(void *a) { (void)a; nx_net_if_down(nx_net_if_find("em0")); }
static void em_states(void *a)
{
    (void)a;
    struct nx_net_if *em = nx_net_if_find("em0");
    /* Down: refused; oversize: refused; both free their chains. */
    REQUIRE(nx_net_if_output(em, packet(20, 0), nx_net_ether_broadcast, NX_NET_ETHERTYPE_IP) == NX_NET_DOWN);
    struct nx_net_mbuf *big = nx_net_mbuf_get(net_pool(), true);
    static uint8_t zero[NX_NET_ETHER_MTU + 1];
    REQUIRE(big && nx_net_mbuf_append(big, zero, sizeof(zero)));
    REQUIRE(nx_net_if_up(em) == NX_NET_OK);
    REQUIRE(em->lladdr[0] == 0x52 && em->lladdr[1] == 0x54 && (em->flags & NX_NET_IFF_RUNNING));
    REQUIRE(nx_net_if_output(em, big, nx_net_ether_broadcast, NX_NET_ETHERTYPE_IP) == NX_NET_TOO_BIG);
    REQUIRE(em->stats.oerrors == 2);
}
static void arp_request(void *a)
{
    (void)a;
    struct nx_net_if *em = nx_net_if_find("em0");
    uint8_t arp[28] = {0};
    nx_net_put16(arp, 1); nx_net_put16(arp + 2, 0x0800); arp[4] = 6; arp[5] = 4; nx_net_put16(arp + 6, 1);
    for (unsigned i = 0; i < 6; ++i) arp[8 + i] = em->lladdr[i];
    arp[14] = 10; arp[15] = 0; arp[16] = 2; arp[17] = 15;
    arp[24] = 10; arp[25] = 0; arp[26] = 2; arp[27] = 2;
    struct nx_net_mbuf *m = nx_net_mbuf_get(net_pool(), true);
    REQUIRE(m && nx_net_mbuf_append(m, arp, sizeof(arp)));
    REQUIRE(nx_net_if_output(em, m, nx_net_ether_broadcast, NX_NET_ETHERTYPE_ARP) == NX_NET_OK);
}

static void tests(void *argument)
{
    (void)argument;
    int32_t status;
    uint32_t pages = page_free_count();
    for (unsigned round = 0; round < 2; ++round) {
        REQUIRE(net_start() && !net_start() && net_pool());
        REQUIRE(net_worker_call(register_sinks, NULL));
        struct nx_net_if *lo = nx_net_if_find("lo0"), *em = nx_net_if_find("em0");
        REQUIRE(lo && (lo->flags & (NX_NET_IFF_UP | NX_NET_IFF_RUNNING | NX_NET_IFF_LOOPBACK)) ==
                (NX_NET_IFF_UP | NX_NET_IFF_RUNNING | NX_NET_IFF_LOOPBACK));
        REQUIRE(nx_net_if_count() == (em ? 2u : 1u) && (!em || !(em->flags & NX_NET_IFF_UP)));

        ip_packets = 0;
        REQUIRE(net_worker_call(loop_burst, NULL) && accepted == NX_NET_IFQ_MAXLEN);
        for (unsigned t = 0; t < 100 && ip_packets < NX_NET_IFQ_MAXLEN; ++t) proc_delay(1);
        REQUIRE(ip_packets == NX_NET_IFQ_MAXLEN && ip_ifp == lo && lo->queue_length == 0);
        REQUIRE(lo->stats.iqdrops == 5 && lo->stats.opackets == NX_NET_IFQ_MAXLEN);
        REQUIRE(net_pool()->stats.mbufs == 0);   /* every looped packet was freed once */
        if (!round) console_puts("NET LOOPBACK OK\r\n");

        if (em) {
            REQUIRE(net_worker_call(em_states, NULL));
            arp_replies = 0;
            /* QEMU holds receive for ~1 s after enabling it; retry. */
            for (unsigned attempt = 0; attempt < 5 && !arp_replies; ++attempt) {
                REQUIRE(net_worker_call(arp_request, NULL));
                for (unsigned t = 0; t < 100 && !arp_replies; ++t) proc_delay(1);
            }
            REQUIRE(arp_replies && em->stats.ipackets >= 1 && em->stats.opackets >= 1);
            REQUIRE(net_worker_call(em_down, NULL) && !e1000_started() && !(em->flags & NX_NET_IFF_UP));
            REQUIRE(net_worker_call(em_up, NULL) && em_ok && e1000_started());   /* restartable */
            if (!round) console_puts("NET ETHERNET OK\r\n");
        } else if (!round) {
            console_puts("NET ETHERNET ABSENT\r\n");
        }
        REQUIRE(net_worker_call(unregister_sinks, NULL));
        net_stop();
        REQUIRE(!net_pool() && nx_net_if_count() == 0 && !e1000_started());
        REQUIRE(proc_wait(&status) > 0 && status == 0);   /* reap the worker */
        REQUIRE(page_free_count() == pages);
    }
    proc_exit(0);
}

void net_stack_selftest(void)
{
    uint32_t before = page_free_count();
    v7_pid_t pid = proc_spawn(tests, NULL);
    REQUIRE(pid > 0);
    proc_run();
    int32_t status;
    REQUIRE(proc_collect(&status) == pid && status == 0);
    REQUIRE(page_free_count() == before);
    console_puts("NET STACK OK\r\n");
}
