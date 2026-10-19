#ifndef NEXIX_VGA_H
#define NEXIX_VGA_H
#include <stddef.h>
#include <stdint.h>
/* IRQs disabled; buffer is an 80 by 25 VGA text surface. */
void vga_init(volatile uint16_t *buffer);
void vga_write(const uint8_t *bytes, size_t count);
#endif
