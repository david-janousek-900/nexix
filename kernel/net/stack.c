#include "net/kernel.h"
#include "net/if.h"
#include "net/arp.h"
#include "net/route.h"
#include "net/e1000.h"
#include "net/worker.h"
#include "memory.h"
#include "interrupts.h"
#include <nexix/syscall.h>

static struct nx_net_budget budget;
static struct nx_net_mbuf_pool pool;
static struct nx_net_if lo0, em0;
static struct nx_net_work lo_work;
static struct nx_net_timer arp_timer;
static bool started;

struct nx_net_mbuf_pool *net_pool(void) { return started ? &pool : NULL; }

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
    nx_net_if_set_change_hook(nx_net_arp_if_changed);
    if (nx_net_proto_register(NX_NET_ETHERTYPE_ARP, nx_net_arp_input) != NX_NET_OK) { *ok = false; return; }
    nx_net_timer_init(&arp_timer, arp_tick, NULL);
    net_timer_arm(&arp_timer, net_ticks_from_ms(NX_NET_ARP_TIMER_MS));
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
    nx_net_route_clear_default();
    while (nx_net_if_count()) nx_net_if_detach(nx_net_if_at(0));
    nx_net_arp_flush(NULL);
    nx_net_if_set_change_hook(NULL);
    nx_net_proto_unregister(NX_NET_ETHERTYPE_ARP);
    net_worker_cancel(&lo_work);
}

bool net_start(void)
{
    if (started) return false;
    struct nx_net_kernel_limits limits = nx_net_kernel_limits(page_total_count());
    nx_net_budget_init(&budget, limits.packet_pages);
    if (!nx_net_mbuf_pool_init(&pool, &nx_net_kernel_pages, &budget, limits.mbuf)) return false;
    if (!net_worker_start()) { nx_net_mbuf_pool_destroy(&pool); return false; }
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
    started = false;
}
