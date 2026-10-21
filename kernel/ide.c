#include "ide.h"
#include "io.h"
#include <nexix/errno.h>
struct ata {uint16_t io,control;unsigned slave;};
static uint16_t master_serial[2][10];
static bool master_known[2];
static const struct ata ports[]={{0x1f0,0x3f6,0},{0x1f0,0x3f6,1},{0x170,0x376,0},{0x170,0x376,1}};
#define BUSY 0x80
#define DRDY 0x40
#define DRQ 0x08
#define FAULT 0x21
#define POLLS 1000000U
static void delay(const struct ata *a)
{
    for (unsigned i=0;i<4;++i) (void)inb(a->control);
}
static int ready(const struct ata *a,bool data, bool check_errors)
{
    for (unsigned i=0;i<POLLS;++i) {
        uint8_t status=inb(a->control);
        if (!status || status==0xff) return -NX_EIO;
        if (status&BUSY) continue;
        if (check_errors && (status&FAULT)) return -NX_EIO;
        if ((status&DRDY) && !!(status&DRQ)==data) return 0;
    }
    return -NX_EIO;
}
/* ATA requires DRDY before non-diagnostic commands; a drive without it is unusable. */
static int drive_ready(const struct ata *a)
{
    for (unsigned i=0;i<POLLS;++i) {
        uint8_t status=inb(a->control);
        if (!status || status==0xff) return -NX_ENODEV;
        if (!(status&BUSY) && (status&DRDY)) return 0;
    }
    return -NX_EIO;
}
static int transfer(struct block_device *device,uint32_t sector,void *buffer,bool write)
{
    const struct ata *a=device->context;
    if(sector>=device->blocks || !buffer)return -NX_EIO;
    outb(a->io+6,0xe0 | (a->slave<<4) | ((sector>>24)&15));
    delay(a);
    /* A previous command error is cleared by issuing the next command. */
    if (ready(a,false,false) || drive_ready(a)) return -NX_EIO;
    outb(a->io+2,1);
    outb(a->io+3,sector);
    outb(a->io+4,sector>>8);
    outb(a->io+5,sector>>16);
    outb(a->io+7,write ? 0x30 : 0x20);
    delay(a);
    if (ready(a,true,true)) return -NX_EIO;
    uint8_t *bytes=buffer;
    for (unsigned i=0;i<256;++i) {
        if (write) outw(a->io,(uint16_t)bytes[2*i] | ((uint16_t)bytes[2*i+1]<<8));
        else {
            uint16_t word=inw(a->io);
            bytes[2*i]=word;bytes[2*i+1]=word>>8;
        }
    }
    delay(a);
    if (ready(a,false,true)) return -NX_EIO;
    if (write) {
        /* Flush the device cache before acknowledging each filesystem write. */
        outb(a->io+7,0xe7);
        delay(a);
        if (ready(a,false,true)) return -NX_EIO;
    }
    return 0;
}
static int command(struct block_device *device,uint32_t sector,void *buffer,bool write)
{
    int error=transfer(device,sector,buffer,write);
    if(error){
        const struct ata *a=device->context;
        outb(a->control,6);
        for(unsigned i=0;i<16;++i)delay(a); /* Hold reset beyond the select delay. */
        outb(a->control,2);delay(a);
        (void)ready(a,false,false); /* Bounded channel reset before a retry. */
    }
    return error;
}
static int read_sector(struct block_device *device,uint32_t sector,void *buffer)
{ return command(device,sector,buffer,false); }
static int write_sector(struct block_device *device,uint32_t sector,const void *buffer)
{ return command(device,sector,(void *)buffer,true); }
int ide_init_unit(struct block_device *device,unsigned index)
{
    if (!device || index>=4) return -NX_EINVAL;
    const struct ata *a=&ports[index];
    if(!a->slave)master_known[index/2]=false;
    *device=(struct block_device){0};
    outb(a->control,2); /* nIEN: disable device interrupts for polling. */
    outb(a->io+6,0xa0 | (a->slave<<4));
    delay(a);
    /* An absent slave may mirror the master's status. Verify that the
     * device/head register actually reports the selected device too. */
    if((inb(a->io+6)&0x10)!=(a->slave<<4))return -NX_ENODEV;
    uint8_t status=inb(a->io+7);
    if (!status || status==0xff) return -NX_ENODEV;
    int error=drive_ready(a);if (error) return error;
    outb(a->io+2,0);outb(a->io+3,0);outb(a->io+4,0);outb(a->io+5,0);
    outb(a->io+7,0xec);
    delay(a);
    if (ready(a,true,true)) return -NX_EIO;
    if (inb(a->io+4) || inb(a->io+5)) return -NX_EIO;
    uint16_t words[256];
    for (unsigned i=0;i<256;++i) words[i]=inw(a->io);
    delay(a);
    if (ready(a,false,true)) return -NX_EIO;
    uint32_t blocks=(uint32_t)words[60] | ((uint32_t)words[61]<<16);
    if (!(words[49]&(1U<<9)) || !blocks || blocks>(1U<<28) ||
        (words[83]&0xc000)!=0x4000 || !(words[83]&(1U<<12))) return -NX_EIO;
    if ((words[106]&0xc000)==0x4000 && (words[106]&(1U<<12))) {
        uint32_t size=(uint32_t)words[117] | ((uint32_t)words[118]<<16);
        if (size!=256) return -NX_EIO;
    }
    /* Some absent slaves also echo device/head writes. IDENTIFY must not
     * present the known master's nonblank hardware serial as a second disk. */
    if(a->slave && master_known[index/2]){
        bool same=true,identified=false;
        for(unsigned i=0;i<10;++i){
            uint16_t serial=words[10+i];
            if(serial!=master_serial[index/2][i])same=false;
            if(serial && serial!=0x2020 && serial!=0xffff)identified=true;
        }
        if(same && identified)return -NX_ENODEV;
    }
    if(!a->slave){
        for(unsigned i=0;i<10;++i)master_serial[index/2][i]=words[10+i];
        master_known[index/2]=true;
    }
    *device=(struct block_device){.blocks=blocks,.context=(void *)a,.read=read_sector,.write=write_sector};
    return 0;
}

int ide_init(struct block_device *device){return ide_init_unit(device,0);}
