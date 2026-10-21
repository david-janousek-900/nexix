#include "net/e1000.h"
#include "net/compat.h"
#include "net/wire.h"
#include "net/kernel.h"
#include "net/worker.h"
#include "console.h"
#include "interrupts.h"
#include "memory.h"
#include "paging.h"
#include "process.h"
#define REQUIRE(c) do { if (!(c)) panic("e1000 self-test: " #c); } while (0)

void e1000_test_fake_rx(unsigned ahead, uint16_t length, uint8_t status, uint8_t errors);
void e1000_test_reclaim(void);
unsigned e1000_test_ready(void);
void e1000_test_rx_pause(unsigned ahead);
void e1000_test_rx_resume(void);

#define FRAMES 100u
#define ICMP_ID 0x4e58u
static struct nx_net_budget budget;
static struct nx_net_mbuf_pool pool;
static unsigned received, arp_replies, unexpected;
static bool seen[256];
static uint8_t frame[E1000_FRAME_MAX + 1], gateway_mac[6];
static const uint8_t guest_ip[4] = {10, 0, 2, 15}, gateway_ip[4] = {10, 0, 2, 2};

/* Echo frames span 60..1514 bytes; QEMU's user-mode gateway reflects them. */
static size_t test_length(unsigned seq) { return 60 + (seq * 97) % (E1000_FRAME_MAX - 60 + 1); }
static uint8_t pattern(unsigned seq, size_t i) { return (uint8_t)(seq * 7 + i * 13); }
static uint16_t checksum(const uint8_t *data, size_t length)
{
    struct nx_net_checksum c;
    nx_net_checksum_init(&c);
    nx_net_checksum_update(&c, data, length);
    return nx_net_checksum_finish(&c);
}

static void dump(void)
{
    struct e1000_stats s = e1000_stats();
    console_puts("e1000 stats rx="); console_hex(s.rx_frames); console_puts(" tx="); console_hex(s.tx_frames);
    console_puts(" done="); console_hex(s.tx_completed); console_puts(" irqs="); console_hex(s.irqs);
    console_puts(" missed="); console_hex(s.rx_missed); console_puts(" bad="); console_hex(s.rx_malformed);
    console_puts(" ready="); console_hex(e1000_test_ready()); console_puts(" got="); console_hex(received);
    console_puts(" other="); console_hex(unexpected); console_puts("\r\n");
}
static void input(void *context, struct nx_net_mbuf *m)
{
    (void)context;
    REQUIRE(net_worker_context() && (m->flags & NX_NET_M_PKTHDR));
    size_t length = nx_net_mbuf_length(m);
    REQUIRE(m->pkt_len == length && length >= E1000_FRAME_MIN && length <= E1000_FRAME_MAX);
    REQUIRE(nx_net_mbuf_copydata(m, 0, length, frame));
    nx_net_mbuf_freem(m);
    uint16_t type = nx_net_get16(frame + 12);
    if (type == 0x0806 && length >= 42 && nx_net_get16(frame + 20) == 2 &&
        frame[28] == gateway_ip[0] && frame[29] == gateway_ip[1] &&
        frame[30] == gateway_ip[2] && frame[31] == gateway_ip[3]) {             /* ARP reply */
        for (unsigned i = 0; i < 6; ++i) gateway_mac[i] = frame[22 + i];
        ++arp_replies;
        return;
    }
    const uint8_t *ip = frame + 14, *icmp = ip + 20;
    if (type != 0x0800 || length < 42 || ip[0] != 0x45 || ip[9] != 1 || icmp[0] != 0 ||
        nx_net_get16(icmp + 4) != ICMP_ID) { ++unexpected; return; }
    unsigned seq = nx_net_get16(icmp + 6);
    size_t total = nx_net_get16(ip + 2);
    REQUIRE(seq < 256 && !seen[seq] && 14 + total == test_length(seq) && length >= 14 + total);
    REQUIRE(checksum(ip, 20) == 0 && checksum(icmp, total - 20) == 0);
    for (size_t i = 42; i < 14 + total; ++i) REQUIRE(frame[i] == pattern(seq, i));
    seen[seq] = true;
    ++received;
}
static struct nx_net_mbuf *echo(unsigned seq)
{
    size_t length = test_length(seq), total = length - 14;
    const uint8_t *mac = e1000_mac();
    for (unsigned i = 0; i < 6; ++i) { frame[i] = gateway_mac[i]; frame[6 + i] = mac[i]; }
    nx_net_put16(frame + 12, 0x0800);
    uint8_t *ip = frame + 14, *icmp = ip + 20;
    for (unsigned i = 0; i < 20; ++i) ip[i] = 0;
    ip[0] = 0x45; nx_net_put16(ip + 2, (uint16_t)total); nx_net_put16(ip + 4, (uint16_t)seq);
    ip[8] = 64; ip[9] = 1;
    for (unsigned i = 0; i < 4; ++i) { ip[12 + i] = guest_ip[i]; ip[16 + i] = gateway_ip[i]; }
    nx_net_put16(ip + 10, checksum(ip, 20));
    icmp[0] = 8; icmp[1] = 0; nx_net_put16(icmp + 2, 0);
    nx_net_put16(icmp + 4, ICMP_ID); nx_net_put16(icmp + 6, (uint16_t)seq);
    for (size_t i = 42; i < length; ++i) frame[i] = pattern(seq, i);
    nx_net_put16(icmp + 2, checksum(icmp, total - 20));
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    REQUIRE(m && nx_net_mbuf_append(m, frame, length));
    return m;
}
static struct nx_net_mbuf *raw(size_t length)
{
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    for (size_t i = 0; i < length; ++i) frame[i] = (uint8_t)i;
    REQUIRE(m && nx_net_mbuf_append(m, frame, length));
    return m;
}
static bool wait_for(volatile unsigned *counter, unsigned target)
{
    for (unsigned t = 0; t < 300 && *counter < target; ++t) proc_delay(1);
    if (*counter != target) dump();
    return *counter == target;
}

