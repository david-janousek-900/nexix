/* Fixed-size headerless TC media. No sequential-tape record framing or marks. */
#include "dectape.h"
#include "files.h"
#include <nexix/errno.h>
struct tc_media {struct inode *inode;struct block_device device;};
static struct tc_media media[NX_TC_UNITS];
bool tc_image_busy(const struct inode *inode)
{if(!inode)return false;for(unsigned i=0;i<NX_TC_UNITS;++i)if(media[i].inode==inode)return true;return false;}
static int read_block(struct block_device *d,uint32_t b,void *bytes)
{return files_tape_read(((struct tc_media *)d->context)->inode,(uint64_t)b*BLOCK_SIZE,bytes,BLOCK_SIZE);}
static int write_block(struct block_device *d,uint32_t b,const void *bytes)
{return files_tape_write(((struct tc_media *)d->context)->inode,(uint64_t)b*BLOCK_SIZE,bytes,BLOCK_SIZE);}
int tc_media_load(const struct nx_tape_load *a,bool unload)
{
    if(a->unit>=NX_TC_UNITS || a->reserved || a->capacity || a->flags&~(NX_TAPE_INITIALIZE|NX_TAPE_READONLY))return -NX_EINVAL;
    struct tc_media *m=&media[a->unit];
    if(unload){
        if(a->flags || a->path)return -NX_EINVAL;
        if(!m->inode)return -NX_ENXIO;
        int error=tc_bind(a->unit,NULL);if(error)return error;
        struct inode *inode=m->inode;*m=(struct tc_media){0};return files_inode_put(inode);
    }
    if(m->inode)return -NX_EBUSY;
    if((a->flags&(NX_TAPE_INITIALIZE|NX_TAPE_READONLY))==(NX_TAPE_INITIALIZE|NX_TAPE_READONLY))return -NX_EINVAL;
    struct inode *inode;int error=files_tape_backing(a->path,!(a->flags&NX_TAPE_READONLY),&inode);if(error)return error;
    if(a->flags&NX_TAPE_INITIALIZE){
        if(inode->i_size)error=-NX_EEXIST;
        /* Allocate/write real blocks so ENOSPC is reported while loading, not
         * hidden behind a sparse image that will fail much later. */
        uint8_t zero[BLOCK_SIZE]={0};
        for(unsigned b=0;!error && b<NX_TC_BLOCKS;++b)error=files_tape_write(inode,(uint64_t)b*BLOCK_SIZE,zero,BLOCK_SIZE);
    }else if(inode->i_size!=NX_TC_BYTES)error=-NX_EINVAL;
    if(!error){
        *m=(struct tc_media){.inode=inode,.device={.blocks=NX_TC_BLOCKS,.context=m,.read=read_block,
            .write=(a->flags&NX_TAPE_READONLY)?NULL:write_block,.read_only=!!(a->flags&NX_TAPE_READONLY)}};
        error=tc_bind(a->unit,&m->device);
    }
    if(error){*m=(struct tc_media){0};(void)files_inode_put(inode);}
    return error;
}
