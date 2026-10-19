#include "vga.h"
#include "io.h"
#define WIDTH 80U
#define HEIGHT 25U
static volatile uint16_t *screen;
static unsigned row, column;
static void cursor(void)
{
    unsigned position = row * WIDTH + column;
    outb(0x3d4, 14); outb(0x3d5, position >> 8);
    outb(0x3d4, 15); outb(0x3d5, position & 255);
}
static void advance(void)
{
    if (column == WIDTH) { column = 0; ++row; }
    if (row < HEIGHT) return;
    for (unsigned i = 0; i < WIDTH * (HEIGHT - 1); ++i)
        screen[i] = screen[i + WIDTH];
    for (unsigned i = WIDTH * (HEIGHT - 1); i < WIDTH * HEIGHT; ++i)
        screen[i] = 0x0720;
    row = HEIGHT - 1;
}
void vga_init(volatile uint16_t *buffer)
{
    screen = buffer; row = column = 0;
    for (unsigned i = 0; i < WIDTH * HEIGHT; ++i) screen[i] = 0x0720;
    outb(0x3d4, 10); outb(0x3d5, 14);
    outb(0x3d4, 11); outb(0x3d5, 15);
    cursor();
}
void vga_write(const uint8_t *bytes, size_t count)
{
    if (!screen) return;
    for (size_t i = 0; i < count; ++i) {
        uint8_t c = bytes[i];
        if (c == '\r') column = 0;
        else if (c == '\n') ++row;
        else if (c == '\b') { if (column) --column; }
        else if (c == '\t') {
            unsigned spaces = 8 - column % 8;
            while (spaces--) { screen[row * WIDTH + column++] = 0x0720; advance(); }
        } else if (c >= 32 && c != 127) {
            screen[row * WIDTH + column++] = 0x0700 | c;
        }
        advance();
    }
    cursor();
}
