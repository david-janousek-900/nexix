#include "io.h"
#include "serial.h"
#define COM1 0x3f8
void serial_init(void)
{
    outb(COM1 + 1, 0);
    outb(COM1 + 3, 0x80);
    outb(COM1, 1);
    outb(COM1 + 1, 0);
    outb(COM1 + 3, 3);
    outb(COM1 + 2, 0xc7);
    outb(COM1 + 4, 0x0b);
}
void serial_puts(const char *s)
{
    while (*s) {
        while (!(inb(COM1 + 5) & 0x20))
            __asm__ volatile ("pause");
        outb(COM1, (uint8_t)*s++);
    }
}
void serial_hex(uint64_t value)
{
    char buffer[19] = "0x0000000000000000";
    const char *digits = "0123456789abcdef";
    for (unsigned i = 0; i < 16; ++i)
        buffer[17 - i] = digits[(value >> (i * 4)) & 15];
    serial_puts(buffer);
}
