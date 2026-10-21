#include "net/e1000.h"
#include "net/worker.h"
#include "console.h"
#include "interrupts.h"
#include "memory.h"
#include "process.h"

#define CTRL_ASDE (1u << 5)
#define CTRL_SLU (1u << 6)
#define CTRL_RST (1u << 26)
#define STATUS_LU (1u << 1)
#define RCTL_EN (1u << 1)
#define RCTL_BAM (1u << 15)
#define RCTL_SECRC (1u << 26)     /* BSIZE 00 with BSEX 0: 2048-byte buffers */
#define TCTL_EN (1u << 1)
#define TCTL_PSP (1u << 3)
#define TCTL_CT (0x0fu << 4)
#define TCTL_COLD (0x40u << 12)
#define TXD_EOP 0x01u
#define TXD_IFCS 0x02u
#define TXD_RS 0x08u
#define TXD_DD 0x01u
#define EERD_START 1u
#define EERD_DONE (1u << 4)
#define INTERRUPTS (E1000_ICR_TXDW | E1000_ICR_LSC | E1000_ICR_RXDMT0 | E1000_ICR_RXO | E1000_ICR_RXT0)
#define SPIN 100000u
#define BUFFERS_PER_PAGE (PAGE_SIZE / E1000_BUFFER)
#define BUFFER_PAGES (E1000_RING / BUFFERS_PER_PAGE)

struct rx_desc { uint64_t address; uint16_t length, checksum; uint8_t status, errors; uint16_t special; };
struct tx_desc { uint64_t address; uint16_t length; uint8_t cso, cmd, status, css; uint16_t special; };
_Static_assert(sizeof(struct rx_desc) == 16 && sizeof(struct tx_desc) == 16, "legacy descriptors");
_Static_assert(E1000_RING * 16 % 128 == 0 && E1000_RING * 16 <= PAGE_SIZE, "descriptor ring length");

static struct e1000_device nic;
static bool discovered;

const struct e1000_device *e1000_discover(void)
{
    if (discovered) return nic.registers ? &nic : NULL;
    discovered = true;
    struct pci_bus *bus = pci_system();
    const struct pci_function *f = pci_find(bus, E1000_VENDOR, E1000_DEVICE_82540EM);
    if (!f) return NULL;
    const struct pci_bar *bar = &f->bars[0];
    if ((bar->kind != PCI_BAR_MEM32 && bar->kind != PCI_BAR_MEM64) || bar->size < E1000_REGISTER_BYTES) {
        console_puts("e1000: BAR0 is not a register window\r\n");
        return NULL;
    }
    /* Map before enabling decode, so a failed mapping leaves the device off. */
    volatile void *registers = pci_map_bar(f, 0);
    if (!registers || !pci_enable(bus, f, PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER)) {
        console_puts("e1000: cannot map or enable adapter\r\n");
        return NULL;
    }
    nic = (struct e1000_device){ .pci = f, .registers = registers, .irq = f->irq_line };
    return &nic;
}

/* Driver state. Rings and buffers are identity-mapped pages below 1 GiB, so
 * their virtual addresses are their bus addresses. Everything except the
 * pending-ICR word is touched only in worker context (or before the IRQ is
 * attached / after it is detached). */
static struct {
    bool started, discarding, link;
    struct nx_net_mbuf_pool *pool;
    e1000_input_fn input;
    void *context;
    volatile struct rx_desc *rx;
    volatile struct tx_desc *tx;
    uint8_t *rx_buffers[BUFFER_PAGES], *tx_buffers[BUFFER_PAGES];
    unsigned rx_next, tx_tail, tx_clean;
    uint8_t mac[6];
    struct nx_net_work work;
    struct e1000_stats stats;
} d;
static volatile uint32_t pending;   /* ICR causes not yet seen by the worker */

