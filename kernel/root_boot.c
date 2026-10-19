#include "console.h"
#include "uaccess.h"
#include "userland.h"
#include "files.h"
#include "serial.h"
#include "tty.h"
#include "ide.h"
#include "storage.h"
#include "namespace.h"
#include "clock.h"
#include "profile.h"
#include <nexix/errno.h>
#include <nexix/signal.h>
extern const uint8_t _binary_build_root_img_start[], _binary_build_root_img_end[];
static uint8_t root_bytes[1024 * V7_BSIZE];
static struct ramdisk root_device;
static struct block_device ide_device;
static struct v7fs root_volume;
static struct exec_args init_args;
static void mount_root(void)
{
    size_t length=(uintptr_t)_binary_build_root_img_end-(uintptr_t)_binary_build_root_img_start;
    if(length!=sizeof(root_bytes))panic("unsupported root volume size");
    for(size_t i=0;i<length;++i)root_bytes[i]=_binary_build_root_img_start[i];
    if(ramdisk_init(&root_device,root_bytes,length,false) ||
       v7fs_mount(&root_volume,&root_device.device,false) || files_mount_root(&root_volume))
        panic("cannot mount root");
    console_puts("ROOT MOUNTED\r\n");
}
_Noreturn void rootfs_start(void)
{
    disk_native_init();
    int error=ide_init(&ide_device);
    if(error==-NX_ENODEV) {
        mount_root();
        console_puts("ROOT RAM\r\n");
    } else {
        if(error || v7fs_mount(&root_volume,&ide_device,false) ||
           files_mount_root(&root_volume)) panic("cannot mount IDE root");
        console_puts("ROOT IDE\r\nROOT MOUNTED\r\n");
    }
    v7_ino_t ino;
    if(v7fs_lookup(&root_volume,V7_ROOTINO,"/etc/init",&ino))panic("missing init");
    const char name[]="/etc/init";
    for(size_t i=0;i<sizeof(name);++i)init_args.bytes[i]=name[i];
    init_args.used=sizeof(name);init_args.argc=1;
    const char *environment = console_mode() == BOOT_CONSOLE_VGA ? "CONSOLE=vga" :
        console_mode() == BOOT_CONSOLE_SERIAL ? "CONSOLE=serial" : "CONSOLE=multi";
    init_args.envc=1;init_args.envp[0]=init_args.used;
    do { init_args.bytes[init_args.used++]=*environment; } while (*environment++);
    struct exec_image image={0};
    if(elf_load_inode(&root_volume,ino,&init_args,&image)||files_image_text(ino,&image))panic("cannot load init");
    struct proc *p=proc_alloc();
    if(!p || p->p_pid!=1)panic("cannot allocate PID 1");
    p->p_textp=image.owner;image.owner=NULL;
    p->p_pgrp=0; /* Init has no terminal; its console child forms the group. */
    struct exec_image empty={.vm=p->p_vm};
    p->p_vm=image.vm;p->p_size=image.size;p->p_brk_base=p->p_brk=image.heap;
    p->p_user->u_exdata.entry=image.entry;p->p_user->u_exdata.text=image.text;
    p->p_user->u_exdata.data=image.data;p->p_user->u_exdata.bss=image.bss;
    p->p_user->u_tsize=image.text;p->p_user->u_dsize=image.data+image.bss;
    p->p_user->u_ssize=EXEC_STACK_PAGES*PAGE_SIZE;
    p->p_user->u_comm[0]='i';p->p_user->u_comm[1]='n';p->p_user->u_comm[2]='i';p->p_user->u_comm[3]='t';
    exec_image_release(&empty);
    if(!proc_start_init(p,image.entry,image.stack))panic("cannot start init");
    console_puts("INIT PID1\r\nBOOT OK\r\n");
    proc_run();
    panic("init scheduler returned");
}

