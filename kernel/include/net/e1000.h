#ifndef NEXIX_NET_E1000_H
#define NEXIX_NET_E1000_H
#include "pci.h"

/* QEMU's default "e1000" model: Intel 82540EM. Task 416 discovers it, enables
 * memory decoding and bus mastering, and maps BAR0; task 417 drives it.
 */
#define E1000_VENDOR 0x8086u
#define E1000_DEVICE_82540EM 0x100eu
#define E1000_REGISTER_BYTES 0x20000u   /* BAR0 register window */
#define E1000_CTRL 0x0000u
#define E1000_STATUS 0x0008u

struct e1000_device {
    const struct pci_function *pci;
    volatile uint32_t *registers;   /* Uncached MMIO mapping of BAR0. */
    uint8_t irq;                    /* 8259 IRQ, or PCI_IRQ_NONE. */
};
/* Returns NULL when no supported adapter exists or it cannot be enabled. */
const struct e1000_device *e1000_discover(void);
static inline uint32_t e1000_read(const struct e1000_device *nic, uint32_t offset)
{ return nic->registers[offset / 4]; }
void e1000_selftest(void);
#endif
