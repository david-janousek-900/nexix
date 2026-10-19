#include "console.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char vga[256],serial[256];
static unsigned vga_used,serial_used,vga_inits,serial_inits;
void vga_init(volatile uint16_t *p){assert(p==(void *)0xb8000);++vga_inits;}
void vga_write(const uint8_t *p,size_t n)
{assert(vga_used+n<sizeof(vga));memcpy(vga+vga_used,p,n);vga_used+=n;}
void serial_init(void){++serial_inits;}
void serial_puts(const char *p)
{size_t n=strlen(p);assert(serial_used+n<sizeof(serial));memcpy(serial+serial_used,p,n);serial_used+=n;}
int main(void)
{
    console_init(BOOT_CONSOLE_VGA);
    console_puts("boot\r\n");console_hex(0xdeadbeef);
    assert(vga_inits==1 && !serial_inits && !serial_used);
    assert(!strcmp(vga,"boot\r\n0x00000000deadbeef"));
    unsigned saved=vga_used;
    console_select(BOOT_CONSOLE_SERIAL);
    console_puts("panic\r\n");console_hex(UINT64_MAX);
    assert(console_mode()==BOOT_CONSOLE_SERIAL && serial_inits==1 && vga_used==saved);
    assert(!strcmp(serial,"panic\r\n0xffffffffffffffff"));
    console_select(BOOT_CONSOLE_MULTI);console_puts("multi");
    assert(vga_used==saved && !strcmp(serial+serial_used-5,"multi"));
    console_select(BOOT_CONSOLE_VGA);saved=serial_used;console_puts("local");
    assert(serial_used==saved && vga_inits==1);
    puts("PASS: exclusive console diagnostics, hex output and mode switching without clearing VGA");
}
