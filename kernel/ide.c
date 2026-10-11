#include "ide.h"
#include "io.h"
#include <nexix/errno.h>
#define DATA 0x1f0
#define STATUS 0x1f7
#define CONTROL 0x3f6
#define BUSY 0x80
#define DRQ 0x08
#define FAULT 0x21
#define POLLS 1000000U
static void delay(void)
{
    for (unsigned i=0;i<4;++i) (void)inb(CONTROL);
}
static int ready(bool data, bool check_errors)
{
    for (unsigned i=0;i<POLLS;++i) {
        uint8_t status=inb(CONTROL);
        if (!status || status==0xff) return -NX_EIO;
        if (status&BUSY) continue;
        if (check_errors && (status&FAULT)) return -NX_EIO;
        if (!!(status&DRQ)==data) return 0;
    }
    return -NX_EIO;
}
static int transfer(struct block_device *device,uint32_t sector,void *buffer,bool write)
{
    (void)device;
    outb(0x1f6,0xe0 | ((sector>>24)&15));
    delay();
    /* A previous command error is cleared by issuing the next command. */
    if (ready(false,false)) return -NX_EIO;
    outb(0x1f2,1);
    outb(0x1f3,sector);
    outb(0x1f4,sector>>8);
    outb(0x1f5,sector>>16);
    outb(STATUS,write ? 0x30 : 0x20);
    delay();
    if (ready(true,true)) return -NX_EIO;
    uint8_t *bytes=buffer;
    for (unsigned i=0;i<256;++i) {
        if (write) outw(DATA,(uint16_t)bytes[2*i] | ((uint16_t)bytes[2*i+1]<<8));
        else {
            uint16_t word=inw(DATA);
            bytes[2*i]=word;bytes[2*i+1]=word>>8;
        }
    }
    delay();
    if (ready(false,true)) return -NX_EIO;
    if (write) {
        /* Flush the device cache before acknowledging each filesystem write. */
        outb(STATUS,0xe7);
        delay();
        if (ready(false,true)) return -NX_EIO;
    }
    return 0;
}
static int read_sector(struct block_device *device,uint32_t sector,void *buffer)
{ return transfer(device,sector,buffer,false); }
static int write_sector(struct block_device *device,uint32_t sector,const void *buffer)
{ return transfer(device,sector,(void *)buffer,true); }
int ide_init(struct block_device *device)
{
    if (!device) return -NX_EINVAL;
    *device=(struct block_device){0};
    outb(CONTROL,2); /* nIEN: disable device interrupts for polling. */
    outb(0x1f6,0xa0);
    delay();
    uint8_t status=inb(STATUS);
    if (!status || status==0xff) return -NX_ENODEV;
    outb(0x1f2,0);outb(0x1f3,0);outb(0x1f4,0);outb(0x1f5,0);
    outb(STATUS,0xec);
    delay();
    if (ready(true,true)) return -NX_EIO;
    if (inb(0x1f4) || inb(0x1f5)) return -NX_EIO;
    uint16_t words[256];
    for (unsigned i=0;i<256;++i) words[i]=inw(DATA);
    delay();
    if (ready(false,true)) return -NX_EIO;
    uint32_t blocks=(uint32_t)words[60] | ((uint32_t)words[61]<<16);
    if (!(words[49]&(1U<<9)) || !blocks || blocks>(1U<<28) ||
        (words[83]&0xc000)!=0x4000 || !(words[83]&(1U<<12))) return -NX_EIO;
    if ((words[106]&0xc000)==0x4000 && (words[106]&(1U<<12))) {
        uint32_t size=(uint32_t)words[117] | ((uint32_t)words[118]<<16);
        if (size!=256) return -NX_EIO;
    }
    *device=(struct block_device){.blocks=blocks,.read=read_sector,.write=write_sector};
    return 0;
}
