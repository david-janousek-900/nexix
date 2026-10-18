/* Native regular-file media for V7 ht/tm; serialized with the file layer. */
#include "tape.h"
#include "files.h"
#include "accounting.h"
#include "uaccess.h"
#include <nexix/errno.h>
struct tape_unit {
    struct inode *inode;
    struct tape tape;
    uint64_t origin,origin_record;
    unsigned family,unit,density;
    bool readonly,opened,raw,norewind,canwrite,writeonly,written,buffer_cursor;
};
static struct tape_unit units[9];
static char control;
static struct tape_unit *unit(unsigned family,unsigned number)
{return family==NX_TAPE_HT?(number?NULL:&units[0]):family==NX_TAPE_TM && number<8?&units[number+1]:NULL;}
static int read_media(void *p,uint64_t at,void *bytes,size_t n)
{return files_tape_read(((struct tape_unit *)p)->inode,at,bytes,n);}
static int write_media(void *p,uint64_t at,const void *bytes,size_t n)
{return files_tape_write(((struct tape_unit *)p)->inode,at,bytes,n);}
bool tape_image_busy(const struct inode *inode)
{if(!inode)return false;for(unsigned i=0;i<9;++i)if(units[i].inode==inode)return true;return false;}
bool tape_matches(uint16_t dev,bool raw)
{return (raw && dev==NX_TAPE_CONTROL_DEV) || (dev>>8)==(raw?19U:17U) || (dev>>8)==(raw?20U:18U);}
static void origin(struct tape_unit *u){u->origin=u->tape.position;u->origin_record=u->tape.record;u->buffer_cursor=true;}
static int load(struct address_space *vm,uintptr_t address,bool unload)
{
    if(!proc_superuser(proc_current()->p_user))return -NX_EPERM;
    struct nx_tape_load a;int error=copyin(vm,address,&a,sizeof(a));if(error)return error;
    struct tape_unit *u=unit(a.family,a.unit);
    if(!u || a.reserved || a.flags&~(NX_TAPE_READONLY|NX_TAPE_INITIALIZE))return -NX_EINVAL;
    if(u->opened)return -NX_EBUSY;
    if(unload){
        if(a.flags || a.path || a.capacity)return -NX_EINVAL;
        if(!u->inode)return -NX_ENXIO;
        struct inode *inode=u->inode;*u=(struct tape_unit){0};return files_inode_put(inode);
    }
    if(u->inode)return -NX_EBUSY;
    if((a.flags&NX_TAPE_INITIALIZE) && (a.flags&NX_TAPE_READONLY))return -NX_EINVAL;
    if(!(a.flags&NX_TAPE_INITIALIZE) && a.capacity)return -NX_EINVAL;
    struct inode *inode;error=files_tape_backing(a.path,!(a.flags&NX_TAPE_READONLY),&inode);if(error)return error;
    *u=(struct tape_unit){.inode=inode,.family=a.family,.unit=a.unit,.readonly=!!(a.flags&NX_TAPE_READONLY)};
    struct tape_media media={u,inode->i_size,read_media,u->readonly?NULL:write_media};
    error=tape_image(&u->tape,media,a.family==NX_TAPE_HT?10:2,!!(a.flags&NX_TAPE_INITIALIZE),a.capacity);
    if(error){*u=(struct tape_unit){0};(void)files_inode_put(inode);return error;}
    origin(u);return 0;
}
int tape_open(uint16_t dev,bool raw,unsigned access,void **handle)
{
    if(raw && dev==NX_TAPE_CONTROL_DEV){*handle=&control;return 0;}
    unsigned family=(dev>>8)-(raw?19:17),minor=dev&255;
    unsigned number=minor&((family==NX_TAPE_HT)?077:0177);
    struct tape_unit *u=unit(family,number);
    if(!u || !u->inode || u->opened)return -NX_ENXIO;
    if(access && u->readonly)return -NX_EROFS;
    /* Open resets error state, but does not physically rewind the transport. */
    uint64_t position=u->tape.position,record=u->tape.record;
    struct tape_media media={u,u->inode->i_size,read_media,u->readonly?NULL:write_media};
    int error=tape_image(&u->tape,media,family==NX_TAPE_HT?10:2,false,0);if(error)return error;
    if(position>=64 && position<=u->tape.end && record<=u->tape.records){u->tape.position=position;u->tape.record=record;}
    u->opened=true;u->raw=raw;u->canwrite=access!=0;u->writeonly=access==1;u->written=false;
    u->norewind=!!(minor&NX_TAPE_NOREWIND);u->density=family==NX_TAPE_HT && (minor&NX_TAPE_800)?800:1600;
    if(family==NX_TAPE_TM)u->density=800;
    origin(u);*handle=u;return 0;
}
int tape_close(void *handle)
{
    if(handle==&control)return 0;
    struct tape_unit *u=handle;int error=0;
    if(u->writeonly || (u->canwrite && u->written)){
        error=tape_mark(&u->tape);if(!error)error=tape_mark(&u->tape);if(!error)error=tape_move(&u->tape,true);
    }
    if(!u->norewind){u->tape.position=64;u->tape.record=0;}
    u->opened=false;return error;
}
bool tape_seekable(void *handle){return handle!=&control;}
struct user_copy {struct address_space *vm;uintptr_t address;};
static int input(void *p,size_t off,void *bytes,size_t n)
{struct user_copy *c=p;return copyin(c->vm,c->address+off,bytes,n);}
static int output(void *p,size_t off,void *bytes,size_t n)
{struct user_copy *c=p;return copyout(c->vm,c->address+off,bytes,n);}
static int buffer_in(void *p,size_t off,void *bytes,size_t n)
{uint8_t *b=bytes,*a=(uint8_t *)p+off;while(n--)*b++=*a++;return 0;}
static int buffer_out(void *p,size_t off,void *bytes,size_t n)
{uint8_t *b=(uint8_t *)p+off,*a=bytes;while(n--)*b++=*a++;return 0;}
static int position_block(struct tape_unit *u,uint64_t block)
{
    if(block>NX_TAPE_MAX_RECORDS)return -NX_ENXIO;
    uint64_t target=u->origin_record+block;
    if(!u->buffer_cursor){u->tape.position=u->origin;u->tape.record=u->origin_record;u->buffer_cursor=true;}
    while(u->tape.record>target){int error=tape_move(&u->tape,true);if(error)return error;}
    while(u->tape.record<target){
        uint32_t length;int error=tape_length(&u->tape,&length);if(error)return error;
        if(!length)return -NX_ENXIO;
        error=tape_move(&u->tape,false);if(error)return error;
    }
    return 0;
}
int64_t tape_io(void *handle,struct address_space *vm,uintptr_t address,size_t count,bool write,int64_t offset)
{
    if(handle==&control)return -NX_ENXIO;
    struct tape_unit *u=handle;
    if(u->raw && count && ((address|count)&1))return -NX_EFAULT;
    if(!count)return 0;
    if(u->raw){
        struct user_copy copy={vm,address};
        int64_t n=write?tape_write_record(&u->tape,count,input,&copy):tape_read_record(&u->tape,count,output,&copy);
        if(write && n>0)u->written=true;
        return n;
    }
    size_t done=0;int error=0;
    while(done<count){
        uint64_t at=(uint64_t)offset+done;unsigned within=at%512;size_t n=512-within;if(n>count-done)n=count-done;
        error=position_block(u,at/512);if(error)break;
        uint64_t position=u->tape.position,record=u->tape.record;
        uint8_t bytes[512]={0};int64_t got=tape_read_record(&u->tape,512,buffer_out,bytes);
        if(got<0){error=got;break;}
        u->buffer_cursor=got==512;
        if(got && got!=512){error=-NX_EIO;break;}
        if(!write){
            if(!got)break;
            error=copyout(vm,address+done,bytes+within,n);if(error)break;
        }else{
            u->tape.position=position;u->tape.record=record;
            error=copyin(vm,address+done,bytes+within,n);if(error)break;
            got=tape_write_record(&u->tape,512,buffer_in,bytes);if(got<0){error=got;break;}
            u->written=true;u->buffer_cursor=true;
        }
        done+=n;
    }
    return done?(int64_t)done:error;
}
static int operation(struct tape_unit *u,struct nx_tape_operation a,int64_t *offset)
{
    if(!a.count || a.count>65536 || a.operation<NX_TAPE_REWIND || a.operation>NX_TAPE_ERASE)return -NX_EINVAL;
    if((a.operation==NX_TAPE_REWIND || a.operation==NX_TAPE_ERASE) && a.count!=1)return -NX_EINVAL;
    if((a.operation==NX_TAPE_WRITE_MARK || a.operation==NX_TAPE_ERASE) && !u->canwrite)return -NX_EBADF;
    int error=0;unsigned steps=0;u->tape.residual=a.count;
    for(unsigned i=0;i<a.count && !error;++i){
        switch(a.operation){
        case NX_TAPE_REWIND:error=tape_rewind(&u->tape);break;
        case NX_TAPE_FORWARD_RECORD:case NX_TAPE_BACK_RECORD:
            error=tape_move(&u->tape,a.operation==NX_TAPE_BACK_RECORD);break;
        case NX_TAPE_FORWARD_FILE:case NX_TAPE_BACK_FILE:
            for(;;){
                uint32_t length;bool backward=a.operation==NX_TAPE_BACK_FILE;
                if(++steps>65536){error=-NX_EINVAL;break;}
                if(backward){error=tape_move(&u->tape,true);if(error)break;}
                error=tape_length(&u->tape,&length);if(error)break;
                if(!backward){error=tape_move(&u->tape,false);if(error)break;}
                if(!length)break;
            }break;
        case NX_TAPE_WRITE_MARK:error=tape_mark(&u->tape);break;
        case NX_TAPE_ERASE:error=tape_erase(&u->tape);break;
        }
        u->tape.residual=a.count-i-(error?0:1);
    }
    origin(u);if(offset)*offset=0;return error;
}
int tape_ioctl(void *handle,struct address_space *vm,uint64_t request,uintptr_t address,int64_t *offset)
{
    if(handle==&control){
        if(request==NX_TAPE_LOAD || request==NX_TAPE_UNLOAD)return load(vm,address,request==NX_TAPE_UNLOAD);
        return -NX_ENOTTY;
    }
    struct tape_unit *u=handle;
    if(request==NX_TAPE_STATUS){
        struct nx_tape_status s={.family=u->family,.unit=u->unit,.flags=NX_TAPE_ONLINE|NX_TAPE_OPEN|
            (u->readonly?0:NX_TAPE_WRITABLE)|(u->tape.poisoned?NX_TAPE_ERROR:0),.density=u->density,
            .record=u->tape.record,.position=u->tape.position,.records=u->tape.records,.end=u->tape.end,
            .capacity=u->tape.capacity,.residual=u->tape.residual};
        return copyout(vm,address,&s,sizeof(s));
    }
    if(request==NX_TAPE_OPERATION){
        struct nx_tape_operation a;int error=copyin(vm,address,&a,sizeof(a));if(error)return error;
        return operation(u,a,offset);
    }
    return -NX_ENOTTY;
}
