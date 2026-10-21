#ifndef NEXIX_NET_E1000_H
#define NEXIX_NET_E1000_H
#include "pci.h"
#include "net/mbuf.h"

/* QEMU's default "e1000" model: Intel 82540EM. Task 416 discovers it, enables
 * memory decoding and bus mastering, and maps BAR0; task 417 drives it with
 * legacy descriptors, copying frames between mbufs and fixed DMA buffers.
 */
#define E1000_VENDOR 0x8086u
#define E1000_DEVICE_82540EM 0x100eu
#define E1000_REGISTER_BYTES 0x20000u   /* BAR0 register window */

#define E1000_CTRL 0x0000u
#define E1000_STATUS 0x0008u
#define E1000_EERD 0x0014u
#define E1000_ICR 0x00c0u
#define E1000_IMS 0x00d0u
#define E1000_IMC 0x00d8u
#define E1000_RCTL 0x0100u
#define E1000_TCTL 0x0400u
#define E1000_TIPG 0x0410u
#define E1000_MPC 0x4010u
#define E1000_RDBAL 0x2800u
#define E1000_RDBAH 0x2804u
#define E1000_RDLEN 0x2808u
#define E1000_RDH 0x2810u
#define E1000_RDT 0x2818u
#define E1000_TDBAL 0x3800u
#define E1000_TDBAH 0x3804u
#define E1000_TDLEN 0x3808u
#define E1000_TDH 0x3810u
#define E1000_TDT 0x3818u
#define E1000_MTA 0x5200u
#define E1000_RAL0 0x5400u
#define E1000_RAH0 0x5404u

#define E1000_ICR_TXDW (1u << 0)
#define E1000_ICR_LSC (1u << 2)
#define E1000_ICR_RXDMT0 (1u << 4)
#define E1000_ICR_RXO (1u << 6)
#define E1000_ICR_RXT0 (1u << 7)

/* Ring geometry: 32 legacy descriptors each (one page holds 256), 2 KiB
 * buffers. A ring of N holds N - 1 frames in flight. */
#define E1000_RING 32u
#define E1000_BUFFER 2048u
#define E1000_FRAME_MIN 14u      /* Ethernet header; the device pads to 60. */
#define E1000_FRAME_MAX 1514u    /* Without FCS; the device strips/appends it. */

/* Legacy receive descriptor bits. */
#define E1000_RXD_DD 0x01u
#define E1000_RXD_EOP 0x02u
enum e1000_rx_verdict { E1000_RX_DELIVER, E1000_RX_MALFORMED, E1000_RX_CONTINUE };
/* Classify one completed receive descriptor. discarding tracks a frame that
 * spans descriptors: every piece is dropped and it counts as one malformed
 * frame, reported on its final (EOP) piece. Pure; host tested. */
static inline enum e1000_rx_verdict e1000_rx_classify(uint16_t length, uint8_t status,
                                                       uint8_t errors, bool *discarding)
{
    if (!(status & E1000_RXD_EOP)) { *discarding = true; return E1000_RX_CONTINUE; }
    bool spanned = *discarding;
    *discarding = false;
    if (spanned || errors || length < E1000_FRAME_MIN || length > E1000_FRAME_MAX)
        return E1000_RX_MALFORMED;
    return E1000_RX_DELIVER;
}

struct e1000_stats {
    uint64_t rx_frames, rx_bytes, rx_malformed, rx_dropped, rx_missed;
    uint64_t tx_frames, tx_bytes, tx_full, tx_rejected, tx_completed;
    uint64_t irqs, link_changes, overruns;
};
struct e1000_device {
    const struct pci_function *pci;
    volatile uint32_t *registers;   /* Uncached MMIO mapping of BAR0. */
    uint8_t irq;                    /* 8259 IRQ, or PCI_IRQ_NONE. */
};
/* Returns NULL when no supported adapter exists or it cannot be enabled. */
const struct e1000_device *e1000_discover(void);
static inline uint32_t e1000_read(const struct e1000_device *nic, uint32_t offset)
{ return nic->registers[offset / 4]; }
static inline void e1000_write(const struct e1000_device *nic, uint32_t offset, uint32_t value)
{ nic->registers[offset / 4] = value; }

/* Received frames are copied into mbuf chains from pool and handed to input,
 * which takes ownership, in network-worker context. */
typedef void (*e1000_input_fn)(void *context, struct nx_net_mbuf *frame);

/* Process context with the network worker running. Resets the adapter, reads
 * its MAC, allocates rings/buffers, programs RX/TX and attaches its IRQ. On
 * failure everything acquired is released and false returned. */
bool e1000_start(struct nx_net_mbuf_pool *pool, e1000_input_fn input, void *context);
/* Network-worker context. Masks and detaches the IRQ, disables RX/TX, resets
 * the adapter so DMA has stopped, then frees every ring and buffer page. */
void e1000_stop(void);
bool e1000_started(void);
/* Worker context. Consumes frame: a malformed length (< 14 or > 1514 bytes)
 * or a full ring drops it (counted) and returns false. */
bool e1000_transmit(struct nx_net_mbuf *frame);
/* Worker context: reclaim completed transmits and deliver received frames.
 * The IRQ handler schedules this through the worker; callers may also poll. */
void e1000_poll(void);
const uint8_t *e1000_mac(void);
bool e1000_link_up(void);
struct e1000_stats e1000_stats(void);

void e1000_selftest(void);
#endif
