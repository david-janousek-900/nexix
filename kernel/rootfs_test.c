#include "console.h"
#include "exec.h"
#include "v7/fs.h"
#include "serial.h"
#include "userland.h"
#include <nexix/errno.h>
#include <nexix/file.h>
#define REQUIRE(c) do { if (!(c)) panic("root image self-test: " #c); } while (0)
extern uint8_t _binary_build_root_img_start[], _binary_build_root_img_end[];
static struct ramdisk root_ram;
static struct v7fs root_fs;
static uint8_t executable[131072];
static struct exec_args root_args;
void rootfs_selftest(void)
{
    uint32_t pages=page_free_count();
    size_t length=(uintptr_t)_binary_build_root_img_end-(uintptr_t)_binary_build_root_img_start;
    REQUIRE(ramdisk_init(&root_ram,_binary_build_root_img_start,length,true)==0);
    REQUIRE(v7fs_mount(&root_fs,&root_ram.device,true)==0);
    const char *paths[]={"/etc/init","/bin/sh","/bin/echo","/bin/cat","/bin/ls","/bin/pwd","/bin/cp","/bin/ln","/bin/rm","/bin/date","/bin/touch"};
    for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);++i){
        v7_ino_t ino;struct v7fs_inode node;size_t done;
        REQUIRE(v7fs_lookup(&root_fs,2,paths[i],&ino)==0);
        REQUIRE(v7fs_inode_get(&root_fs,ino,&node)==0);
        REQUIRE(node.mode==(V7_IFREG|0755) && node.uid==0 && node.gid==0);
        REQUIRE(node.size<=sizeof(executable));
        REQUIRE(v7fs_read(&root_fs,ino,0,executable,node.size,&done)==0 && done==node.size);
        const struct embedded_program *original=userland_program(paths[i]);
        REQUIRE(original && node.size==(uintptr_t)original->end-(uintptr_t)original->start);
        for(size_t j=0;j<node.size;++j)REQUIRE(executable[j]==original->start[j]);
        struct exec_image image={0};
        REQUIRE(elf_load(executable,node.size,&root_args,&image)==0);
        exec_image_release(&image);
        REQUIRE(page_free_count()==pages);
    }
    const char *dirs[]={"/","/bin","/etc","/dev","/tmp","/usr","/usr/adm"};
    for(unsigned i=0;i<sizeof(dirs)/sizeof(dirs[0]);++i){
        v7_ino_t ino;struct v7fs_inode node;
        REQUIRE(v7fs_lookup(&root_fs,2,dirs[i],&ino)==0);
        REQUIRE(v7fs_inode_get(&root_fs,ino,&node)==0 && node.mode==(V7_IFDIR|0755));
    }
    v7_ino_t console,alias,tty;struct v7fs_inode node;
    REQUIRE(v7fs_lookup(&root_fs,2,"/dev/console",&console)==0);
    REQUIRE(v7fs_lookup(&root_fs,2,"/dev/ttyS0",&alias)==0 && alias==console);
    REQUIRE(v7fs_inode_get(&root_fs,console,&node)==0 && node.mode==(V7_IFCHR|0600) && node.nlink==2 && node.addr[0]==0x100);
    REQUIRE(v7fs_lookup(&root_fs,2,"/dev/tty",&tty)==0);
    REQUIRE(v7fs_inode_get(&root_fs,tty,&node)==0 && node.addr[0]==0x200);
    v7_ino_t null;
    REQUIRE(v7fs_lookup(&root_fs,2,"/dev/null",&null)==0);
    REQUIRE(v7fs_inode_get(&root_fs,null,&node)==0 && node.mode==(V7_IFCHR|0666) && node.addr[0]==NX_NULL_DEV);
    const char *config[]={"/etc/rc","/etc/ttys","/etc/passwd","/.profile","/etc/utmp","/usr/adm/wtmp"};
    for(unsigned i=0;i<sizeof(config)/sizeof(config[0]);++i){
        v7_ino_t ino;
        REQUIRE(v7fs_lookup(&root_fs,2,config[i],&ino)==0);
        REQUIRE(v7fs_inode_get(&root_fs,ino,&node)==0 && node.mode==(V7_IFREG|0644));
        if(i==1||i>=4)REQUIRE(node.size==0);
    }
    REQUIRE(v7fs_unmount(&root_fs)==0 && page_free_count()==pages);
    console_puts("NEXIX_ROOT_IMAGE_OK\r\nNEXIX_ROOT_ELF_OK\r\n");
}
