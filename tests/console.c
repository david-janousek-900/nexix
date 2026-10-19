#include "console.h"
#include <nexix/klog.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char vga[131072],serial[131072];
static unsigned vga_used,serial_used,vga_inits,serial_inits;
static uint64_t ticks;
uint64_t timer_ticks(void){return ticks;}
void vga_init(volatile uint16_t *p){assert(p==(void *)0xb8000);++vga_inits;}
void vga_write(const uint8_t *p,size_t n)
{assert(vga_used+n<sizeof(vga));memcpy(vga+vga_used,p,n);vga_used+=n;}
void serial_init(void){++serial_inits;}
void serial_putc(uint8_t c){assert(serial_used+1<sizeof(serial));serial[serial_used++]=c;}
int main(void)
{
    console_init(BOOT_CONSOLE_VGA);
    console_puts("boot\r\n");console_hex(0xdeadbeef);console_puts("\n");
    assert(vga_inits==1 && !serial_inits && !serial_used);
    assert(!strcmp(vga,"[    0.000000] boot\r\n[    0.000000] 0x00000000deadbeef\r\n"));
    unsigned saved=vga_used;
    ticks=12345;console_select(BOOT_CONSOLE_SERIAL);
    console_puts("panic ");console_hex(UINT64_MAX);console_puts("\r\n");
    assert(serial_inits==1 && vga_used==saved);
    assert(!strcmp(serial,"[  123.450000] panic 0xffffffffffffffff\r\n"));
    char snapshot[NX_KLOG_CAPACITY+1];memset(snapshot,0xa5,sizeof(snapshot));
    size_t n=console_log_copy(NULL,0);
    assert(n==console_log_copy(snapshot,n-1) && snapshot[0]==(char)0xa5);
    assert(console_log_copy(snapshot,sizeof(snapshot))==n && snapshot[n]==(char)0xa5);
    snapshot[n]=0;assert(!strchr(snapshot,'\r') && strstr(snapshot,"[  123.450000] panic"));
    console_select(BOOT_CONSOLE_MULTI);console_puts("multi\n");
    assert(vga_used==saved);
    console_select(BOOT_CONSOLE_VGA);saved=serial_used;console_puts("local\n");
    assert(serial_used==saved && vga_inits==1);
    for(unsigned i=0;i<1000;++i){char line[64];snprintf(line,sizeof(line),"message %04u\n",i);console_puts(line);}
    n=console_log_copy(snapshot,sizeof(snapshot));snapshot[n]=0;
    assert(n<NX_KLOG_CAPACITY && snapshot[0]=='[' && !strstr(snapshot,"message 0000"));
    assert(strstr(snapshot,"message 0999\n") && !strstr(snapshot,"panic"));
    char again[NX_KLOG_CAPACITY];assert(console_log_copy(again,sizeof(again))==n && !memcmp(again,snapshot,n));
    console_init(BOOT_CONSOLE_VGA);ticks=0;
    for(unsigned i=0;i<2048;++i)console_puts("\n");
    assert(console_log_copy(snapshot,sizeof(snapshot))==NX_KLOG_CAPACITY);
    assert(snapshot[0]=='[' && snapshot[NX_KLOG_CAPACITY-1]=='\n');
    puts("PASS: timestamped fragmented messages, exclusive routing, bounded non-destructive snapshots and line-aligned ring wrap");
}
