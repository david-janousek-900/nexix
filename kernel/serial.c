#include "io.h"
#include "serial.h"
#include "tty.h"
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
void serial_putc(uint8_t byte)
{
    while (!(inb(COM1 + 5) & 0x20)) __asm__ volatile ("pause");
    outb(COM1, byte);
}
void serial_puts(const char *s) { while (*s) serial_putc((uint8_t)*s++); }
void serial_configure(unsigned rate, unsigned parity)
{
    /* Caller has disabled IRQs; keep IER masked while DLAB aliases it. */
    while (!(inb(COM1 + 5) & 0x40)) __asm__ volatile ("pause");
    uint8_t ier = inb(COM1 + 1);
    outb(COM1 + 1, 0);
    uint16_t divisor = 115200 / rate;
    outb(COM1 + 3, 0x80);
    outb(COM1, divisor & 255); outb(COM1 + 1, divisor >> 8);
    outb(COM1 + 3, parity ? (parity == 1 ? 0x0a : 0x1a) : 0x03);
    outb(COM1 + 1, ier);
}
void serial_receive_enable(void)
{
    outb(COM1 + 2, 0x07); /* FIFO, one-byte receive threshold. */
    outb(COM1 + 4, 0x0b); /* OUT2 routes the UART interrupt to IRQ4. */
    outb(COM1 + 1, 0x05); /* Receive data and line status, no TX IRQ. */
    outb(0x21, inb(0x21) & ~(1U << 4));
}
void serial_interrupt(void)
{
    /* Bounded IRQ work. Remaining FIFO data retriggers RX or timeout IRQ. */
    for (unsigned i = 0; i < 256 && !(inb(COM1 + 2) & 1); ++i) {
        uint8_t status = inb(COM1 + 5);
        if (status & 1) {
            uint8_t byte = inb(COM1);
            if (!(status & 0x1c)) tty_receive(byte); /* Discard parity/framing/break errors. */
        } else (void)inb(COM1 + 6); /* Acknowledge any modem-status interrupt. */
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