extern const uint8_t _binary_build_root_exec_test_elf_start[], _binary_build_root_exec_test_elf_end[];
extern const uint8_t _binary_build_setid_probe_elf_start[], _binary_build_setid_probe_elf_end[];
extern const uint8_t _binary_build_text_probe_elf_start[], _binary_build_text_probe_elf_end[];
extern const uint8_t _binary_build_core_probe_elf_start[], _binary_build_core_probe_elf_end[];
extern const uint8_t _binary_build_trace_image_elf_start[], _binary_build_trace_image_elf_end[];
extern const uint8_t _binary_build_monitor_fixture_elf_start[], _binary_build_monitor_fixture_elf_end[];
extern const uint8_t _binary_build_fmonitor_fixture_elf_start[], _binary_build_fmonitor_fixture_elf_end[];
extern const uint8_t _binary_build_namespace_img_start[],_binary_build_namespace_img_end[];
static uint8_t namespace_bytes[7][256*BLOCK_SIZE];
static struct ramdisk namespace_devices[7];
static int (*namespace_writes[7])(struct block_device *,uint32_t,const void *);
static unsigned namespace_write_faults,namespace_all_write_faults;
static int namespace_fault_write(struct block_device *d,uint32_t block,const void *bytes)
{
    for(unsigned i=0;i<7;++i)if(d==&namespace_devices[i].device){
        if((namespace_all_write_faults&(1U<<i))||(block==V7_SUPERB&&(namespace_write_faults&(1U<<i))))return -NX_EIO;
        return namespace_writes[i](d,block,bytes);
    }
    panic("foreign namespace fault device");
}
static int namespace_bad_read(struct block_device *d,uint32_t block,void *bytes)
{(void)d;(void)block;(void)bytes;return -NX_EIO;}
static void namespace_fixture_device(unsigned i)
{
    size_t length=(uintptr_t)_binary_build_namespace_img_end-(uintptr_t)_binary_build_namespace_img_start;
    if(length!=sizeof(namespace_bytes[i]))panic("namespace fixture size");
    for(size_t n=0;n<length;++n)namespace_bytes[i][n]=_binary_build_namespace_img_start[n];
    if(ramdisk_init(&namespace_devices[i],namespace_bytes[i],length,i==2)||
       storage_bind(i+1,&namespace_devices[i].device,false))panic("namespace fixture bind");
    namespace_writes[i]=namespace_devices[i].device.write;
    namespace_devices[i].device.write=namespace_fault_write;
}
static uint8_t disk_sector[512];
static uint32_t disk_saved=UINT32_MAX;
static int disk_fixture_read(struct block_device *d,uint32_t b,void *p)
{(void)d;for(unsigned i=0;i<512;++i)((uint8_t *)p)[i]=b==disk_saved?disk_sector[i]:(uint8_t)(b+i);return 0;}
static int disk_fixture_write(struct block_device *d,uint32_t b,const void *p)
{(void)d;for(unsigned i=0;i<512;++i)disk_sector[i]=((const uint8_t *)p)[i];disk_saved=b;return 0;}
static struct block_device disk_fixture={.blocks=340670,.read=disk_fixture_read,.write=disk_fixture_write};
/* Task 73: two isolated sparse RAM media; positive tags gate real ATA tests. */
static uint8_t order_sectors[2][512];
static uint32_t order_saved[2];
static bool order_native;
static struct block_device order_ata[2];
static int order_read(struct block_device *d,uint32_t b,void *bytes)
{
    unsigned u=(uintptr_t)d->context;
    for(unsigned i=0;i<512;++i)((uint8_t *)bytes)[i]=b==order_saved[u]?order_sectors[u][i]:(uint8_t)(b+31*u+i);
    return 0;
}
static int order_write(struct block_device *d,uint32_t b,const void *bytes)
{
    unsigned u=(uintptr_t)d->context;order_saved[u]=b;
    for(unsigned i=0;i<512;++i)order_sectors[u][i]=((const uint8_t *)bytes)[i];
    return 0;
}
static struct block_device order_ram[2]={
    {.blocks=340670,.context=(void *)0,.read=order_read,.write=order_write},
    {.blocks=340670,.context=(void *)1,.read=order_read,.write=order_write}};
