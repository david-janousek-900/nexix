#include "console.h"
#include "serial.h"
#include "vga.h"
#include "interrupts.h"
#include <nexix/klog.h>
#include <nexix/syscall.h>
static enum boot_console selected;
static char messages[NX_KLOG_CAPACITY];
static size_t head, count;
static bool line_start=true, overwritten;
static void device(uint8_t c)
{
    if(selected==BOOT_CONSOLE_VGA)vga_write(&c,1);
    else serial_putc(c);
}
static void emit(char c)
{
    if(count==sizeof(messages)){
        overwritten=messages[head]!='\n';
        head=(head+1)%sizeof(messages);--count;
    }
    messages[(head+count++)%sizeof(messages)]=c;
    if(c=='\n')device('\r');
    device((uint8_t)c);
}
static void timestamp(void)
{
    uint64_t ticks=timer_ticks(),seconds=ticks/NX_CLK_TCK;
    char digits[20];unsigned n=0;
    do {digits[n++]='0'+seconds%10;seconds/=10;} while(seconds);
    emit('[');
    for(unsigned i=n;i<5;++i)emit(' ');
    while(n)emit(digits[--n]);
    emit('.');
    unsigned fraction=(ticks%NX_CLK_TCK)*(1000000U/NX_CLK_TCK);
    for(unsigned divisor=100000;divisor;divisor/=10)emit('0'+fraction/divisor%10);
    emit(']');emit(' ');
}
void console_init(enum boot_console mode)
{
    head=count=0;line_start=true;overwritten=false;
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
    while(*text){
        char c=*text++;
        if(c=='\r')continue;
        if(line_start){timestamp();line_start=false;}
        emit(c);
        if(c=='\n')line_start=true;
    }
}
void console_hex(uint64_t value)
{
    char buffer[19]="0x0000000000000000";
    const char *digits="0123456789abcdef";
    for(unsigned i=0;i<16;++i)buffer[17-i]=digits[(value>>(i*4))&15];
    console_puts(buffer);
}
size_t console_log_copy(char *destination,size_t capacity)
{
    size_t skip=0;
    if(overwritten){
        while(skip<count && messages[(head+skip)%sizeof(messages)]!='\n')++skip;
        if(skip<count)++skip;
    }
    size_t length=count-skip;
    if(capacity>=length && destination)
        for(size_t i=0;i<length;++i)destination[i]=messages[(head+skip+i)%sizeof(messages)];
    return length;
}
