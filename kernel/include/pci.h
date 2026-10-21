#ifndef NEXIX_PCI_H
#define NEXIX_PCI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* PCI configuration mechanism #1 enumeration (task 416). The decoding core
 * reaches configuration space only through these operations, so host tests
 * can supply a simulated bus. Offsets are dword aligned.
 */
struct pci_config_ops {
    uint32_t (*read)(void *context, uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
    void (*write)(void *context, uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
    void *context;
};

#define PCI_MAX_FUNCTIONS 32u
#define PCI_COMMAND_IO 0x0001u
#define PCI_COMMAND_MEMORY 0x0002u
#define PCI_COMMAND_MASTER 0x0004u
#define PCI_COMMAND_INTX_DISABLE 0x0400u
#define PCI_IRQ_NONE 0xffu

enum pci_bar_kind { PCI_BAR_NONE, PCI_BAR_IO, PCI_BAR_MEM32, PCI_BAR_MEM64 };
struct pci_bar {
    uint64_t base, size;
    enum pci_bar_kind kind;
    bool prefetchable;
};
struct pci_function {
    uint8_t bus, device, function, header;
    uint16_t vendor, device_id;
    uint8_t class_code, subclass, prog_if, revision;
    /* Legacy INTx routing from firmware: pin 1..4 (0 = none) and the 8259 IRQ
     * the line is routed to, or PCI_IRQ_NONE when absent or not routable. */
    uint8_t irq_pin, irq_line;
    struct pci_bar bars[6];   /* Header type 0 only; the upper half of a 64-bit BAR is NONE. */
};
struct pci_bus {
    const struct pci_config_ops *ops;
    size_t count;
    bool truncated;           /* More functions existed than PCI_MAX_FUNCTIONS. */
    struct pci_function functions[PCI_MAX_FUNCTIONS];
};

/* Scan every bus/device/function, honouring the multifunction header bit.
 * BARs are sized with decoding disabled; the command register is restored. */
void pci_scan(struct pci_bus *, const struct pci_config_ops *);
const struct pci_function *pci_find(const struct pci_bus *, uint16_t vendor, uint16_t device_id);
/* Set (and verify) command bits; clears INTx disable when IRQ delivery is wanted. */
bool pci_enable(const struct pci_bus *, const struct pci_function *, uint16_t bits);
uint16_t pci_command(const struct pci_bus *, const struct pci_function *);

/* Kernel binding: port I/O 0xcf8/0xcfc and the boot-time bus. */
extern const struct pci_config_ops pci_port_ops;
struct pci_bus *pci_system(void);
void pci_init(void);
void pci_log(void);   /* One console line per discovered function. */
/* Map one memory BAR into the uncached MMIO window; NULL when not a usable
 * memory BAR (absent, I/O, unaligned, overlapping RAM or window exhausted). */
volatile void *pci_map_bar(const struct pci_function *, unsigned bar);
#endif
