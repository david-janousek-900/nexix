#ifndef TEST_RTC_IO_H
#define TEST_RTC_IO_H
#include <stdint.h>
void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
static inline void io_wait(void) {}
#endif
