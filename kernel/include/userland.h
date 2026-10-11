#ifndef NEXIX_USERLAND_H
#define NEXIX_USERLAND_H
#include "exec.h"
struct embedded_program { const char *path; const uint8_t *start, *end; };
const struct embedded_program *userland_program(const char *);
void v7_userland_selftest(void);
void rootfs_selftest(void);
void root_exec_selftest(void);
_Noreturn void rootfs_start(void);
#endif
