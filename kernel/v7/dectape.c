/* V7 dev/tc.c: eight random-access DECtapes, source-sized media and recovery. */
#include "dectape.h"
#include <nexix/errno.h>
struct tc_unit {struct block_device view,*backing;unsigned refs;bool failed;};
static struct tc_unit units[NX_TC_UNITS];
bool tc_major(unsigned major){return major==NX_TC_MAJOR;}
struct block_device *tc_backing(struct block_device *d)
{for(unsigned i=0;i<NX_TC_UNITS;++i)if(d==&units[i].view)return units[i].backing;return NULL;}
bool tc_bound(struct block_device *d)
{if(!d)return false;for(unsigned i=0;i<NX_TC_UNITS;++i)if(units[i].backing==d)return true;return false;}
static int transfer(struct block_device *d,uint32_t block,void *bytes,bool write)
{
    struct tc_unit *u=d->context;struct block_device *b=u->backing;
    if(!b || !u->refs)return -NX_ENODEV;
    if(block>=NX_TC_BLOCKS || u->failed)return -NX_EIO;
    if(write && (b->read_only || !b->write))return b->read_only?-NX_EROFS:-NX_ENODEV;
    uint8_t scratch[BLOCK_SIZE];int error=-NX_EIO;
    for(unsigned attempt=0;attempt<20;++attempt){
        error=write?b->write(b,block,bytes):b->read(b,block,scratch);
        if(!error){if(!write)for(unsigned i=0;i<BLOCK_SIZE;++i)((uint8_t *)bytes)[i]=scratch[i];return 0;}
        /* TC select/illegal-operation errors poison this unit until close.
         * Exhausted ordinary transfer retries do not set source tcper[]. */
        if(error==-NX_ENODEV || error==-NX_ENXIO)u->failed=true;
        if(error!=-NX_EIO)break;
    }
    return error;
}
static int read_one(struct block_device *d,uint32_t b,void *p){return transfer(d,b,p,false);}
static int write_one(struct block_device *d,uint32_t b,const void *p){return transfer(d,b,(void *)p,true);}
int tc_bind(unsigned unit,struct block_device *base)
{
    if(unit>=NX_TC_UNITS)return -NX_ENXIO;
    struct tc_unit *u=&units[unit];if(u->refs)return -NX_EBUSY;
    if(base && (base->blocks!=NX_TC_BLOCKS || !base->read || base->read==read_one))return -NX_EINVAL;
    *u=(struct tc_unit){.backing=base,.view={.blocks=NX_TC_BLOCKS,.read=read_one,.write=write_one}};
    u->view.context=u;
    if(base){u->view.read_only=base->read_only;if(!base->write)u->view.write=NULL;}
    return 0;
}
int tc_acquire(uint16_t dev,struct block_device **out)
{
    if(!tc_major(dev>>8) || (dev&255)>=NX_TC_UNITS)return -NX_ENXIO;
    struct tc_unit *u=&units[dev&255];if(!u->backing)return -NX_ENODEV;
    if(u->refs==UINT32_MAX)return -NX_ENFILE;
    ++u->refs;u->view.read_only=u->backing->read_only;*out=&u->view;return 0;
}
void tc_release(struct block_device *d)
{
    for(unsigned i=0;i<NX_TC_UNITS;++i)if(d==&units[i].view && units[i].refs){
        if(!--units[i].refs)units[i].failed=false;
        return;
    }
}
