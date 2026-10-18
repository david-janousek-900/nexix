/* Production adapter with in-memory regular-inode backing and user mappings. */
#include "tape.h"
#include "files.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct proc process;static struct user user;
static struct inode image;static unsigned pins;static unsigned char disk[8192];
static int failure;
struct proc *proc_current(void){return &process;}
bool proc_superuser(struct user *u){return !u->u_uid;}
int files_tape_backing(uintptr_t path,bool write,struct inode **out)
{(void)write;if(!path)return -NX_EFAULT;if(tape_image_busy(&image))return -NX_EBUSY;++pins;*out=&image;return 0;}
int files_inode_put(struct inode *inode){assert(inode==&image && pins);--pins;return 0;}
int files_tape_read(struct inode *inode,uint64_t at,void *b,size_t n)
{assert(inode==&image);if(at+n>(uint64_t)image.i_size)return -NX_EIO;memcpy(b,disk+at,n);return 0;}
int files_tape_write(struct inode *inode,uint64_t at,const void *b,size_t n)
{assert(inode==&image);if(failure)return failure;if(at+n>sizeof(disk))return -NX_ENOSPC;memcpy(disk+at,b,n);if(at+n>(uint64_t)image.i_size)image.i_size=at+n;return 0;}
int copyin(const struct address_space *vm,uintptr_t at,void *b,size_t n)
{(void)vm;if(at<4096)return -NX_EFAULT;memcpy(b,(void *)at,n);return 0;}
int copyout(const struct address_space *vm,uintptr_t at,const void *b,size_t n)
{(void)vm;if(at<4096)return -NX_EFAULT;memcpy((void *)at,b,n);return 0;}
static int control(void *c,unsigned command,struct nx_tape_load *a)
{return tape_ioctl(c,&process.p_vm,command,(uintptr_t)a,NULL);}
static int op(void *h,unsigned command,unsigned count)
{struct nx_tape_operation a={command,count};return tape_ioctl(h,&process.p_vm,NX_TAPE_OPERATION,(uintptr_t)&a,NULL);}
int main(void)
{
    process.p_user=&user;void *c,*h,*alias;unsigned char b[1024] __attribute__((aligned(2)))={1,2,3,4};
    assert(!tape_open(NX_TAPE_CONTROL_DEV,true,2,&c));
    struct nx_tape_load a={.family=NX_TAPE_HT,.flags=NX_TAPE_INITIALIZE,.path=4096,.capacity=8192};
    user.u_uid=1;assert(control(c,NX_TAPE_LOAD,&a)==-NX_EPERM && !pins);user.u_uid=0;
    a.unit=1;assert(control(c,NX_TAPE_LOAD,&a)==-NX_EINVAL && !pins);a.unit=0;
    a.reserved=1;assert(control(c,NX_TAPE_LOAD,&a)==-NX_EINVAL);a.reserved=0;
    assert(control(c,NX_TAPE_LOAD,&a)==0 && pins==1 && tape_image_busy(&image));
    assert(control(c,NX_TAPE_LOAD,&a)==-NX_EBUSY && pins==1);
    assert(!tape_open(NX_TAPE_DEV(0,NX_TAPE_NOREWIND,1),true,2,&h));
    assert(tape_open(NX_TAPE_DEV(0,0,0),false,0,&alias)==-NX_ENXIO);
    assert(control(c,NX_TAPE_UNLOAD,&a)==-NX_EBUSY);
    assert(tape_io(h,&process.p_vm,(uintptr_t)b+1,2,true,0)==-NX_EFAULT);
    assert(tape_io(h,&process.p_vm,(uintptr_t)b,4,true,0)==4);
    assert(op(h,NX_TAPE_WRITE_MARK,1)==0 && tape_io(h,&process.p_vm,(uintptr_t)b,4,true,0)==4);
    assert(!op(h,NX_TAPE_REWIND,1) && !op(h,NX_TAPE_FORWARD_FILE,1));
    assert(tape_io(h,&process.p_vm,(uintptr_t)b,2,false,123)==-NX_EIO);
    assert(!op(h,NX_TAPE_BACK_FILE,1));assert(tape_io(h,&process.p_vm,(uintptr_t)b,512,false,0)==0);
    assert(tape_io(h,&process.p_vm,(uintptr_t)b,512,false,0)==4);
    assert(!tape_close(h));assert(!tape_open(NX_TAPE_DEV(0,0,1),true,0,&h));
    struct nx_tape_status s;assert(!tape_ioctl(h,&process.p_vm,NX_TAPE_STATUS,(uintptr_t)&s,NULL));
    assert(s.record==4 && s.records==5 && s.position<s.end);
    assert(op(h,NX_TAPE_WRITE_MARK,1)==-NX_EBADF && !tape_close(h));
    a=(struct nx_tape_load){.family=NX_TAPE_HT};assert(!control(c,NX_TAPE_UNLOAD,&a) && !pins);
    a.flags=NX_TAPE_READONLY;a.path=4096;assert(!control(c,NX_TAPE_LOAD,&a));
    assert(tape_open(NX_TAPE_DEV(0,0,1),true,1,&h)==-NX_EROFS);
    assert(!tape_open(NX_TAPE_DEV(0,0,1),true,0,&h));
    assert(tape_io(h,&process.p_vm,(uintptr_t)b,512,false,0)==4 && !tape_close(h));
    a=(struct nx_tape_load){.family=NX_TAPE_HT};assert(!control(c,NX_TAPE_UNLOAD,&a));
    a.flags=NX_TAPE_INITIALIZE;a.path=4096;a.capacity=8192;
    assert(control(c,NX_TAPE_LOAD,&a)==-NX_EEXIST && !pins); /* failed load releases inode */
    a.flags=0;a.capacity=0;assert(!control(c,NX_TAPE_LOAD,&a));
    assert(!tape_open(NX_TAPE_DEV(0,0,1),true,1,&h));failure=-NX_ENOSPC;
    assert(op(h,NX_TAPE_WRITE_MARK,2)==-NX_ENOSPC);
    assert(!tape_ioctl(h,&process.p_vm,NX_TAPE_STATUS,(uintptr_t)&s,NULL) && s.residual==2);
    assert(tape_close(h)==-NX_EIO);failure=0;
    assert(!tape_open(NX_TAPE_DEV(0,0,1),true,0,&h) && !tape_close(h));
    a=(struct nx_tape_load){.family=NX_TAPE_HT};assert(!control(c,NX_TAPE_UNLOAD,&a) && !pins);
    image.i_size=0;a=(struct nx_tape_load){.family=NX_TAPE_TM,.flags=NX_TAPE_INITIALIZE,.path=4096,.capacity=8192};
    assert(!control(c,NX_TAPE_LOAD,&a) && !tape_open(NX_TAPE_DEV(1,0,0),false,2,&h));
    for(unsigned i=0;i<sizeof(b);++i)b[i]=(unsigned char)i;
    assert(tape_io(h,&process.p_vm,(uintptr_t)b,1024,true,0)==1024);
    assert(tape_io(h,&process.p_vm,(uintptr_t)b,512,true,1024)==512);
    memset(b,0,sizeof(b));assert(tape_io(h,&process.p_vm,(uintptr_t)b,4,false,510)==4);
    assert(b[0]==254 && b[1]==255 && b[2]==0 && b[3]==1);
    assert(tape_io(h,&process.p_vm,(uintptr_t)b,1024,false,0)==1024);
    for(unsigned i=0;i<sizeof(b);++i)assert(b[i]==(unsigned char)i);
    assert(!op(h,NX_TAPE_REWIND,1) && !op(h,NX_TAPE_ERASE,1));
    assert(!tape_io(h,&process.p_vm,(uintptr_t)b,512,false,0) && !tape_close(h));
    a=(struct nx_tape_load){.family=NX_TAPE_TM};assert(!control(c,NX_TAPE_UNLOAD,&a) && !pins);
    assert(!tape_close(c));puts("PASS: tape adapter privileges, aliases, pins, marks/spacing, read-only and failure cleanup");
}
