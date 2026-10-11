#ifndef NEXIX_FPU_H
#define NEXIX_FPU_H
#include <stdbool.h>
#include <nexix/fpu.h>
void fpu_init(void);
void fpu_default(struct nx_fpu_state *);
void fpu_save(struct nx_fpu_state *);
void fpu_restore(const struct nx_fpu_state *);
bool fpu_valid(const struct nx_fpu_state *);
#endif
