#ifndef NEXIX_CONSOLE_H
#define NEXIX_CONSOLE_H
#include "boot.h"
/* Polling diagnostics remain usable before IRQs and during panic. */
void console_init(enum boot_console mode);
void console_select(enum boot_console mode);
enum boot_console console_mode(void);
void console_puts(const char *text);
void console_hex(uint64_t value);
#endif
