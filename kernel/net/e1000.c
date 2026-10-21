#include "net/e1000.h"
#include "console.h"
#include "interrupts.h"
#include "memory.h"
#include "paging.h"
#include "process.h"
#define REQUIRE(c) do { if (!(c)) panic("e1000 discovery self-test: " #c); } while (0)

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

void e1000_selftest(void)
{
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
    const struct e1000_device *found = e1000_discover();
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
}
