#include "pci.h"

#define VENDOR 0x00
#define COMMAND 0x04
#define CLASS 0x08
#define HEADER 0x0c
#define BAR0 0x10
#define INTERRUPT 0x3c

static uint32_t read(const struct pci_bus *bus, const struct pci_function *f, uint8_t offset)
{
    return bus->ops->read(bus->ops->context, f->bus, f->device, f->function, offset);
}
static void write(const struct pci_bus *bus, const struct pci_function *f, uint8_t offset, uint32_t value)
{
    bus->ops->write(bus->ops->context, f->bus, f->device, f->function, offset, value);
}

/* Size one BAR with decoding disabled; returns the number of slots consumed. */
static unsigned size_bar(const struct pci_bus *bus, const struct pci_function *f,
                         unsigned index, struct pci_bar *bar)
{
    uint8_t offset = BAR0 + 4 * index;
    uint32_t low = read(bus, f, offset);
    *bar = (struct pci_bar){ 0 };
    bool io = low & 1, wide = !io && ((low >> 1) & 3) == 2;
    if (wide && index == 5) return 1;   /* malformed: no upper half */
    uint32_t high = wide ? read(bus, f, offset + 4) : 0;
    write(bus, f, offset, UINT32_MAX);
    if (wide) write(bus, f, offset + 4, UINT32_MAX);
    uint32_t mask_low = read(bus, f, offset), mask_high = wide ? read(bus, f, offset + 4) : 0;
    write(bus, f, offset, low);
    if (wide) write(bus, f, offset + 4, high);
    uint64_t mask, size;
    if (io) {
        /* x86 ports are 16 bits; devices may leave the upper half zero. */
        uint32_t m = (mask_low & ~UINT32_C(3)) | UINT32_C(0xffff0000);
        if (m == UINT32_C(0xffff0000)) return 1;
        size = (uint32_t)(~m + 1);
        mask = m;
    } else if (wide) {
        mask = ((uint64_t)mask_high << 32 | mask_low) & ~UINT64_C(15);
        if (!mask) return 2;
        size = ~mask + 1;
    } else {
        uint32_t m = mask_low & ~UINT32_C(15);
        if (!m) return 1;
        size = (uint32_t)(~m + 1);
        mask = m;
    }
    if (!size || (size & (size - 1)) || (mask & (size - 1))) return wide ? 2 : 1;   /* malformed */
    bar->size = size;
    bar->base = ((uint64_t)high << 32 | low) & (io ? ~UINT64_C(3) : ~UINT64_C(15));
    bar->kind = io ? PCI_BAR_IO : wide ? PCI_BAR_MEM64 : PCI_BAR_MEM32;
    bar->prefetchable = !io && (low & 8);
    return wide ? 2 : 1;
}

static void probe(struct pci_bus *bus, uint8_t b, uint8_t d, uint8_t fn)
{
    struct pci_function f = { .bus = b, .device = d, .function = fn };
    uint32_t id = read(bus, &f, VENDOR);
    if ((id & 0xffff) == 0xffff) return;
    if (bus->count == PCI_MAX_FUNCTIONS) { bus->truncated = true; return; }
    uint32_t class = read(bus, &f, CLASS), interrupt = read(bus, &f, INTERRUPT);
    f.vendor = id & 0xffff;
    f.device_id = id >> 16;
    f.revision = class & 0xff;
    f.prog_if = class >> 8;
    f.subclass = class >> 16;
    f.class_code = class >> 24;
    f.header = (read(bus, &f, HEADER) >> 16) & 0x7f;
    f.irq_pin = (interrupt >> 8) & 0xff;
    f.irq_line = interrupt & 0xff;
    if (f.irq_pin < 1 || f.irq_pin > 4) f.irq_pin = 0;
    if (!f.irq_pin || f.irq_line >= 16) f.irq_line = PCI_IRQ_NONE;
    if (f.header == 0) {
        uint32_t command = read(bus, &f, COMMAND);
        write(bus, &f, COMMAND, command & ~(uint32_t)(PCI_COMMAND_IO | PCI_COMMAND_MEMORY) & 0xffff);
        for (unsigned i = 0; i < 6;) i += size_bar(bus, &f, i, &f.bars[i]);
        write(bus, &f, COMMAND, command & 0xffff);
    }
    bus->functions[bus->count++] = f;
}

void pci_scan(struct pci_bus *bus, const struct pci_config_ops *ops)
{
    bus->ops = ops;
    bus->count = 0;
    bus->truncated = false;
    for (unsigned b = 0; b < 256; ++b)
        for (uint8_t d = 0; d < 32; ++d) {
            struct pci_function first = { .bus = (uint8_t)b, .device = d };
            if ((read(bus, &first, VENDOR) & 0xffff) == 0xffff) continue;
            bool multi = (read(bus, &first, HEADER) >> 16) & 0x80;
            for (uint8_t fn = 0; fn < (multi ? 8 : 1); ++fn) probe(bus, (uint8_t)b, d, fn);
        }
}
const struct pci_function *pci_find(const struct pci_bus *bus, uint16_t vendor, uint16_t device_id)
{
    for (size_t i = 0; i < bus->count; ++i)
        if (bus->functions[i].vendor == vendor && bus->functions[i].device_id == device_id)
            return &bus->functions[i];
    return NULL;
}
uint16_t pci_command(const struct pci_bus *bus, const struct pci_function *f)
{
    return read(bus, f, COMMAND) & 0xffff;
}
bool pci_enable(const struct pci_bus *bus, const struct pci_function *f, uint16_t bits)
{
    /* The upper half is status: write zeros so no RW1C bit is cleared. */
    uint16_t command = (pci_command(bus, f) | bits) & ~PCI_COMMAND_INTX_DISABLE;
    write(bus, f, COMMAND, command);
    return (pci_command(bus, f) & (bits | PCI_COMMAND_INTX_DISABLE)) == bits;
}
