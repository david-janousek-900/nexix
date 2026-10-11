#include "userland.h"
#include "files.h"
#include "serial.h"
#include "tty.h"
#include <nexix/errno.h>
extern const uint8_t _binary_build_root_img_start[], _binary_build_root_img_end[];
static uint8_t root_bytes[1024 * V7_BSIZE];
static struct ramdisk root_device;
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
    serial_puts("NEXIX_ROOT_MOUNTED\r\n");
}
_Noreturn void rootfs_start(void)
{
    mount_root();
    v7_ino_t ino;
    if(v7fs_lookup(&root_volume,V7_ROOTINO,"/etc/init",&ino))panic("missing init");
    const char name[]="/etc/init";
    for(size_t i=0;i<sizeof(name);++i)init_args.bytes[i]=name[i];
    init_args.used=sizeof(name);init_args.argc=1;
    struct exec_image image={0};
    if(elf_load_inode(&root_volume,ino,&init_args,&image))panic("cannot load init");
    struct proc *p=proc_alloc();
    if(!p || p->p_pid!=1)panic("cannot allocate PID 1");
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
    serial_puts("NEXIX_INIT_PID1\r\nNEXIX_BOOT_OK\r\n");
    proc_run();
    panic("init scheduler returned");
}

extern const uint8_t _binary_build_root_exec_test_elf_start[], _binary_build_root_exec_test_elf_end[];
void root_exec_selftest(void)
{
    uint32_t pages=page_free_count();
    mount_root();
    uint32_t blocks=v7_disk_u32(root_volume.super.tfree);
    uint16_t inodes=v7_disk_u16(root_volume.super.tinode);
    struct exec_image image={0};
    if(elf_load(_binary_build_root_exec_test_elf_start,
        (uintptr_t)_binary_build_root_exec_test_elf_end-(uintptr_t)_binary_build_root_exec_test_elf_start,
        &init_args,&image))panic("cannot load root exec test");
    struct proc *p=proc_alloc();if(!p)panic("cannot allocate root exec test");
    struct exec_image empty={.vm=p->p_vm};
    p->p_vm=image.vm;p->p_size=image.size;p->p_brk_base=p->p_brk=image.heap;
    exec_image_release(&empty);
    if(!proc_start_user(p,image.entry,image.stack))panic("cannot start root exec test");
    v7_pid_t pid=p->p_pid;proc_run();int32_t status;
    if(proc_collect(&status)!=pid || status || proc_collect(NULL)!=-NX_ECHILD)
        panic("root exec test failed");
    if(files_live() || tty_open_count() || page_free_count()!=pages ||
        v7_disk_u32(root_volume.super.tfree)!=blocks ||
        v7_disk_u16(root_volume.super.tinode)!=inodes)
        panic("root exec cleanup failed");
    if(files_mount(NULL) || v7fs_unmount(&root_volume))panic("cannot detach test root");
    serial_puts("NEXIX_ROOT_EXEC_OK\r\nNEXIX_ROOT_EXEC_CLEANUP_OK\r\n");
}