static unsigned sent_base, burst;
static void send_many(void *a)
{
    (void)a;
    for (unsigned i = 0; i < burst; ++i)
        while (!e1000_transmit(echo(sent_base + i))) e1000_poll();   /* reclaim when full */
}
static void malformed_tx(void *a)
{
    (void)a;
    uint64_t drops = pool.stats.drops;
    REQUIRE(!e1000_transmit(nx_net_mbuf_get(&pool, true)));
    REQUIRE(!e1000_transmit(raw(E1000_FRAME_MIN - 1)));
    REQUIRE(!e1000_transmit(raw(E1000_FRAME_MAX + 1)));
    REQUIRE(e1000_stats().tx_rejected == 3 && pool.stats.drops == drops + 3);
}
/* The worker is busy in these calls, so nothing reclaims or receives meanwhile. */
static unsigned tx_accepted, rx_ready;
static void fill_tx(void *a)
{
    (void)a;
    e1000_poll();
    tx_accepted = 0;
    for (unsigned i = 0; i < E1000_RING + 4; ++i)
        if (e1000_transmit(echo(sent_base + i))) ++tx_accepted;
}
static void fill_rx(void *a)
{
    (void)a;
    for (unsigned i = 0; i < 2 * (E1000_RING - 1); ++i)
        while (!e1000_transmit(echo(sent_base + i))) e1000_test_reclaim();
    /* Spin (never sleep) until replies fill every free receive slot. */
    uint64_t deadline = timer_ticks() + 200;
    while (e1000_test_ready() < E1000_RING - 1 && timer_ticks() < deadline) __asm__ volatile ("pause");
    rx_ready = e1000_test_ready();
}
static void malformed_rx(void *a)
{
    (void)a;
    e1000_poll();
    REQUIRE(e1000_test_ready() == 0);
    e1000_test_rx_pause(5);
    e1000_test_fake_rx(0, E1000_FRAME_MAX + 1, E1000_RXD_EOP, 0);   /* too long */
    e1000_test_fake_rx(1, E1000_FRAME_MIN - 1, E1000_RXD_EOP, 0);   /* too short */
    e1000_test_fake_rx(2, 100, E1000_RXD_EOP, 0x01);                /* CRC error */
    e1000_test_fake_rx(3, 1000, 0, 0);                              /* spans two */
    e1000_test_fake_rx(4, 1000, E1000_RXD_EOP, 0);
    uint64_t before = e1000_stats().rx_malformed, frames = e1000_stats().rx_frames;
    e1000_poll();
    e1000_test_rx_resume();
    REQUIRE(e1000_stats().rx_malformed == before + 4 && e1000_stats().rx_frames == frames);
}
static void arp_request(void *a)
{
    (void)a;
    const uint8_t *mac = e1000_mac();
    uint8_t arp[42] = {0};
    for (unsigned i = 0; i < 6; ++i) { arp[i] = 0xff; arp[6 + i] = mac[i]; arp[22 + i] = mac[i]; }
    nx_net_put16(arp + 12, 0x0806);
    nx_net_put16(arp + 14, 1); nx_net_put16(arp + 16, 0x0800); arp[18] = 6; arp[19] = 4;
    nx_net_put16(arp + 20, 1);
    for (unsigned i = 0; i < 4; ++i) { arp[28 + i] = guest_ip[i]; arp[38 + i] = gateway_ip[i]; }
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    REQUIRE(m && nx_net_mbuf_append(m, arp, sizeof(arp)) && e1000_transmit(m));
}
static void stop(void *a) { (void)a; e1000_stop(); }
static void reset_seen(void) { for (unsigned i = 0; i < 256; ++i) seen[i] = false; received = 0; }