static uint8_t *buffer(uint8_t *const pages[], unsigned index)
{
    return pages[index / BUFFERS_PER_PAGE] + (index % BUFFERS_PER_PAGE) * E1000_BUFFER;
}
static void release_pages(void)
{
    if (d.rx) page_free((uintptr_t)d.rx);
    if (d.tx) page_free((uintptr_t)d.tx);
    for (unsigned i = 0; i < BUFFER_PAGES; ++i) {
        if (d.rx_buffers[i]) page_free((uintptr_t)d.rx_buffers[i]);
        if (d.tx_buffers[i]) page_free((uintptr_t)d.tx_buffers[i]);
    }
    d.rx = NULL; d.tx = NULL;
    for (unsigned i = 0; i < BUFFER_PAGES; ++i) d.rx_buffers[i] = d.tx_buffers[i] = NULL;
}
static bool allocate_pages(void)
{
    uintptr_t flags = proc_irq_save();   /* page allocator requires serialization */
    bool ok = (d.rx = (void *)page_alloc()) && (d.tx = (void *)page_alloc());
    for (unsigned i = 0; ok && i < BUFFER_PAGES; ++i)
        ok = (d.rx_buffers[i] = (void *)page_alloc()) && (d.tx_buffers[i] = (void *)page_alloc());
    if (!ok) release_pages();
    proc_irq_restore(flags);
    return ok;
}

static bool reset(void)
{
    e1000_write(&nic, E1000_IMC, UINT32_MAX);
    e1000_write(&nic, E1000_RCTL, 0);
    e1000_write(&nic, E1000_TCTL, 0);
    e1000_write(&nic, E1000_CTRL, e1000_read(&nic, E1000_CTRL) | CTRL_RST);
    unsigned spins = 0;
    while ((e1000_read(&nic, E1000_CTRL) & CTRL_RST) && ++spins < SPIN) __asm__ volatile ("pause");
    e1000_write(&nic, E1000_IMC, UINT32_MAX);
    (void)e1000_read(&nic, E1000_ICR);
    return spins < SPIN;
}
static bool eeprom(uint8_t word, uint16_t *value)
{
    e1000_write(&nic, E1000_EERD, (uint32_t)word << 8 | EERD_START);
    for (unsigned spins = 0; spins < SPIN; ++spins) {
        uint32_t r = e1000_read(&nic, E1000_EERD);
        if (r & EERD_DONE) { *value = r >> 16; return true; }
        __asm__ volatile ("pause");
    }
    return false;
}
static void interrupt(void *argument)
{
    (void)argument;
    uint32_t cause = e1000_read(&nic, E1000_ICR);   /* reading acknowledges INTx */
    if (!cause) return;                               /* shared line, not ours */
    ++d.stats.irqs;
    pending |= cause;
    net_worker_submit(&d.work);                       /* never sleeps; coalesces */
}
static void serviced(void *argument) { (void)argument; e1000_poll(); }

