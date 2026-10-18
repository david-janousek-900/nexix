#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ide.h"
#include <nexix/errno.h>
static uint8_t registers[8], status, disk[4][512];
static uint16_t identify[256];
static unsigned word, sector, commands, flushes, polls;
static bool identifying, stuck, fail_flush, fail_read;
uint8_t inb(uint16_t port)
{
    if (port==0x1f7 || port==0x3f6) { ++polls;return stuck ? 0x80 : status; }
    assert(port>=0x1f0 && port<=0x1f6);
    return registers[port-0x1f0];
}
void outb(uint16_t port,uint8_t value)
{
    if (port==0x3f6) { assert(value==2 || value==6);if(value==6)status=0x40;return; }
    assert(port>=0x1f0 && port<=0x1f7);
    registers[port-0x1f0]=value;
    if (port!=0x1f7) return;
    ++commands;word=0;identifying=value==0xec;
    if (value==0xe7) { ++flushes;status=fail_flush ? 0x41 : 0x40;return; }
    assert(value==0xec || value==0x20 || value==0x30);
    if (!identifying) {
        assert(registers[2]==1);
        sector=registers[3] | ((unsigned)registers[4]<<8) |
            ((unsigned)registers[5]<<16) | ((unsigned)(registers[6]&15)<<24);
        assert(sector<4);
    }
    status=(fail_read && value==0x20) ? 0x41 : 0x48;
}
uint16_t inw(uint16_t port)
{
    assert(port==0x1f0 && word<256 && status==0x48);
    uint16_t value=identifying ? identify[word] :
        disk[sector][2*word] | ((uint16_t)disk[sector][2*word+1]<<8);
    if (++word==256) status=0x40;
    return value;
}
void outw(uint16_t port,uint16_t value)
{
    assert(port==0x1f0 && word<256 && status==0x48 && registers[7]==0x30);
    disk[sector][2*word]=value;disk[sector][2*word+1]=value>>8;
    if (++word==256) status=0x40;
}
static void reset(void)
{
    memset(registers,0,sizeof(registers));memset(identify,0,sizeof(identify));
    status=0x40;word=commands=flushes=polls=0;
    identifying=stuck=fail_flush=fail_read=false;
    identify[49]=1<<9;identify[60]=4;identify[83]=0x5000;
}
int main(void)
{
    struct block_device device;
    reset();status=0;assert(ide_init(&device)==-NX_ENODEV && !device.blocks);
    reset();stuck=true;assert(ide_init(&device)==-NX_EIO && polls>=1000000 && polls<1000020);
    reset();identify[49]=0;assert(ide_init(&device)==-NX_EIO);
    reset();identify[106]=0x5000;identify[117]=2048;assert(ide_init(&device)==-NX_EIO);
    reset();identify[83]=0x4000;assert(ide_init(&device)==-NX_EIO);
    reset();assert(!ide_init(&device) && device.blocks==4);
    uint8_t input[1025],output[1025];
    for(unsigned i=0;i<sizeof(input);++i)input[i]=(i*37)^0xa5;
    assert(!block_write(&device,1,2,input+1) && flushes==2);
    memset(output,0,sizeof(output));
    assert(!block_read(&device,1,2,output+1));
    assert(!memcmp(input+1,output+1,1024));
    unsigned before=commands;
    assert(block_write(&device,3,2,input)==-NX_EIO && commands==before);
    fail_read=true;assert(block_read(&device,0,1,output)==-NX_EIO);
    fail_read=false;assert(!block_read(&device,0,1,output));
    reset();assert(!ide_init(&device));fail_flush=true;
    assert(block_write(&device,0,1,input)==-NX_EIO && flushes==1);
    puts("PASS: IDE absent/timeout/unsupported device, unaligned multi-sector I/O, bounds, read and flush errors");
}
