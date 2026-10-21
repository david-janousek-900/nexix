#include "pci.h"
#include "console.h"
#include "io.h"
#include "paging.h"
#include "process.h"

#define CONFIG_ADDRESS 0xcf8
#define CONFIG_DATA 0xcfc

static uint32_t address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    return UINT32_C(0x80000000) | (uint32_t)bus << 16 | (uint32_t)(device & 31) << 11 |
        (uint32_t)(function & 7) << 8 | (offset & 0xfc);
}
/* The address/data pair is not atomic: keep IRQs off across both accesses. */
static uint32_t port_read(void *context, uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    (void)context;
    uintptr_t flags = proc_irq_save();
    outl(CONFIG_ADDRESS, address(bus, device, function, offset));
    uint32_t value = inl(CONFIG_DATA);
    proc_irq_restore(flags);
    return value;
}
static void port_write(void *context, uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    (void)context;
    uintptr_t flags = proc_irq_save();
    outl(CONFIG_ADDRESS, address(bus, device, function, offset));
    outl(CONFIG_DATA, value);
    proc_irq_restore(flags);
}
const struct pci_config_ops pci_port_ops = { port_read, port_write, NULL };

static struct pci_bus bus;
struct pci_bus *pci_system(void) { return &bus; }

void pci_init(void)
{
    /* One line only: the boot screen must keep its earlier messages visible. */
    pci_scan(&bus, &pci_port_ops);
    if (bus.truncated) console_puts("pci: function table full; later functions ignored\r\n");
    console_puts("PCI SCAN OK\r\n");
}
void pci_log(void)
{
    for (size_t i = 0; i < bus.count; ++i) {
        const struct pci_function *f = &bus.functions[i];
        console_puts("pci ");
        console_hex((uint64_t)f->bus << 16 | (uint64_t)f->device << 8 | f->function);
        console_puts(" id="); console_hex((uint64_t)f->vendor << 16 | f->device_id);
        console_puts(" class="); console_hex((uint64_t)f->class_code << 16 | (uint64_t)f->subclass << 8 | f->prog_if);
        console_puts("\r\n");
    }
}

volatile void *pci_map_bar(const struct pci_function *f, unsigned index)
{
    if (index >= 6) return NULL;
    const struct pci_bar *bar = &f->bars[index];
    if ((bar->kind != PCI_BAR_MEM32 && bar->kind != PCI_BAR_MEM64) || !bar->base ||
        (bar->base & (bar->size - 1)) || bar->size > MMIO_SIZE) return NULL;
    uintptr_t flags = proc_irq_save();
    uintptr_t mapped = paging_map_mmio(bar->base, bar->size);
    proc_irq_restore(flags);
    return (volatile void *)mapped;
}
