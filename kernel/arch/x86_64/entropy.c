#include "entropy.h"
/* CPUID gates the instruction on older CPUs. RDRAND failure is bounded and
 * propagated; its architected carry flag is mandatory, even for zero data. */
bool entropy_arch_word(uint64_t *word)
{
    uint32_t a=1,b,c,d;
    __asm__ volatile("cpuid":"+a"(a),"=b"(b),"=c"(c),"=d"(d));
    if(!(c&(UINT32_C(1)<<30)))return false;
    for(unsigned retry=0;retry<10;++retry) {
        unsigned char success;
        __asm__ volatile("rdrand %0; setc %1":"=r"(*word),"=qm"(success)::"cc");
        if(success)return true;
    }
    return false;
}
