#include "ide.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t regs[2][8],status[4],media[4][2][512];
static unsigned word[4],sector[4],resets;
static bool identify[4],absent[4],fail,mirror_slave,alias_identify;
static unsigned channel(uint16_t port)
{assert((port>=0x1f0 && port<=0x1f7)||port==0x3f6||(port>=0x170 && port<=0x177)||port==0x376);return port==0x376 || port<0x1f0?1:0;}
static unsigned drive(unsigned c){return c*2+((regs[c][6]>>4)&1);}
uint8_t inb(uint16_t port)
{
    unsigned c=channel(port),u=drive(c);
    if(port==0x3f6 || port==0x376 || (port&7)==7)return absent[u]?0:status[u];
    if(mirror_slave && (u&1) && (port&7)==6)return regs[c][6]&~0x10;
    return regs[c][port&7];
}
void outb(uint16_t port,uint8_t value)
{
    unsigned c=channel(port);
    if(port==0x3f6 || port==0x376){assert(value==2 || value==6);if(value==6){++resets;status[c*2]=status[c*2+1]=0x40;}return;}
    unsigned r=port&7;regs[c][r]=value;if(r!=7)return;
    unsigned u=drive(c);word[u]=0;identify[u]=value==0xec;
    assert(value==0xec || value==0x20 || value==0x30 || value==0xe7);
    if(value==0xe7){status[u]=0x40;return;}
    sector[u]=regs[c][3];assert(identify[u] || sector[u]<2);
    status[u]=0x48;if(fail && value==0x20){status[u]=0x41;fail=false;}
}
uint16_t inw(uint16_t port)
{
    unsigned u=drive(channel(port)),i=word[u]++;assert((port&7)==0 && i<256 && status[u]==0x48);
    uint16_t result=identify[u]?(i==10?(uint16_t)(alias_identify?(u&~1U)+1:u+1):i==49?512:i==60?2:i==83?0x5000:0):
        media[u][sector[u]][i*2]|((uint16_t)media[u][sector[u]][i*2+1]<<8);
    if(word[u]==256)status[u]=0x40;
    return result;
}
void outw(uint16_t port,uint16_t value)
{
    unsigned u=drive(channel(port)),i=word[u]++;assert((port&7)==0 && i<256 && status[u]==0x48);
    media[u][sector[u]][i*2]=value;media[u][sector[u]][i*2+1]=value>>8;
    if(word[u]==256)status[u]=0x40;
}
int main(void)
{
    struct block_device d[4],missing;uint8_t bytes[512],out[512];
    for(unsigned i=0;i<4;++i)status[i]=0x40;
    for(unsigned i=0;i<4;++i){assert(!ide_init_unit(&d[i],i) && d[i].blocks==2);memset(bytes,0x40+i,512);assert(!block_write(&d[i],1,1,bytes));}
    for(unsigned pass=0;pass<3;++pass)for(unsigned i=0;i<4;++i){
        memset(bytes,0x40+i,512);assert(!block_read(&d[i],1,1,out) && !memcmp(out,bytes,512));
    }
    fail=true;assert(block_read(&d[3],1,1,out)==-NX_EIO && resets==1);
    assert(!block_read(&d[3],1,1,out) && out[0]==0x43);
    mirror_slave=true;assert(ide_init_unit(&missing,1)==-NX_ENODEV && !missing.blocks);
    mirror_slave=false;alias_identify=true;
    assert(ide_init_unit(&missing,1)==-NX_ENODEV && !missing.blocks);
    alias_identify=false;assert(!ide_init_unit(&missing,1));
    absent[2]=true;assert(ide_init_unit(&missing,2)==-NX_ENODEV && !missing.blocks);
    assert(ide_init_unit(&missing,4)==-NX_EINVAL);
    puts("PASS: ATA primary/secondary master/slave selection, independent media, reset/error recovery and absent unit");
}
