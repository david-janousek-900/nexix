#include "console.h"
#include "fpu.h"
#include "serial.h"
#include "interrupts.h"
#include <stddef.h>
static struct nx_fpu_state initial;
static uint32_t mxcsr_mask;
static uint32_t word(const struct nx_fpu_state *s, unsigned offset)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= (uint32_t)s->bytes[offset + i] << (8 * i);
    return value;
}
void fpu_save(struct nx_fpu_state *s)
{
    /* FXSAVE leaves reserved bytes untouched; never expose stack contents in
     * a signal frame or carry padding from a prior process. */
    for (size_t i = 0; i < sizeof(*s); ++i) s->bytes[i] = 0;
    __asm__ volatile ("fxsave64 %0" : "=m"(*s) : : "memory");
}
void fpu_restore(const struct nx_fpu_state *s)
{ __asm__ volatile ("fxrstor64 %0" : : "m"(*s) : "memory"); }
void fpu_default(struct nx_fpu_state *s)
{ for (size_t i = 0; i < sizeof(*s); ++i) s->bytes[i] = initial.bytes[i]; }
bool fpu_valid(const struct nx_fpu_state *s)
{ return !(word(s, 24) & ~mxcsr_mask); }
void fpu_init(void)
{
    uint32_t a = 1, b, c, d;
    __asm__ volatile ("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d));
    if ((d & 0x07000001U) != 0x07000001U) panic("x87/FXSAVE/SSE2 required");
    uintptr_t cr0, cr4;
    __asm__ volatile ("mov %%cr0, %0; mov %%cr4, %1" : "=r"(cr0), "=r"(cr4));
    cr0 = (cr0 & ~12UL) | 0x22UL; /* Clear EM/TS; set MP/NE. */
    cr4 = (cr4 & ~(1UL << 18)) | 0x600UL; /* OSFXSR/OSXMMEXCPT, no OSXSAVE. */
    __asm__ volatile ("mov %0, %%cr0; mov %1, %%cr4; fninit" : : "r"(cr0), "r"(cr4) : "memory");
    fpu_save(&initial);
    mxcsr_mask = word(&initial, 28);
    if (!mxcsr_mask) mxcsr_mask = 0xffbf;
    for (size_t i = 0; i < sizeof(initial); ++i) initial.bytes[i] = 0;
    initial.bytes[0] = 0x7f; initial.bytes[1] = 3;
    initial.bytes[24] = 0x80; initial.bytes[25] = 0x1f;
    fpu_restore(&initial);
    console_puts("FPU OK\r\n");
}