static void dma_tests(void *argument)
{
    (void)argument;
    int32_t status;
    uint32_t pages = page_free_count();
    nx_net_budget_init(&budget, 96);
    REQUIRE(nx_net_mbuf_pool_init(&pool, &nx_net_kernel_pages, &budget, (struct nx_net_mbuf_limits){ 512, 192 }));
    REQUIRE(!e1000_start(&pool, input, NULL));           /* worker not running */
    REQUIRE(net_worker_start());
    for (unsigned round = 0; round < 2; ++round) {
        REQUIRE(e1000_start(&pool, input, NULL) && !e1000_start(&pool, input, NULL));
        const uint8_t *mac = e1000_mac();
        REQUIRE(mac[0] == 0x52 && mac[1] == 0x54 && mac[2] == 0x00 && !(mac[0] & 1));
        REQUIRE(e1000_link_up());

        /* QEMU holds receives for about a second after RCTL is written;
         * retrying ARP until the gateway answers covers that window. */
        arp_replies = 0;
        for (unsigned attempt = 0; attempt < 5 && !arp_replies; ++attempt) {
            REQUIRE(net_worker_call(arp_request, NULL));
            for (unsigned t = 0; t < 100 && !arp_replies; ++t) proc_delay(1);
        }
        REQUIRE(arp_replies && gateway_mac[0] == 0x52 && gateway_mac[1] == 0x55);
        if (!round) console_puts("E1000 ARP OK\r\n");

        /* 100 echoes of 60..1514 bytes wrap both 32-entry rings three times. */
        reset_seen();
        sent_base = 0; burst = FRAMES;
        REQUIRE(net_worker_call(send_many, NULL));
        REQUIRE(wait_for(&received, FRAMES));
        struct e1000_stats s = e1000_stats();
        REQUIRE(s.tx_frames >= FRAMES && s.rx_frames >= FRAMES && s.irqs > 0 && !unexpected);
        if (!round) console_puts("E1000 TRAFFIC OK\r\n");

        /* A full transmit ring refuses and counts; queued frames still go. */
        reset_seen();
        sent_base = 0;
        uint64_t full = e1000_stats().tx_full;
        REQUIRE(net_worker_call(fill_tx, NULL));
        REQUIRE(tx_accepted == E1000_RING - 1 && e1000_stats().tx_full == full + 5);
        REQUIRE(wait_for(&received, E1000_RING - 1));

        /* Receive ring full while unserviced: the device reports the overrun
         * (real hardware drops and counts; QEMU holds frames and counts each
         * retry), and reception resumes once slots are returned. */
        reset_seen();
        sent_base = 0;
        uint64_t missed = e1000_stats().rx_missed, overruns = e1000_stats().overruns;
        REQUIRE(net_worker_call(fill_rx, NULL));
        REQUIRE(rx_ready == E1000_RING - 1);
        for (unsigned t = 0; t < 300 && received < 2 * (E1000_RING - 1); ++t) proc_delay(1);
        s = e1000_stats();
        REQUIRE(received >= E1000_RING - 1 && (s.rx_missed > missed || s.overruns > overruns));
        if (!round) console_puts("E1000 RINGS FULL OK\r\n");

        REQUIRE(net_worker_call(malformed_tx, NULL));
        REQUIRE(net_worker_call(malformed_rx, NULL));
        reset_seen();
        sent_base = 0; burst = 5;
        REQUIRE(net_worker_call(send_many, NULL) && wait_for(&received, 5));   /* still healthy */
        if (!round) console_puts("E1000 MALFORMED OK\r\n");

        REQUIRE(net_worker_call(stop, NULL) && !e1000_started());
        REQUIRE(pool.stats.mbufs == 0 && pool.stats.clusters == 0);
    }
    net_worker_stop();
    REQUIRE(proc_wait(&status) > 0 && status == 0);
    REQUIRE(nx_net_mbuf_pool_destroy(&pool));
    REQUIRE(page_free_count() == pages);
    console_puts("E1000 CLEANUP OK\r\n");
    proc_exit(0);
}

