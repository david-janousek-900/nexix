#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pci.h"

/* Simulated configuration space: per-function registers plus BAR size masks.
 * Writing all ones to a BAR reads back its mask; any other write stores the
 * value, as real BAR decoders do. */
struct sim_function {
    bool present;
    uint32_t regs[64];
    uint32_t bar_mask[6];
    bool sizing[6];
    unsigned command_writes;
    uint16_t command_during_sizing;
};
static struct sim_function sim[2][32][8];
static unsigned reads;

static uint32_t sim_read(void *context, uint8_t b, uint8_t d, uint8_t f, uint8_t offset)
{
    (void)context;
    ++reads;
    assert(!(offset & 3) && d < 32 && f < 8);
    if (b >= 2 || !sim[b][d][f].present) return UINT32_MAX;
    struct sim_function *s = &sim[b][d][f];
    unsigned index = (offset - 0x10) / 4;
    if (offset >= 0x10 && offset < 0x28 && s->sizing[index]) return s->bar_mask[index];
    return s->regs[offset / 4];
}
static void sim_write(void *context, uint8_t b, uint8_t d, uint8_t f, uint8_t offset, uint32_t value)
{
    (void)context;
    assert(b < 2 && sim[b][d][f].present);
    struct sim_function *s = &sim[b][d][f];
    unsigned index = (offset - 0x10) / 4;
    if (offset >= 0x10 && offset < 0x28) {
        /* Sizing must happen with decoding disabled. */
        assert(!(s->regs[1] & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)));
        s->sizing[index] = value == UINT32_MAX;
        if (!s->sizing[index]) s->regs[offset / 4] = value;
        return;
    }
    if (offset == 4) {
        ++s->command_writes;
        /* Status (upper half) is RW1C: writing ones would clear it. */
        assert(!(value >> 16));
        s->regs[1] = (s->regs[1] & 0xffff0000) | (value & 0x0407);
        return;
    }
    s->regs[offset / 4] = value;
}
static const struct pci_config_ops ops = { sim_read, sim_write, NULL };

static struct sim_function *add(uint8_t b, uint8_t d, uint8_t f, uint16_t vendor, uint16_t device,
                                uint32_t class, bool multi)
{
    struct sim_function *s = &sim[b][d][f];
    memset(s, 0, sizeof(*s));
    s->present = true;
    s->regs[0] = (uint32_t)device << 16 | vendor;
    s->regs[1] = 0x02900000 | PCI_COMMAND_IO | PCI_COMMAND_MEMORY;   /* status bits set */
    s->regs[2] = class;
    s->regs[3] = multi ? 0x00800000 : 0;
    return s;
}
static void bar(struct sim_function *s, unsigned i, uint32_t value, uint32_t mask)
{
    s->regs[4 + i] = value;
    s->bar_mask[i] = mask;
}

