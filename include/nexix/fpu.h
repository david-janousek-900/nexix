#ifndef NEXIX_ABI_FPU_H
#define NEXIX_ABI_FPU_H
#include <stdint.h>
/* FXSAVE64 format; AVX/XSAVE are outside the current native ABI. */
struct nx_fpu_state { uint8_t bytes[512]; } __attribute__((aligned(16)));
_Static_assert(sizeof(struct nx_fpu_state) == 512, "FP state size");
#endif