static int order_batch(void)
{
    struct disk_view view;int error=disk_acquire(NX_DISK_DEV(NX_DISK_HP,0,false),false,&view);if(error)return error;
    struct disk_request requests[3]={0};uint8_t bytes[3][512];unsigned blocks[]={836,0,418};
    for(unsigned i=0;i<3;++i){error=disk_submit(&view,&requests[i],blocks[i],bytes[i],false);if(error)break;}
    int dispatch=disk_dispatch(view.binding);if(!error)error=dispatch;
    for(unsigned i=0;!error && i<3;++i)
        if(!requests[i].done || requests[i].error || bytes[i][0]!=(uint8_t)(i==1 && order_native?'N':blocks[i]))error=-NX_EIO;
    int release=disk_release(&view);return error?error:release;
}
static bool exec_testing;
static uint64_t memdev_probe=UINT64_C(0x123456789abcdef0);
extern const uint8_t kernel_rodata_start[];
/* Test-held physical addresses are below the 1 GiB allocator limit. */
_Static_assert(PHYSICAL_LIMIT<=UINT32_MAX,"test frame address width");
static uint32_t held_pages[32768];
static unsigned held_count;
static v7_pid_t held_owner;
static v7_pid_t trace_orphan;
bool root_exec_test_gate(struct interrupt_frame *frame)
{
    if(!exec_testing||!proc_current()||frame->rax<0x900||frame->rax>0x91f)return false;
    struct proc *p=proc_current();
    if(frame->rax==0x900){
        frame->rax=(uint64_t)-NX_ENOSYS;return true; /* Retired tracing test hook. */
    }else if(frame->rax==0x901){frame->rax=p->p_uid;return true;}
    else if(frame->rax==0x902){
        if(held_count||frame->rdi>16)panic("invalid exec fault injection");
        held_owner=p->p_pid;
        while(page_free_count()>frame->rdi){
            if(held_count==sizeof(held_pages)/sizeof(held_pages[0]))panic("exec fault fixture capacity");
            uintptr_t page=page_alloc();if(!page)panic("exec fault page allocation");
            held_pages[held_count++]=page;
        }
    }else if(frame->rax==0x903){
        if(held_owner!=p->p_pid)panic("foreign exec fault cleanup");
        while(held_count)if(!page_free(held_pages[--held_count]))panic("exec fault page cleanup");
        held_owner=0;
    }else if(frame->rax==0x904){
        if(p->p_user->u_uid)panic("unprivileged text fixture install");
        v7_ino_t directory,ino;size_t done;
        const char name[V7_DIRSIZ]="text";
        size_t size=(uintptr_t)_binary_build_text_probe_elf_end-(uintptr_t)_binary_build_text_probe_elf_start;
        if(v7fs_lookup(&root_volume,V7_ROOTINO,"/tmp",&directory)||
           v7fs_create(&root_volume,directory,name,V7_IFREG|0755,0,0,&ino)||
           v7fs_write(&root_volume,ino,0,_binary_build_text_probe_elf_start,size,&done)||done!=size)
            panic("cannot install text fixture");
    }else if(frame->rax==0x905){
        frame->rax=p->p_textp?p->p_textp->x_count:0;return true;
    }else if(frame->rax==0x907){
        if(p->p_user->u_uid)panic("unprivileged core fixture install");
        v7_ino_t directory,ino;size_t done;const char name[V7_DIRSIZ]="core-probe";
        size_t size=(uintptr_t)_binary_build_core_probe_elf_end-(uintptr_t)_binary_build_core_probe_elf_start;
        if(v7fs_lookup(&root_volume,V7_ROOTINO,"/tmp/core-dir",&directory)||
           v7fs_create(&root_volume,directory,name,V7_IFREG|0755,0,0,&ino)||
           v7fs_write(&root_volume,ino,0,_binary_build_core_probe_elf_start,size,&done)||done!=size)
            panic("cannot install core fixture");
    }else if(frame->rax==0x908){
        if(p->p_user->u_uid)panic("unprivileged core mount fixture");
        root_volume.read_only=!!frame->rdi;
    }else if(frame->rax==0x909){
        if(p->p_user->u_uid)panic("unprivileged namespace fixture");
        namespace_write_faults=namespace_all_write_faults=0;
        for(unsigned i=0;i<7;++i)namespace_fixture_device(i);
        for(unsigned i=0;i<BLOCK_SIZE;++i)namespace_bytes[3][BLOCK_SIZE+i]=0;
        namespace_devices[4].device.read=namespace_bad_read;
    }else if(frame->rax==0x90a){
        if(p->p_user->u_uid)panic("unprivileged namespace repair fixture");
        namespace_fixture_device(3);namespace_fixture_device(4);
    }else if(frame->rax==0x90b){
        if(p->p_user->u_uid)panic("unprivileged namespace teardown fixture");
        frame->rax=files_reset_volumes();return true;
    }else if(frame->rax==0x90c){
        if(p->p_user->u_uid||frame->rdi>127)panic("invalid namespace write fault");
        namespace_write_faults=frame->rdi;
    }else if(frame->rax==0x90d){
        if(p->p_user->u_uid||!frame->rdi||frame->rdi>=STORAGE_SLOTS)panic("invalid namespace fault recovery");
        struct v7fs *fs=namespace_volume(frame->rdi);
        if(!fs||namespace_write_faults||namespace_all_write_faults)panic("namespace fault recovery ordering");
        fs->failed=false; /* Injected callbacks reject writes before transfer; fixture-only recovery. */
    }else if(frame->rax==0x90e){
        if(p->p_user->u_uid)panic("unprivileged accounting seed");
        struct user *u=p->p_user;
        const char name[V7_DIRSIZ]="abcdefghijklmn";
        for(unsigned i=0;i<V7_DIRSIZ;++i)u->u_comm[i]=name[i];
        u->u_utime=8196;u->u_stime=65535;u->u_cutime=12345;u->u_cstime=54321;
        u->u_start=clock_now()-17;
    }else if(frame->rax==0x90f){
        if(p->p_user->u_uid||frame->rdi>127)panic("invalid accounting write fault");
        namespace_all_write_faults=frame->rdi;
    }else if(frame->rax==0x910){
        /* Register exactly one stopped grandchild for bootstrap reap checks.
         * No process state is changed: actual debugger exit must release it. */
        if(p->p_user->u_uid||trace_orphan)panic("invalid trace orphan fixture");
        const struct proc *child=NULL,*debugger=NULL;
        for(unsigned i=1;i<V7_NPROC;++i){
            const struct proc *q=&proc_table()[i];
            if(q->p_stat==V7_SSTOP&&q->p_pid==(int64_t)frame->rdi)child=q;
        }
        if(child)for(unsigned i=1;i<V7_NPROC;++i){
            const struct proc *q=&proc_table()[i];
            if(q->p_stat!=V7_UNUSED&&q->p_pid==child->p_ppid&&q->p_ppid==p->p_pid)debugger=q;
        }
        if(!debugger)panic("trace fixture is not a stopped grandchild");
        trace_orphan=child->p_pid;
    }else if(frame->rax==0x911){
        if(p->p_user->u_uid)panic("unprivileged trace image install");
        v7_ino_t directory,ino;size_t done;const char name[V7_DIRSIZ]="trace";
        size_t size=(uintptr_t)_binary_build_trace_image_elf_end-(uintptr_t)_binary_build_trace_image_elf_start;
        if(v7fs_lookup(&root_volume,V7_ROOTINO,"/tmp",&directory)||
           v7fs_create(&root_volume,directory,name,V7_IFREG|0755,0,0,&ino)||
           v7fs_write(&root_volume,ino,0,_binary_build_trace_image_elf_start,size,&done)||done!=size)
            panic("cannot install trace image");
    }else if(frame->rax==0x912){
        frame->rax=p->p_user->u_prof.pr_scale;return true;
    }else if(frame->rax==0x913){
        if(p->p_user->u_uid)panic("unprivileged profile sample fixture");
        profile_sample(p,frame->rdi,frame->rsi);
    }else if(frame->rax==0x914){
        if(p->p_user->u_uid)panic("unprivileged profile system fixture");
        v7_time_t start=p->p_user->u_stime;
        while(p->p_user->u_stime-start<3)__asm__ volatile("sti; hlt; cli":::"memory");
        frame->rax=p->p_user->u_stime-start;return true;
    }else if(frame->rax==0x915){
        if(p->p_user->u_uid)panic("unprivileged profile sleep fixture");
        v7_time_t start=p->p_user->u_stime;proc_delay(3);
        frame->rax=p->p_user->u_stime-start;return true;
    }else if(frame->rax==0x916){
        if(p->p_user->u_uid||frame->rdi>1)panic("invalid monitor fixture install");
        const uint8_t *start=frame->rdi?_binary_build_fmonitor_fixture_elf_start:_binary_build_monitor_fixture_elf_start;
        const uint8_t *end=frame->rdi?_binary_build_fmonitor_fixture_elf_end:_binary_build_monitor_fixture_elf_end;
        v7_ino_t directory,ino;size_t done;const char name[V7_DIRSIZ]="monitor";
        size_t size=(uintptr_t)end-(uintptr_t)start;
        if(v7fs_lookup(&root_volume,V7_ROOTINO,"/tmp",&directory)||
           v7fs_create(&root_volume,directory,name,V7_IFREG|0755,0,0,&ino)||
           v7fs_write(&root_volume,ino,0,start,size,&done)||done!=size)
            panic("cannot install monitor fixture");
    }else if(frame->rax==0x917){
        if(p->p_user->u_uid)panic("unprivileged free physical fixture");
        for(uintptr_t a=PAGE_SIZE;a<PHYSICAL_LIMIT-PAGE_SIZE;a+=PAGE_SIZE)
            if(page_is_eligible(a)&&page_is_eligible(a+PAGE_SIZE)&&
               !page_is_allocated(a)&&!page_is_allocated(a+PAGE_SIZE)){
                frame->rax=a;return true;
            }
        panic("no free physical fixture pair");
    }else if(frame->rax==0x918){
        uintptr_t pa;unsigned flags;
        if(p->p_user->u_uid||!vm_translate(&p->p_vm,frame->rdi,&pa,&flags))panic("invalid physical lookup fixture");
        frame->rax=pa;frame->rdx=flags;return true;
    }else if(frame->rax==0x919){
        uint8_t input[128];
        if(p->p_user->u_uid || frame->rsi>sizeof(input))panic("invalid mpx TTY fixture");
        int error=copyin(&p->p_vm,frame->rdi,input,frame->rsi);
        if(error){frame->rax=error;return true;}
        for(unsigned i=0;i<frame->rsi;++i)tty_receive(input[i]);
    }else if(frame->rax==0x91a){frame->rax=tty_open_count();return true;}
    else if(frame->rax==0x91b){
        if(p->p_user->u_uid)panic("unprivileged memory fixture");
        frame->rax=(uintptr_t)&memdev_probe;frame->rdx=(uintptr_t)kernel_rodata_start;
        frame->rcx=memdev_probe;return true;
    }
    else if(frame->rax==0x91c){
        if(p->p_user->u_uid)panic("unprivileged disk fixture");
        if(frame->rdi==3){
            disk_native_init();struct disk_view view;
            int error=disk_acquire(NX_DISK_DEV(NX_DISK_RP,0,false),false,&view);
            if(!error)disk_release(&view);
            frame->rax=error;return true;
        }
        if(frame->rdi==0){
            disk_saved=UINT32_MAX;disk_fixture.read_only=false;
            for(unsigned kind=0;kind<5;++kind)if(disk_bind(kind,0,&disk_fixture))panic("disk fixture bind");
        }else if(frame->rdi==1){
            for(unsigned kind=0;kind<5;++kind)for(unsigned unit=0;unit<8;++unit){
                int error=disk_bind(kind,unit,NULL);
                if(error && error!=-NX_ENODEV)panic("disk fixture leaked handles");
            }
        }else if(frame->rdi==2)disk_fixture.read_only=!!frame->rsi;
        else panic("invalid disk fixture operation");
    }
    else if(frame->rax==0x91d){
        static struct block_device tape_aux;
        if(p->p_user->u_uid)panic("unprivileged tape fixture");
        if(frame->rdi==1){frame->rax=disk_bind(NX_DISK_RP,0,NULL);return true;}
        int error=ide_init_unit(&tape_aux,1);uint8_t bytes[512];
        if(!error)error=block_read(&tape_aux,0,1,bytes);
        const char *tag=frame->rdi==2?"DECTAPE TEST ONLY72":"TAPE TEST ONLY71";
        if(!error)for(unsigned i=0;tag[i];++i)if(bytes[i]!=(uint8_t)tag[i]){error=-NX_ENXIO;break;}
        if(!error)error=disk_bind(NX_DISK_RP,0,&tape_aux);
        frame->rax=error;return true;
    }
    else if(frame->rax==0x91e){
        if(p->p_user->u_uid)panic("unprivileged ordering fixture");
        int error=0;
        if(frame->rdi==0){
            order_native=false;
            for(unsigned u=0;u<2;++u){order_saved[u]=UINT32_MAX;order_ram[u].read_only=false;error=disk_bind(NX_DISK_HP,u,&order_ram[u]);if(error)break;}
        }else if(frame->rdi==1){
            for(unsigned u=0;u<2;++u){error=disk_bind(NX_DISK_HP,u,NULL);if(error)break;}
        }else if(frame->rdi==2){
            uint8_t bytes[512];const char tag[]="ORDER TEST ONLY73";
            for(unsigned u=0;u<2 && !error;++u){
                error=ide_init_unit(&order_ata[u],u+1);
                if(!error)error=block_read(&order_ata[u],0,1,bytes);
                if(!error)for(unsigned i=0;i<sizeof(tag)-1;++i)if(bytes[i]!=(uint8_t)tag[i]){error=-NX_ENXIO;break;}
                if(!error && bytes[32]!='0'+u)error=-NX_ENXIO;
            }
            if(!error){order_native=true;for(unsigned u=0;u<2;++u)if(disk_bind(NX_DISK_HP,u,&order_ata[u]))panic("order native bind");}
        }else if(frame->rdi==3)error=order_batch();
        else if(frame->rdi==4)order_ram[1].read_only=!!frame->rsi;
        else error=-NX_EINVAL;
        frame->rax=error;return true;
    }
    else if(frame->rax==0x91f){
        if(p->p_user->u_uid)panic("unprivileged controller fixture");
        if(frame->rdi==256){frame->rax=timer_ticks();return true;}
        if(frame->rdi>255 || frame->rsi&~15U){frame->rax=-NX_EINVAL;return true;}
        tty_receive_status(frame->rdi,frame->rsi);
    }
    else return false;
    frame->rax=0;return true;
}
static uint8_t storage_bytes[1536];
static struct ramdisk storage_fixture,storage_readonly;
void root_exec_selftest(void)
{
    if(ramdisk_init(&storage_fixture,storage_bytes,sizeof(storage_bytes),false)||
       ramdisk_init(&storage_readonly,storage_bytes,sizeof(storage_bytes),true)||
       storage_bind(1,&storage_fixture.device,false)||storage_bind(2,&storage_readonly.device,false))
        panic("cannot bind storage fixture");
    uint32_t pages=page_free_count();
    mount_root();
    uint32_t blocks=v7_disk_u32(root_volume.super.tfree);
    uint16_t inodes=v7_disk_u16(root_volume.super.tinode);
    v7_ino_t temporary;
    if(v7fs_lookup(&root_volume,V7_ROOTINO,"/tmp",&temporary))panic("missing test temporary directory");
    const char names[][V7_DIRSIZ]={"setid"};
    size_t probe_size=(uintptr_t)_binary_build_setid_probe_elf_end-(uintptr_t)_binary_build_setid_probe_elf_start;
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i){
        v7_ino_t probe;size_t written;
        if(v7fs_create(&root_volume,temporary,names[i],V7_IFREG|0755,0,0,&probe)||
           v7fs_write(&root_volume,probe,0,_binary_build_setid_probe_elf_start,probe_size,&written)||written!=probe_size)
            panic("cannot install setid fixture");
    }
    struct exec_image image={0};
    if(elf_load(_binary_build_root_exec_test_elf_start,
        (uintptr_t)_binary_build_root_exec_test_elf_end-(uintptr_t)_binary_build_root_exec_test_elf_start,
        &init_args,&image))panic("cannot load root exec test");
    struct proc *p=proc_alloc();if(!p)panic("cannot allocate root exec test");
    struct exec_image empty={.vm=p->p_vm};
    p->p_vm=image.vm;p->p_size=image.size;p->p_brk_base=p->p_brk=image.heap;
    exec_image_release(&empty);
    if(!proc_start_user(p,image.entry,image.stack))panic("cannot start root exec test");
    v7_pid_t pid=p->p_pid;exec_testing=true;proc_run();exec_testing=false;int32_t status;
    if(held_count)panic("exec fault fixture leaked pages");
    if(proc_collect(&status)!=pid || status) {
        console_puts("root exec failure status=");console_hex(status);console_puts("\r\n");
        panic("root exec test failed");
    }
    if(!trace_orphan||proc_collect(&status)!=trace_orphan||status!=NX_SIGINT||proc_collect(NULL)!=-NX_ECHILD)
        panic("trace orphan release/reap failed");
    trace_orphan=0;
    console_puts("PTRACE ORPHAN OK\r\n");
    if(files_live() || tty_open_count() || page_free_count()!=pages ||
        v7_disk_u32(root_volume.super.tfree)!=blocks ||
        v7_disk_u16(root_volume.super.tinode)!=inodes)
        panic("root exec cleanup failed");
    for(unsigned i=1;i<STORAGE_SLOTS;++i)if(storage_bind(i,NULL,false))panic("storage handles leaked");
    if(files_mount(NULL) || v7fs_unmount(&root_volume))panic("cannot detach test root");
    console_puts("ROOT EXEC OK\r\nROOT EXEC CLEANUP OK\r\n");
}
