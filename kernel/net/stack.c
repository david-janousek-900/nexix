#include "net/kernel.h"
#include "net/if.h"
#include "net/arp.h"
#include "net/route.h"
#include "net/ip.h"
#include "net/icmp.h"
#include "net/udp.h"
#include "net/tcp.h"
#include "net/e1000.h"
#include "net/worker.h"
#include "memory.h"
#include "interrupts.h"
#include <nexix/syscall.h>

static struct nx_net_budget budget, socket_budget;
static struct nx_net_mbuf_pool pool;
static struct nx_net_slab sockets;
static struct nx_net_if lo0, em0;
static struct nx_net_work lo_work;
static struct nx_net_timer arp_timer, ip_timer, tcp_timer;
static bool started;

struct nx_net_mbuf_pool *net_pool(void) { return started ? &pool : NULL; }
struct nx_net_slab *net_socket_slab(void) { return started ? &sockets : NULL; }

/* Loopback delivery always runs as later worker work, never inside output. */
static void lo_service(void *argument)
{
    (void)argument;
    if (nx_net_loop_service(&lo0, NX_NET_WORK_BATCH)) net_worker_submit(&lo_work);
}
static void lo_kick(void *argument) { (void)argument; net_worker_submit(&lo_work); }

static uint64_t clock_ms(void) { return timer_ticks() * 1000 / NX_CLK_TCK; }
static void arp_tick(void *argument)
{
    (void)argument;
    nx_net_arp_timer();
    net_timer_arm(&arp_timer, net_ticks_from_ms(NX_NET_ARP_TIMER_MS));
}
static void ip_tick(void *argument)
{
    (void)argument;
    nx_net_ip_timer();
    net_timer_arm(&ip_timer, net_ticks_from_ms(NX_NET_IP_TIMER_MS));
}
static void tcp_tick(void *argument)
{
    (void)argument;
    nx_net_tcp_timer();
    net_timer_arm(&tcp_timer, net_ticks_from_ms(NX_NET_TCP_TIMER_MS));
}
/* Keys TCP initial sequence numbers: the cycle counter at start is not
 * predictable from outside to the precision that matters. */
static uint64_t sequence_secret(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32 | lo) ^ timer_ticks() << 17;
}
static void changed(struct nx_net_if *ifp)
{
    nx_net_arp_if_changed(ifp);
    nx_net_ip_if_changed(ifp);
}

static void em_receive(void *context, struct nx_net_mbuf *frame) { nx_net_ether_input(context, frame); }
static bool em_up(struct nx_net_if *ifp)
{
    if (!e1000_start(&pool, em_receive, ifp)) return false;
    const uint8_t *mac = e1000_mac();
    for (unsigned i = 0; i < NX_NET_ETHER_ADDR; ++i) ifp->lladdr[i] = mac[i];
    return true;
}
static void em_down(struct nx_net_if *ifp) { (void)ifp; e1000_stop(); }
static bool em_transmit(struct nx_net_if *ifp, struct nx_net_mbuf *frame)
{
    (void)ifp;
    return e1000_transmit(frame);
}
static const struct nx_net_if_ops em_ops = { em_up, em_down, em_transmit };

