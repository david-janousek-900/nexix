#include "console.h"
#include "serial.h"
#include "vga.h"
static enum boot_console selected;
void console_init(enum boot_console mode)
{
    vga_init((volatile uint16_t *)0xb8000);
    console_select(mode);
}
void console_select(enum boot_console mode)
{
    selected=mode;
    if(mode!=BOOT_CONSOLE_VGA)serial_init();
}
enum boot_console console_mode(void){return selected;}
void console_puts(const char *text)
{
    if(selected!=BOOT_CONSOLE_VGA){serial_puts(text);return;}
    const char *end=text;
    while(*end)++end;
    vga_write((const uint8_t *)text,(size_t)(end-text));
}
void console_hex(uint64_t value)
{
    char buffer[19]="0x0000000000000000";
    const char *digits="0123456789abcdef";
    for(unsigned i=0;i<16;++i)buffer[17-i]=digits[(value>>(i*4))&15];
    console_puts(buffer);
}
