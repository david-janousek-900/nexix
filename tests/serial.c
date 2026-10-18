/* Model the 16550 register interface around the production async TX queue. */
#include "serial.h"
#include <nexix/tty.h>
#include <nexix/modem.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t regs[8],wire[32768],input[32],received[32];
static size_t used,head,count,got;
static uint8_t rx_status[32],errors_seen[32],status_only;
static bool ready;
static unsigned modem_seen,modem_events;
void tty_modem_status(unsigned s){modem_seen=s;++modem_events;}
uint8_t inb(uint16_t port)
{
    if(port==0x21)return 255;
    unsigned r=port-0x3f8;assert(r<8);
    if(r==5){uint8_t result=(ready?0x60:0)|(count?1|rx_status[head]:0)|status_only;
        if(count)rx_status[head]=0;
        status_only=0;return result;}
    if(r==6){unsigned s=regs[6];regs[6]&=0xf0;return s;}
    if(r==2 && (regs[1]&8) && (regs[6]&15))return 0;
    if(r==2)return status_only?6:count?4:ready && (regs[1]&2)?2:1;
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
void tty_receive_status(uint8_t c,unsigned errors){assert(got<sizeof(received));errors_seen[got]=errors;received[got++]=c;}
int main(void)
{
    ready=true;serial_init();serial_receive_enable();assert(regs[1]==13);
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
    serial_configure(9600,0);assert(regs[1]==13 && regs[3]==3);
    memcpy(input,"pfbo",4);head=0;count=4;
    rx_status[0]=4;rx_status[1]=8;rx_status[2]=16;rx_status[3]=2;
    /* TX polling reads/clears LSR before the RX handler: status must survive. */
    assert(serial_enqueue((const uint8_t *)"x",1));serial_interrupt();
    assert(got==11 && errors_seen[7]==NX_RX_PARITY && errors_seen[8]==NX_RX_FRAME && errors_seen[9]==NX_RX_BREAK && errors_seen[10]==NX_RX_OVERRUN);
    status_only=16;serial_interrupt();assert(got==12 && !received[11] && errors_seen[11]==(NX_RX_BREAK|NX_RX_FRAME));
    serial_format(110,0x1e);assert(regs[3]==0x1e);
    serial_dtr(false);assert(!(regs[4]&1));serial_dtr(true);assert(regs[4]&1);
    ready=false;assert(serial_enqueue(bytes,8) && !serial_idle());serial_flush();assert(!(regs[1]&2));ready=true;assert(serial_idle());
    unsigned previous=modem_events;regs[6]=0xb9;serial_interrupt();
    assert(modem_events==previous+1 && modem_seen==(NX_MDM_DCD|NX_MDM_CTS|NX_MDM_DSR) && regs[6]==0xb0);
    regs[6]=8;serial_interrupt();assert(!modem_seen);
    serial_modem_outputs(false,false);assert((regs[4]&3)==0);
    serial_modem_outputs(true,true);assert((regs[4]&3)==3);
    puts("PASS: modem status interrupts, DTR/RTS, serial atomic TX capacity, ring wrap, RX/TX interrupts, IER clearing and ordered console/configuration drain");
}