void e1000_selftest(void)
{
    /* Discovery maps BAR0 first; the window's page tables are permanent. */
    const struct e1000_device *found = e1000_discover();
    /* The MMIO window refuses empty ranges and RAM frames, and leaks nothing. */
    uint32_t before = page_free_count();
    uintptr_t flags = proc_irq_save();
    uintptr_t ram = page_alloc();
    REQUIRE(ram && !paging_map_mmio(ram, PAGE_SIZE) && page_free(ram));
    REQUIRE(!paging_map_mmio(0x100000, 0) && !paging_map_mmio(PAGE_SIZE, PAGE_SIZE));
    REQUIRE(!paging_map_mmio(UINT64_MAX - PAGE_SIZE, 2 * PAGE_SIZE));
    proc_irq_restore(flags);
    REQUIRE(page_free_count() == before);

    pci_log();
    REQUIRE(found == e1000_discover());   /* idempotent */
    if (!found) {
        REQUIRE(!pci_find(pci_system(), E1000_VENDOR, E1000_DEVICE_82540EM));
        console_puts("E1000 ABSENT\r\n");
        console_puts("PCI OK\r\n");
        return;
    }
    const struct pci_function *f = found->pci;
    uint16_t command = pci_command(pci_system(), f);
    REQUIRE((command & (PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER)) == (PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER));
    REQUIRE(!(command & PCI_COMMAND_INTX_DISABLE));
    REQUIRE(f->class_code == 2 && f->subclass == 0);   /* Ethernet controller */
    REQUIRE((uintptr_t)found->registers >= MMIO_BASE && (uintptr_t)found->registers < MMIO_BASE + MMIO_SIZE);
    /* Device registers answer through the mapping: link-up/full-duplex are
     * reset-stable STATUS bits on QEMU, and CTRL never reads as all ones. */
    uint32_t status = e1000_read(found, E1000_STATUS), ctrl = e1000_read(found, E1000_CTRL);
    REQUIRE(status != UINT32_MAX && ctrl != UINT32_MAX && (status & 3) == 3);
    console_puts("e1000 pci="); console_hex((uint64_t)f->bus << 16 | (uint64_t)f->device << 8 | f->function);
    console_puts(" bar0="); console_hex(f->bars[0].base);
    console_puts(" size="); console_hex(f->bars[0].size);
    console_puts(" irq="); console_hex(found->irq);
    console_puts(" status="); console_hex(status);
    console_puts("\r\n");
    console_puts("E1000 FOUND\r\n");
    console_puts("PCI OK\r\n");

    v7_pid_t pid = proc_spawn(dma_tests, NULL);
    REQUIRE(pid > 0);
    proc_run();
    int32_t exit_status;
    REQUIRE(proc_collect(&exit_status) == pid && exit_status == 0);
    REQUIRE(page_free_count() == before);
    console_puts("E1000 DMA OK\r\n");
}