int main(void)
{
    /* Bus 0: host bridge, multifunction ISA/IDE device with a gap, e1000 with a
     * 32-bit memory BAR and I/O BAR, a 64-bit prefetchable BAR device, and a
     * bridge header. Bus 1 holds a device found by scanning every bus. */
    add(0, 0, 0, 0x8086, 0x1237, 0x06000002, false);
    sim[0][0][1].present = true;   /* ignored: function 0 is not multifunction */
    sim[0][0][1].regs[0] = 0x12345678;
    add(0, 1, 0, 0x8086, 0x7000, 0x06010000, true);
    struct sim_function *ide = add(0, 1, 1, 0x8086, 0x7010, 0x01018000, false);
    bar(ide, 4, 0xc041, 0xfffffff1);                      /* 16-byte I/O BAR */
    add(0, 1, 3, 0x8086, 0x7113, 0x06800003, false);       /* function 2 absent */
    struct sim_function *e1000 = add(0, 3, 0, 0x8086, 0x100e, 0x02000003, false);
    bar(e1000, 0, 0xfebc0000, 0xfffe0000);                 /* 128 KiB memory */
    bar(e1000, 1, 0xc001, 0xffffffc1);                     /* 64-byte I/O, upper bits set */
    e1000->regs[15] = 0x0000010b;                          /* INTA#, IRQ 11 */
    struct sim_function *wide = add(0, 4, 0, 0x1af4, 0x1000, 0x02000000, false);
    bar(wide, 0, 0x0000000c, 0xffffc00c);                  /* 64-bit prefetchable, 16 KiB */
    wide->regs[5] = 0x00000008; wide->bar_mask[1] = 0xffffffff;   /* base 0x8_0000_0000 */
    bar(wide, 2, 0xfe000000, 0xfff00000);                  /* 1 MiB, 32-bit */
    bar(wide, 3, 0x12345000, 0xfff0f000);                  /* non-contiguous mask: ignored */
    bar(wide, 5, 0x00000004, 0xfffff004);                  /* 64-bit in last slot: ignored */
    wide->regs[15] = 0x000005ff;                           /* pin 5 is invalid */
    struct sim_function *bridge = add(0, 5, 0, 0x8086, 0x2448, 0x06040000, false);
    bridge->regs[3] = 0x00010000;                          /* header type 1 */
    bar(bridge, 0, 0xffffffff, 0);                         /* not sized for bridges */
    add(1, 7, 0, 0x10ec, 0x8139, 0x02000000, false)->regs[15] = 0x000001ff;   /* line unknown */

    static struct pci_bus bus;
    pci_scan(&bus, &ops);
    assert(bus.count == 8 && !bus.truncated);
    const struct pci_function *f = pci_find(&bus, 0x8086, 0x100e);
    assert(f && f->bus == 0 && f->device == 3 && f->function == 0);
    assert(f->class_code == 2 && f->subclass == 0 && f->revision == 3 && f->header == 0);
    assert(f->irq_pin == 1 && f->irq_line == 11);
    assert(f->bars[0].kind == PCI_BAR_MEM32 && f->bars[0].base == 0xfebc0000 && f->bars[0].size == 0x20000);
    assert(f->bars[1].kind == PCI_BAR_IO && f->bars[1].base == 0xc000 && f->bars[1].size == 64);
    for (unsigned i = 2; i < 6; ++i) assert(f->bars[i].kind == PCI_BAR_NONE);
    assert(e1000->regs[4] == 0xfebc0000 && e1000->regs[5] == 0xc001);   /* restored */
    assert((e1000->regs[1] & 0xffff) == (PCI_COMMAND_IO | PCI_COMMAND_MEMORY));
    assert(e1000->regs[1] >> 16 == 0x0290);

    f = pci_find(&bus, 0x8086, 0x7010);
    assert(f && f->function == 1 && f->bars[4].kind == PCI_BAR_IO && f->bars[4].size == 16 && f->bars[4].base == 0xc040);
    assert(pci_find(&bus, 0x8086, 0x7113)->function == 3 && !pci_find(&bus, 0x5678, 0x1234));
    f = pci_find(&bus, 0x1af4, 0x1000);
    assert(f->bars[0].kind == PCI_BAR_MEM64 && f->bars[0].prefetchable);
    assert(f->bars[0].base == UINT64_C(0x800000000) && f->bars[0].size == 0x4000);
    assert(f->bars[1].kind == PCI_BAR_NONE);                 /* upper half */
    assert(f->bars[2].kind == PCI_BAR_MEM32 && !f->bars[2].prefetchable && f->bars[2].size == 0x100000);
    assert(f->bars[3].kind == PCI_BAR_NONE && f->bars[5].kind == PCI_BAR_NONE);
    assert(f->irq_pin == 0 && f->irq_line == PCI_IRQ_NONE);
    f = pci_find(&bus, 0x8086, 0x2448);
    assert(f->header == 1 && f->bars[0].kind == PCI_BAR_NONE);
    f = pci_find(&bus, 0x10ec, 0x8139);
    assert(f->bus == 1 && f->irq_pin == 1 && f->irq_line == PCI_IRQ_NONE);

    /* Enabling sets the requested bits, clears INTx disable and preserves status. */
    f = pci_find(&bus, 0x8086, 0x100e);
    e1000->regs[1] |= PCI_COMMAND_INTX_DISABLE;
    assert(pci_enable(&bus, f, PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER));
    assert(pci_command(&bus, f) == (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER));
    assert(e1000->regs[1] >> 16 == 0x0290);
    /* A device that refuses a bit is reported as not enabled. */
    unsigned writes = e1000->command_writes;
    assert(!pci_enable(&bus, f, 0x0100) && e1000->command_writes == writes + 1);   /* SERR not emulated */

    /* The function table is bounded. */
    for (uint8_t d = 0; d < 32; ++d)
        if (!sim[1][d][0].present) add(1, d, 0, 0x1234, d, 0xff000000, true);
    for (uint8_t fn = 1; fn < 8; ++fn) add(1, 9, fn, 0x1234, 0x100 + fn, 0xff000000, false);
    pci_scan(&bus, &ops);
    assert(bus.count == PCI_MAX_FUNCTIONS && bus.truncated);
    /* Empty bus. */
    memset(sim, 0, sizeof(sim));
    pci_scan(&bus, &ops);
    assert(bus.count == 0 && !bus.truncated);
    puts("PASS: PCI enumeration, multifunction/gaps, BAR decoding and sizing, INTx routing, command enable and bounds");
    return 0;
}
