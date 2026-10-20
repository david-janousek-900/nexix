#include "kinfo.h"
#include "accounting.h"
#include "memory.h"
#include "uaccess.h"
#include <nexix/errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static struct proc processes[V7_NPROC];
static struct user user;
static unsigned char output[32768];
static const uintptr_t base=0x600000000000UL;
static int locked,saves,restores,writes,deny;
static size_t accessible=sizeof(output);
uintptr_t proc_irq_save(void){assert(!locked);locked=1;++saves;return 0x200;}
void proc_irq_restore(uintptr_t f){assert(locked && f==0x200);locked=0;++restores;}
struct proc *proc_current(void){return &processes[1];}
const struct proc *proc_table(void){assert(locked);return processes;}
bool proc_superuser(struct user *u){assert(locked);return u->u_uid==0;}
uint64_t timer_ticks(void){assert(locked);return UINT64_C(0x123456789);}
unsigned text_info_id(const struct text *t){return t?17:0;}
uint32_t page_total_count(void){return 5000;}
uint32_t page_free_count(void){return 1234;}
bool files_info(unsigned i,struct nx_kinfo_file *r)
{if(i!=174)return false;*r=(struct nx_kinfo_file){.slot=175,.refs=3,.offset=INT64_C(0x123456789)};return true;}
bool inodes_info(unsigned i,struct nx_kinfo_inode *r)
{if(i!=199)return false;*r=(struct nx_kinfo_inode){.slot=200,.dev=0x101,.ino=42};return true;}
bool text_info(unsigned i,struct nx_kinfo_text *r)
{if(i!=39)return false;*r=(struct nx_kinfo_text){.slot=40,.size=UINT64_C(0x100000000)};return true;}
bool mounts_info(unsigned i,struct nx_kinfo_mount *r)
{if(i!=7)return false;*r=(struct nx_kinfo_mount){.slot=8,.dev=0x102};return true;}
void block_statistics(struct nx_kinfo_io *r){r->read_sectors=UINT64_C(0x100000003);r->write_errors=7;}
void cache_statistics(struct nx_kinfo_io *r){r->cache_lookups=13;r->cache_hits=12;}
void tty_statistics(struct nx_kinfo_io *r){r->tty_in=9;r->tty_out=10;}
void cpu_statistics(struct nx_kinfo_io *r){r->user_ticks=1;r->nice_ticks=2;r->system_ticks=3;r->idle_ticks=4;}
bool user_access_ok(const struct address_space *vm,uintptr_t addr,size_t n,bool write)
{assert(locked && vm==&processes[1].p_vm && write);return !deny && addr>=base && addr-base<=accessible && n<=accessible-(addr-base);}
int copyout(const struct address_space *vm,uintptr_t addr,const void *src,size_t n)
{assert(user_access_ok(vm,addr,n,true));memcpy(output+addr-base,src,n);++writes;return 0;}
_Noreturn void panic(const char *s){fprintf(stderr,"panic: %s\n",s);abort();}
void loadavg_snapshot(struct nx_kinfo_load *r){*r=(struct nx_kinfo_load){.average={65536,32768,16384},.scale=65536};}
static int64_t request(unsigned kind,uintptr_t addr,size_t n)
{return kinfo_request(1,kind,addr,n);}
int main(void)
{
    processes[1]=(struct proc){.p_pid=42,.p_ppid=1,.p_stat=V7_SRUN,.p_user=&user,
        .p_size=UINT64_C(0x123456789),.p_wchan=(void *)0xdeadbeef,.p_textp=(void *)0xcafe};
    user.u_ruid=123;user.u_gid=456;user.u_utime=INT64_C(0x1234567890);
    memcpy(user.u_comm,"abcdefghijklmn",14);
    processes[2]=(struct proc){.p_pid=43,.p_stat=V7_SZOMB,.p_xstat=0x1700};
    processes[3].p_stat=V7_SIDL;
    processes[149]=(struct proc){.p_pid=44,.p_stat=V7_SSTOP};
    const unsigned counts[]={0,3,1,1,1,1,1,1,1};
    const unsigned sizes[]={0,144,40,40,32,48,32,96,32};
    for(unsigned kind=1;kind<=8;++kind){
        size_t n=48+counts[kind]*sizes[kind];
        memset(output,0xa5,sizeof(output));writes=0;
        assert(request(kind,0,0)==(int64_t)n && !writes);
        assert(request(kind,base,n-1)==-NX_ERANGE && !writes && output[0]==0xa5);
        assert(request(kind,base,sizeof(output))==(int64_t)n && writes==(int)counts[kind]+1);
        const struct nx_kinfo_header *h=(const void *)output;
        assert(h->version==1 && h->kind==kind && h->record_size==sizes[kind] && h->count==counts[kind]);
        assert(h->header_size==48 && h->total_bytes==n && h->ticks==UINT64_C(0x123456789));
        assert(h->tick_hz==100 && h->flags==1 && !h->reserved && output[n]==0xa5);
    }
    assert(request(1,base,sizeof(output))>0);
    const struct nx_kinfo_process *p=(const void *)(output+48);
    assert(p[0].pid==42 && p[0].ruid==123 && p[0].egid==456 && p[0].waiting==1 && p[0].text_id==17);
    assert(p[0].size_bytes==UINT64_C(0x123456789) && p[0].user_ticks==INT64_C(0x1234567890));
    assert(!memcmp(p[0].command,"abcdefghijklmn\0\0",16) && !p[0].reserved);
    assert(p[1].pid==43 && p[1].status==0x1700 && p[2].slot==150 && p[2].state==V7_SSTOP);
    assert(request(6,base,sizeof(output))>0);
    const struct nx_kinfo_memory *m=(const void *)(output+48);
    assert(m->total_pages==5000 && m->free_pages==1234 && m->allocated_pages==3766 && m->page_size==4096);
    assert(request(7,base,sizeof(output))>0);
    const struct nx_kinfo_io *io=(const void *)(output+48);
    assert(io->read_sectors==UINT64_C(0x100000003) && io->write_errors==7 && io->cache_hits==12 && io->tty_out==10 && io->idle_ticks==4);
    memset(output,0xa5,sizeof(output));writes=0;accessible=60;
    assert(request(1,base,sizeof(output))==-NX_EFAULT && !writes);
    accessible=sizeof(output);deny=1;assert(request(1,base,sizeof(output))==-NX_EFAULT && !writes);deny=0;
    assert(request(1,UINTPTR_MAX-10,sizeof(output))==-NX_EFAULT && !writes);
    assert(request(1,0,1)==-NX_EFAULT && !writes);
    assert(kinfo_request(UINT64_C(0x100000001),1,base,sizeof(output))==-NX_EINVAL);
    assert(kinfo_request(1,UINT64_C(0x100000001),base,sizeof(output))==-NX_EINVAL);
    assert(request(0,base,sizeof(output))==-NX_EINVAL && request(9,0,0)==-NX_EINVAL);
    user.u_uid=123;
    for(unsigned kind=2;kind<=7;++kind)assert(request(kind,0,0)==-NX_EPERM && request(kind,base,sizeof(output))==-NX_EPERM);
    assert(!writes);for(unsigned i=0;i<sizeof(output);++i)assert(output[i]==0xa5);
    assert(request(1,base,sizeof(output))>0);
    assert(request(8,base,sizeof(output))>0);
    assert(saves==restores && !locked);
    puts("PASS: snapshot versions, dense tables, 64-bit fields, bounds, atomic rejection and privilege");
    return 0;
}
