#ifndef NEXIX_SERIAL_H
#define NEXIX_SERIAL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void serial_init(void);
bool serial_enqueue(const uint8_t *,size_t);
void serial_putc(uint8_t byte);
void serial_configure(unsigned rate, unsigned parity); /* 0 none, 1 odd, 2 even */
void serial_format(unsigned rate,uint8_t line_control);
void serial_flush(void);
bool serial_idle(void);
void serial_dtr(bool);
unsigned serial_modem_status(void);
void serial_modem_outputs(bool dtr,bool rts);
void serial_receive_enable(void);
void serial_interrupt(void);
void serial_puts(const char *s);
void serial_hex(uint64_t value);
#endif