bool e1000_start(struct nx_net_mbuf_pool *pool, e1000_input_fn input, void *context)
{
    if (d.started || !pool || !input || !net_worker_running() || !e1000_discover() ||
        nic.irq == PCI_IRQ_NONE) return false;
    if (!reset()) return false;
    uint16_t words[3];
    for (unsigned i = 0; i < 3; ++i) if (!eeprom(i, &words[i])) return false;
    if (!allocate_pages()) return false;
    for (unsigned i = 0; i < 3; ++i) { d.mac[2 * i] = words[i] & 0xff; d.mac[2 * i + 1] = words[i] >> 8; }
    d.pool = pool; d.input = input; d.context = context;
    d.rx_next = d.tx_tail = d.tx_clean = 0;
    d.discarding = false;
    d.stats = (struct e1000_stats){ 0 };
    pending = 0;
    nx_net_work_init(&d.work, serviced, NULL);

    e1000_write(&nic, E1000_CTRL, (e1000_read(&nic, E1000_CTRL) | CTRL_SLU | CTRL_ASDE));
    e1000_write(&nic, E1000_RAL0, (uint32_t)words[1] << 16 | words[0]);
    e1000_write(&nic, E1000_RAH0, words[2] | 1u << 31);   /* address valid */
    for (unsigned i = 0; i < 128; ++i) e1000_write(&nic, E1000_MTA + 4 * i, 0);
    for (unsigned i = 0; i < E1000_RING; ++i) {
        d.rx[i] = (struct rx_desc){ .address = (uintptr_t)buffer(d.rx_buffers, i) };
        d.tx[i] = (struct tx_desc){ .address = (uintptr_t)buffer(d.tx_buffers, i), .status = TXD_DD };
    }
    e1000_write(&nic, E1000_RDBAL, (uint32_t)(uintptr_t)d.rx);
    e1000_write(&nic, E1000_RDBAH, (uint32_t)((uintptr_t)d.rx >> 32));
    e1000_write(&nic, E1000_RDLEN, E1000_RING * sizeof(struct rx_desc));
    e1000_write(&nic, E1000_RDH, 0);
    e1000_write(&nic, E1000_RDT, E1000_RING - 1);
    e1000_write(&nic, E1000_TDBAL, (uint32_t)(uintptr_t)d.tx);
    e1000_write(&nic, E1000_TDBAH, (uint32_t)((uintptr_t)d.tx >> 32));
    e1000_write(&nic, E1000_TDLEN, E1000_RING * sizeof(struct tx_desc));
    e1000_write(&nic, E1000_TDH, 0);
    e1000_write(&nic, E1000_TDT, 0);
    e1000_write(&nic, E1000_TIPG, 0x0060200a);
    e1000_write(&nic, E1000_TCTL, TCTL_EN | TCTL_PSP | TCTL_CT | TCTL_COLD);
    e1000_write(&nic, E1000_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);
    d.link = e1000_read(&nic, E1000_STATUS) & STATUS_LU;
    (void)e1000_read(&nic, E1000_MPC);
    if (!irq_attach(nic.irq, interrupt, NULL)) {
        reset();
        uintptr_t flags = proc_irq_save();
        release_pages();
        proc_irq_restore(flags);
        return false;
    }
    d.started = true;
    e1000_write(&nic, E1000_IMS, INTERRUPTS);
    return true;
}

void e1000_stop(void)
{
    if (!d.started) return;
    e1000_write(&nic, E1000_IMC, UINT32_MAX);
    irq_detach(nic.irq);
    reset();   /* Clears RCTL/TCTL and stops all DMA before pages are freed. */
    net_worker_cancel(&d.work);
    pending = 0;
    uintptr_t flags = proc_irq_save();
    release_pages();
    proc_irq_restore(flags);
    d.started = false;
}
bool e1000_started(void) { return d.started; }

bool e1000_transmit(struct nx_net_mbuf *frame)
{
    size_t length = nx_net_mbuf_length(frame);
    if (!d.started || length < E1000_FRAME_MIN || length > E1000_FRAME_MAX) {
        ++d.stats.tx_rejected;
        nx_net_mbuf_drop(frame);
        return false;
    }
    unsigned next = (d.tx_tail + 1) % E1000_RING;
    if (next == d.tx_clean) {
        ++d.stats.tx_full;
        nx_net_mbuf_drop(frame);
        return false;
    }
    volatile struct tx_desc *desc = &d.tx[d.tx_tail];
    nx_net_mbuf_copydata(frame, 0, length, buffer(d.tx_buffers, d.tx_tail));
    nx_net_mbuf_freem(frame);
    desc->length = (uint16_t)length;
    desc->cmd = TXD_EOP | TXD_IFCS | TXD_RS;
    desc->status = 0;
    __asm__ volatile ("" : : : "memory");   /* descriptor before tail (x86 stores are ordered) */
    d.tx_tail = next;
    e1000_write(&nic, E1000_TDT, next);
    ++d.stats.tx_frames;
    d.stats.tx_bytes += length;
    return true;
}

