#include "io.h"
#include "serial.h"
#include "tty.h"
#include <nexix/modem.h>
#define COM1 0x3f8
#define TX_CAPACITY 4096
static uint8_t tx[TX_CAPACITY];
static unsigned tx_head,tx_count,tx_burst=1;
/* LSR error bits clear on read. TX/idle polling must preserve the receive
 * head's status until the corresponding RBR byte (or break event) is taken. */
static uint8_t pending_status;
static uint8_t line_status(void)
{uint8_t status=inb(COM1+5);if(status&(1|16))pending_status|=status&0x1e;return status;}
static void transmit(void)
{
    if(!(line_status()&0x20))return;
    for(unsigned n=0;n<tx_burst && tx_count;++n){
        outb(COM1,tx[tx_head]);tx_head=(tx_head+1)%TX_CAPACITY;--tx_count;
    }
    if(!tx_count)outb(COM1+1,inb(COM1+1)&~2U);
}
bool serial_enqueue(const uint8_t *bytes,size_t count)
{
    if(count>TX_CAPACITY-tx_count)return false;
    for(size_t i=0;i<count;++i)tx[(tx_head+tx_count+i)%TX_CAPACITY]=bytes[i];
    tx_count+=count;outb(COM1+1,inb(COM1+1)|2);transmit();return true;
}
void serial_init(void)
{
    pending_status=0;
    outb(COM1 + 1, 0);
    outb(COM1 + 3, 0x80);
    outb(COM1, 1);
    outb(COM1 + 1, 0);
    outb(COM1 + 3, 3);
    outb(COM1 + 2, 0xc7);
    /* Only a working 16550 FIFO accepts more than one byte per THRE. */
    tx_burst = (inb(COM1 + 2) & 0xc0) == 0xc0 ? 16 : 1;
    outb(COM1 + 4, 0x0b);
}
void serial_putc(uint8_t byte)
{
    while(tx_count)transmit();
    while (!(line_status() & 0x20)) __asm__ volatile ("pause");
    outb(COM1, byte);
}
void serial_puts(const char *s) { while (*s) serial_putc((uint8_t)*s++); }
void serial_format(unsigned rate,uint8_t line_control)
{
    while(tx_count)transmit();
    /* Caller has disabled IRQs; keep IER masked while DLAB aliases it. */
    while (!(line_status() & 0x40)) __asm__ volatile ("pause");
    uint8_t ier = inb(COM1 + 1);
    outb(COM1 + 1, 0);
    /* Rate 134 is 134.5 baud: 115200/134.5 rounds to 857. */
    uint16_t divisor = rate == 134 ? 857 : 115200 / rate;
    outb(COM1 + 3, 0x80);
    outb(COM1, divisor & 255); outb(COM1 + 1, divisor >> 8);
    outb(COM1 + 3, line_control);
    outb(COM1 + 1, ier);
}
void serial_configure(unsigned rate,unsigned parity)
{serial_format(rate,parity?(parity==1?0x0a:0x1a):3);}
bool serial_idle(void){return !tx_count && (line_status()&0x40);}
void serial_flush(void)
{tx_head=tx_count=0;outb(COM1+1,inb(COM1+1)&~2U);}
void serial_dtr(bool on)
{outb(COM1+4,on?(inb(COM1+4)|1):(inb(COM1+4)&~1U));}
unsigned serial_modem_status(void)
{unsigned s=inb(COM1+6);return (s&128?NX_MDM_DCD:0)|(s&16?NX_MDM_CTS:0)|(s&32?NX_MDM_DSR:0)|(s&64?NX_MDM_RI:0);}
void serial_modem_outputs(bool dtr,bool rts)
{outb(COM1+4,(inb(COM1+4)&~3U)|(dtr?1:0)|(rts?2:0));}
void serial_receive_enable(void)
{
    outb(COM1 + 2, 0x07); /* FIFO, one-byte receive threshold. */
    outb(COM1 + 4, 0x0b); /* OUT2 routes the UART interrupt to IRQ4. */
    outb(COM1 + 1, 0x0d); /* Receive, line and modem status; no TX IRQ. */
    outb(0x21, inb(0x21) & ~(1U << 4));
}
void serial_interrupt(void)
{
    /* Bounded IRQ work. Remaining FIFO data retriggers RX or timeout IRQ. */
    for (unsigned i = 0; i < 256 && (!(inb(COM1 + 2) & 1) || (pending_status&16)); ++i) {
        transmit();
        uint8_t status = line_status()|pending_status;
        pending_status=0;
        if (status & 1) {
            uint8_t byte = inb(COM1);
            unsigned errors=(status&4?NX_RX_PARITY:0)|(status&8?NX_RX_FRAME:0)|
                (status&16?NX_RX_BREAK:0)|(status&2?NX_RX_OVERRUN:0);
            tty_receive_status(byte,errors);
        } else {
            if(status&16)tty_receive_status(0,NX_RX_BREAK|NX_RX_FRAME);
            tty_modem_status(serial_modem_status());
        }
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
