#ifndef NEXIX_SERIAL_H
#define NEXIX_SERIAL_H
#include <stdint.h>
void serial_init(void);
void serial_puts(const char *s);
void serial_hex(uint64_t value);
#endif
