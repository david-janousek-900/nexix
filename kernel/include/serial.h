#ifndef NEXIX_SERIAL_H
#define NEXIX_SERIAL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void serial_init(void);
bool serial_enqueue(const uint8_t *,size_t);
void serial_putc(uint8_t byte);
void serial_configure(unsigned rate, unsigned parity); /* 0 none, 1 odd, 2 even */
void serial_receive_enable(void);
void serial_interrupt(void);
void serial_puts(const char *s);
void serial_hex(uint64_t value);
#endif