static void reclaim(void)
{
    while (d.tx_clean != d.tx_tail && (d.tx[d.tx_clean].status & TXD_DD)) {
        d.tx_clean = (d.tx_clean + 1) % E1000_RING;
        ++d.stats.tx_completed;
    }
}
static void receive(void)
{
    for (unsigned budget = 0; budget < E1000_RING; ++budget) {
        volatile struct rx_desc *desc = &d.rx[d.rx_next];
        uint8_t status = desc->status;
        if (!(status & E1000_RXD_DD)) break;
        __asm__ volatile ("" : : : "memory");   /* status before contents */
        uint16_t length = desc->length;
        switch (e1000_rx_classify(length, status, desc->errors, &d.discarding)) {
        case E1000_RX_MALFORMED:
            ++d.stats.rx_malformed;
            break;
        case E1000_RX_CONTINUE:
            break;
        case E1000_RX_DELIVER: {
            struct nx_net_mbuf *m = nx_net_mbuf_get(d.pool, true);
            if (!m || !nx_net_mbuf_append(m, buffer(d.rx_buffers, d.rx_next), length)) {
                if (m) nx_net_mbuf_freem(m);
                ++d.stats.rx_dropped;
                break;
            }
            ++d.stats.rx_frames;
            d.stats.rx_bytes += length;
            d.input(d.context, m);
            break;
        }
        }
        desc->status = 0;
        desc->errors = 0;
        e1000_write(&nic, E1000_RDT, d.rx_next);   /* hand the slot back */
        d.rx_next = (d.rx_next + 1) % E1000_RING;
    }
}
void e1000_poll(void)
{
    if (!d.started) return;
    uintptr_t flags = proc_irq_save();
    uint32_t cause = pending;
    pending = 0;
    proc_irq_restore(flags);
    if (cause & E1000_ICR_LSC) {
        bool link = e1000_read(&nic, E1000_STATUS) & STATUS_LU;
        if (link != d.link) ++d.stats.link_changes;
        d.link = link;
    }
    if (cause & E1000_ICR_RXO) ++d.stats.overruns;
    d.stats.rx_missed += e1000_read(&nic, E1000_MPC);
    reclaim();
    receive();
    /* More completed descriptors than one pass handles: run again. */
    if (d.rx[d.rx_next].status & E1000_RXD_DD) net_worker_submit(&d.work);
}

const uint8_t *e1000_mac(void) { return d.mac; }
bool e1000_link_up(void) { return d.link; }
struct e1000_stats e1000_stats(void) { return d.stats; }
/* Test hooks: synthesize completed receive descriptors at the ring head, as
 * the device would write them, to exercise validation on real rings. The
 * receiver is paused and the device head moved past them, so ring ownership
 * stays consistent; e1000_test_rx_resume re-enables reception. */
void e1000_test_rx_pause(unsigned ahead);
void e1000_test_rx_pause(unsigned ahead)
{
    e1000_write(&nic, E1000_RCTL, 0);
    e1000_write(&nic, E1000_RDH, (d.rx_next + ahead) % E1000_RING);
}
void e1000_test_rx_resume(void);
void e1000_test_rx_resume(void)
{
    e1000_write(&nic, E1000_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);
}
void e1000_test_fake_rx(unsigned ahead, uint16_t length, uint8_t status, uint8_t errors);
void e1000_test_fake_rx(unsigned ahead, uint16_t length, uint8_t status, uint8_t errors)
{
    volatile struct rx_desc *desc = &d.rx[(d.rx_next + ahead) % E1000_RING];
    desc->length = length;
    desc->errors = errors;
    desc->status = status | E1000_RXD_DD;
}
void e1000_test_reclaim(void);
void e1000_test_reclaim(void) { reclaim(); }
unsigned e1000_test_ready(void);
unsigned e1000_test_ready(void)
{
    unsigned ready = 0;
    while (ready < E1000_RING && (d.rx[(d.rx_next + ready) % E1000_RING].status & E1000_RXD_DD)) ++ready;
    return ready;
}