static void attach(void *argument)
{
    bool *ok = argument;
    nx_net_arp_init(&pool, clock_ms);
    nx_net_ip_init(&pool, clock_ms);
    nx_net_icmp_init();
    nx_net_udp_init();
    nx_net_tcp_init(&pool, clock_ms, sequence_secret());
    nx_net_if_set_change_hook(changed);
    if (nx_net_proto_register(NX_NET_ETHERTYPE_ARP, nx_net_arp_input) != NX_NET_OK ||
        nx_net_proto_register(NX_NET_ETHERTYPE_IP, nx_net_ip_input) != NX_NET_OK ||
        nx_net_ip_register(NX_NET_IPPROTO_ICMP, nx_net_icmp_input) != NX_NET_OK ||
        nx_net_ip_register(NX_NET_IPPROTO_UDP, nx_net_udp_input) != NX_NET_OK ||
        nx_net_icmp_register_notify(NX_NET_IPPROTO_UDP, nx_net_udp_notify) != NX_NET_OK ||
        nx_net_ip_register(NX_NET_IPPROTO_TCP, nx_net_tcp_input) != NX_NET_OK ||
        nx_net_icmp_register_notify(NX_NET_IPPROTO_TCP, nx_net_tcp_notify) != NX_NET_OK) { *ok = false; return; }
    nx_net_ip_set_unknown_hook(nx_net_icmp_unknown);
    nx_net_timer_init(&arp_timer, arp_tick, NULL);
    net_timer_arm(&arp_timer, net_ticks_from_ms(NX_NET_ARP_TIMER_MS));
    nx_net_timer_init(&ip_timer, ip_tick, NULL);
    net_timer_arm(&ip_timer, net_ticks_from_ms(NX_NET_IP_TIMER_MS));
    nx_net_timer_init(&tcp_timer, tcp_tick, NULL);
    net_timer_arm(&tcp_timer, net_ticks_from_ms(NX_NET_TCP_TIMER_MS));
    nx_net_loop_init(&lo0, "lo0", lo_kick, NULL);
    *ok = nx_net_if_attach(&lo0) == NX_NET_OK && nx_net_if_up(&lo0) == NX_NET_OK;
    if (*ok && e1000_discover()) {
        static const uint8_t unknown[NX_NET_ETHER_ADDR];   /* read from EEPROM at up */
        nx_net_ether_init(&em0, "em0", unknown, &em_ops, NULL);
        *ok = nx_net_if_attach(&em0) == NX_NET_OK;
    }
}
static void detach(void *argument)
{
    (void)argument;
    net_timer_cancel(&arp_timer);
    net_timer_cancel(&ip_timer);
    net_timer_cancel(&tcp_timer);
    nx_net_route_clear_default();
    while (nx_net_if_count()) nx_net_if_detach(nx_net_if_at(0));
    nx_net_arp_flush(NULL);
    nx_net_ip_flush();
    nx_net_raw_flush();
    nx_net_udp_flush();
    nx_net_tcp_flush();
    nx_net_icmp_unregister_notify(NX_NET_IPPROTO_TCP);
    nx_net_ip_unregister(NX_NET_IPPROTO_TCP);
    nx_net_icmp_unregister_notify(NX_NET_IPPROTO_UDP);
    nx_net_ip_unregister(NX_NET_IPPROTO_UDP);
    nx_net_ip_set_unknown_hook(NULL);
    nx_net_ip_unregister(NX_NET_IPPROTO_ICMP);
    nx_net_if_set_change_hook(NULL);
    nx_net_proto_unregister(NX_NET_ETHERTYPE_ARP);
    nx_net_proto_unregister(NX_NET_ETHERTYPE_IP);
    net_worker_cancel(&lo_work);
}

bool net_start(void)
{
    if (started) return false;
    struct nx_net_kernel_limits limits = nx_net_kernel_limits(page_total_count());
    nx_net_budget_init(&budget, limits.packet_pages);
    if (!nx_net_mbuf_pool_init(&pool, &nx_net_kernel_pages, &budget, limits.mbuf)) return false;
    nx_net_budget_init(&socket_budget, NX_NET_SOCKET_PAGES);
    if (!nx_net_slab_init(&sockets, &nx_net_kernel_pages, &socket_budget, NX_NET_SOCKET_OBJECT_SIZE,
                          limits.socket_objects)) {
        nx_net_mbuf_pool_destroy(&pool);
        return false;
    }
    if (!net_worker_start()) { nx_net_slab_destroy(&sockets); nx_net_mbuf_pool_destroy(&pool); return false; }
    nx_net_work_init(&lo_work, lo_service, NULL);
    bool ok = false;
    started = true;
    if (!net_worker_call(attach, &ok) || !ok) { net_stop(); return false; }
    return true;
}
void net_stop(void)
{
    if (!started) return;
    net_worker_call(detach, NULL);
    net_worker_stop();
    if (!nx_net_mbuf_pool_destroy(&pool)) panic("network pool still in use at stop");
    if (!nx_net_slab_destroy(&sockets)) panic("socket objects still in use at stop");
    started = false;
}
