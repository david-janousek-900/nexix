#include "core.h"
#include "files.h"
#include <nexix/errno.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
static struct user user;
static struct proc process;
static struct inode file;
static unsigned char pages[4][PAGE_SIZE],output[65536];
static size_t length;
static int opened,closed,begin_error,write_error,write_calls,close_error,translate_error,many;
struct proc *proc_current(void){return &process;}
int64_t clock_now(void){return 1700000000;}
void fpu_save(struct nx_fpu_state *fp)
{memset(fp,0,sizeof(*fp));memset(fp->bytes+160,0x5a,16);}
bool vm_walk_user(const struct address_space *vm,bool (*visit)(uintptr_t,uintptr_t,unsigned,void *),void *arg)
{
    (void)vm;
    if(many){for(unsigned i=0;i<=NX_CORE_MAX_LOADS;++i)if(!visit(USER_BASE+i*2*PAGE_SIZE,(uintptr_t)pages[1],VM_USER|VM_WRITE,arg))return false;return true;}
    return visit(USER_BASE,(uintptr_t)pages[0],VM_USER|VM_EXEC,arg)&&
        visit(USER_BASE+PAGE_SIZE,(uintptr_t)pages[1],VM_USER|VM_WRITE,arg)&&
        visit(USER_BASE+2*PAGE_SIZE,(uintptr_t)pages[2],VM_USER|VM_WRITE,arg)&&
        visit(USER_LIMIT-2*PAGE_SIZE,(uintptr_t)pages[3],VM_USER|VM_WRITE,arg);
}
bool vm_translate(const struct address_space *vm,uintptr_t va,uintptr_t *pa,unsigned *flags)
{
    (void)vm;if(translate_error)return false;
    unsigned index=va==USER_LIMIT-2*PAGE_SIZE?3:(va-USER_BASE)/PAGE_SIZE;
    if(index>3)return false;
    *pa=(uintptr_t)pages[index];*flags=VM_USER|(index?VM_WRITE:VM_EXEC);return true;
}
int files_core_begin(struct inode **out)
{if(begin_error)return begin_error;++opened;*out=&file;memset(output,0,sizeof(output));length=0;return 0;}
int files_core_write(struct inode *inode,uint64_t offset,const void *bytes,size_t size)
{
    if(inode!=&file||offset>sizeof(output)||size>sizeof(output)-offset)return -NX_EIO;
    if(write_error&&++write_calls==write_error)return -NX_EIO;
    memcpy(output+offset,bytes,size);if(offset+size>length)length=offset+size;return 0;
}
int files_inode_put(struct inode *inode){if(inode!=&file)return -NX_EINVAL;++closed;return close_error;}
static struct interrupt_frame frame;
static void reset(void)
{
    user=(struct user){.u_uid=42,.u_ruid=42,.u_gid=7,.u_rgid=7};memcpy(user.u_comm,"TASK52",6);
    process=(struct proc){.p_user=&user,.p_pid=123,.p_ppid=45};
    frame=(struct interrupt_frame){.cs=0x23,.ss=0x1b,.rflags=0x202,.rip=USER_BASE+64,
        .rsp=USER_LIMIT-2*PAGE_SIZE+128,.r12=0x123456789abcdef0UL};
    opened=closed=begin_error=write_error=write_calls=close_error=translate_error=many=0;
    for(unsigned i=0;i<4;++i)memset(pages[i],0x30+i,PAGE_SIZE);
}
int main(void)
{
    for(unsigned n=1;n<17;++n)CHECK(core_signal(n)==(n==3||n==4||n==5||n==6||n==7||n==8||n==10||n==11||n==12));
    reset();CHECK(!core_dump(NX_SIGTERM,&frame)&&!opened);
    user.u_uid=0;CHECK(!core_dump(NX_SIGSEGV,&frame)&&!opened);user.u_uid=42;
    user.u_gid=8;CHECK(!core_dump(NX_SIGSEGV,&frame)&&!opened);user.u_gid=7;
    CHECK(!core_dump(NX_SIGSEGV,NULL)&&!opened);
    frame.cs=8;CHECK(!core_dump(NX_SIGSEGV,&frame)&&!opened);frame.cs=0x23;
    many=1;CHECK(!core_dump(NX_SIGSEGV,&frame)&&!opened);many=0;
    begin_error=-NX_EROFS;CHECK(!core_dump(NX_SIGSEGV,&frame)&&!opened&&!closed);begin_error=0;
    write_error=1;CHECK(!core_dump(NX_SIGSEGV,&frame)&&opened==1&&closed==1);
    reset();write_error=5;CHECK(!core_dump(NX_SIGSEGV,&frame)&&closed==1&&length>4096);
    reset();translate_error=1;CHECK(!core_dump(NX_SIGSEGV,&frame)&&opened==1&&closed==1);
    reset();close_error=-NX_EIO;CHECK(!core_dump(NX_SIGSEGV,&frame)&&closed==1);
    reset();CHECK(core_dump(NX_SIGSEGV,&frame)&&opened==1&&closed==1);
    struct nx_core_elf_header header;memcpy(&header,output,sizeof(header));
    CHECK(header.type==4&&header.machine==62&&header.phnum==3);
    struct nx_core_program ph[3];memcpy(ph,output+header.phoff,sizeof(ph));
    CHECK(ph[0].type==4&&ph[1].vaddr==USER_BASE+PAGE_SIZE&&ph[1].filesz==2*PAGE_SIZE&&ph[2].vaddr==USER_LIMIT-2*PAGE_SIZE);
    CHECK(output[ph[1].offset]==0x31&&output[ph[1].offset+PAGE_SIZE]==0x32&&output[ph[2].offset]==0x33);
    FILE *f=fopen("build/core-fixture.elf","wb");CHECK(f);
    CHECK(fwrite(output,1,length,f)==length&&fclose(f)==0);return 0;
}
