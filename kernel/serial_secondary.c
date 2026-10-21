/* Native PC COM2 transport for the second V7 terminal. IRQs are disabled by
 * callers, as for COM1; no interrupt path allocates or waits for a reader. */
#include "io.h"
#include "serial.h"
#include "tty.h"
#include <nexix/modem.h>
#define COM2 0x2f8
#define TX_CAPACITY 4096
static uint8_t tx[TX_CAPACITY],pending_status;
static unsigned tx_head,tx_count,tx_burst=1;
static bool attached;
static uint8_t line_status(void)
{
    uint8_t s=inb(COM2+5);
    if(s&(1|16))pending_status|=s&0x1e;
    return s;
}
static void transmit(void)
{
    if(!(line_status()&0x20))return;
    for(unsigned n=0;n<tx_burst && tx_count;++n){
        outb(COM2,tx[tx_head]);tx_head=(tx_head+1)%TX_CAPACITY;--tx_count;
    }
    if(!tx_count)outb(COM2+1,inb(COM2+1)&~2U);
}
static bool enqueue(void *context,const uint8_t *bytes,size_t count)
{
    (void)context;
    if(count>TX_CAPACITY-tx_count)return false;
    for(size_t i=0;i<count;++i)tx[(tx_head+tx_count+i)%TX_CAPACITY]=bytes[i];
    tx_count+=count;outb(COM2+1,inb(COM2+1)|2);transmit();return true;
}
static bool idle(void *context)
{(void)context;return !tx_count && (line_status()&0x40);}
static void flush(void *context)
{(void)context;tx_head=tx_count=0;outb(COM2+1,inb(COM2+1)&~2U);}
static void format(void *context,unsigned rate,uint8_t control)
{
    (void)context;
    while(tx_count)transmit();
    while(!(line_status()&0x40))__asm__ volatile("pause");
    uint8_t ier=inb(COM2+1);outb(COM2+1,0);
    /* Rate 134 is 134.5 baud: 115200/134.5 rounds to 857. */
    uint16_t divisor=rate==134?857:115200/rate;
    outb(COM2+3,0x80);outb(COM2,divisor&255);outb(COM2+1,divisor>>8);
    outb(COM2+3,control);outb(COM2+1,ier);
}
static void outputs(void *context,bool dtr,bool rts)
{(void)context;outb(COM2+4,(inb(COM2+4)&~3U)|(dtr?1:0)|(rts?2:0));}
static unsigned status(void *context)
{
    (void)context;unsigned s=inb(COM2+6);
    return (s&128?NX_MDM_DCD:0)|(s&16?NX_MDM_CTS:0)|
        (s&32?NX_MDM_DSR:0)|(s&64?NX_MDM_RI:0);
}
static const struct tty_driver driver={NULL,enqueue,idle,flush,format,outputs,status};
bool serial_secondary_init(void)
{
    if(attached)return true;
    /* An absent UART reads 0xff. Leave IRQ3 masked and line one unbound. */
    uint8_t scratch=inb(COM2+7);
    outb(COM2+7,0x5a);
    bool present=inb(COM2+7)==0x5a;
    outb(COM2+7,scratch);
    if(!present)return false;
    outb(COM2+1,0);outb(COM2+3,0x80);outb(COM2,1);outb(COM2+1,0);
    outb(COM2+3,3);outb(COM2+2,7);outb(COM2+4,0x0b);
    /* Only a working 16550 FIFO accepts more than one byte per THRE. */
    tx_burst=(inb(COM2+2)&0xc0)==0xc0?16:1;
    pending_status=0;tx_head=tx_count=0;
    if(tty_attach(1,&driver))return false;
    attached=true;
    outb(COM2+1,0x0d);
    outb(0x21,inb(0x21)&~(1U<<3));
    return true;
}
void serial_secondary_interrupt(void)
{
    if(!attached)return;
    for(unsigned n=0;n<256 && (!(inb(COM2+2)&1)||(pending_status&16));++n){
        transmit();
        uint8_t s=line_status()|pending_status;pending_status=0;
        if(s&1){
            uint8_t byte=inb(COM2);
            unsigned errors=(s&4?NX_RX_PARITY:0)|(s&8?NX_RX_FRAME:0)|
                (s&16?NX_RX_BREAK:0)|(s&2?NX_RX_OVERRUN:0);
            tty_receive_line(1,byte,errors);
        }else{
            if(s&16)tty_receive_line(1,0,NX_RX_BREAK|NX_RX_FRAME);
            tty_modem_line(1,status(NULL));
        }
    }
}
