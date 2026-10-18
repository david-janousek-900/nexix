/* Model the 16550 register interface around the production async TX queue. */
#include "serial.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t regs[8],wire[32768],input[32],received[32];
static size_t used,head,count,got;
static bool ready;
uint8_t inb(uint16_t port)
{
    if(port==0x21)return 255;
    unsigned r=port-0x3f8;assert(r<8);
    if(r==5)return (ready?0x60:0)|(count?1:0);
    if(r==2)return count?4:ready && (regs[1]&2)?2:1;
    if(!r && !(regs[3]&128)){assert(count);uint8_t c=input[head++];--count;return c;}
    return regs[r];
}
void outb(uint16_t port,uint8_t value)
{
    if(port==0x21)return;
    unsigned r=port-0x3f8;assert(r<8);
    if(!r && !(regs[3]&128)){assert(ready && used<sizeof(wire));wire[used++]=value;}
    else regs[r]=value;
}
void tty_receive(uint8_t c){assert(got<sizeof(received));received[got++]=c;}
int main(void)
{
    ready=true;serial_init();serial_receive_enable();assert(regs[1]==5);
    uint8_t bytes[4096];for(unsigned i=0;i<sizeof(bytes);++i)bytes[i]=i*19;
    ready=false;
    assert(serial_enqueue(bytes,4096) && !used && (regs[1]&2));
    assert(!serial_enqueue(bytes,1)); /* Full frame rejection is atomic. */
    ready=true;serial_interrupt();assert(used==4096 && !memcmp(wire,bytes,4096) && !(regs[1]&2));
    /* Wrap the ring with two variable-sized frames and simultaneous RX. */
    ready=false;assert(serial_enqueue(bytes,518) && serial_enqueue(bytes,70));
    memcpy(input,"receive",7);head=0;count=7;ready=true;serial_interrupt();
    assert(used==4684 && !memcmp(wire+4096,bytes,518) && !memcmp(wire+4614,bytes,70));
    assert(got==7 && !memcmp(received,"receive",7) && !(regs[1]&2));
    ready=false;assert(serial_enqueue(bytes,128));ready=true;serial_putc('Z');
    assert(used==4813 && !memcmp(wire+4684,bytes,128) && wire[4812]=='Z');
    serial_configure(9600,0);assert(regs[1]==5 && regs[3]==3);
    puts("PASS: serial atomic TX capacity, ring wrap, RX/TX interrupts, IER clearing and ordered console/configuration drain");
}
