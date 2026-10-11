#include "userland.h"
#include "files.h"
#include "tty.h"
#include "serial.h"
#include <nexix/errno.h>
#define REQUIRE(c) do { if (!(c)) panic("V7 userland self-test: " #c); } while (0)
extern const uint8_t _binary_build_root_test_img_start[];
static uint8_t backing[1024*V7_BSIZE];
static struct ramdisk ram;
static struct v7fs fs;
static struct exec_args args;
void v7_userland_selftest(void)
{
    uint32_t before=page_free_count();
    const struct embedded_program *init=userland_program("/etc/init");
    REQUIRE(init);
    struct exec_image init_image={0};
    REQUIRE(elf_load(init->start,init->end-init->start,&args,&init_image)==0);
    exec_image_release(&init_image);
    REQUIRE(page_free_count()==before);
    serial_puts("NEXIX_V7_INIT_ELF_OK\r\n");
    for(unsigned pass=0;pass<2;++pass){
        for(size_t i=0;i<sizeof(backing);++i)backing[i]=_binary_build_root_test_img_start[i];
        REQUIRE(ramdisk_init(&ram,backing,sizeof(backing),false)==0);
        REQUIRE(v7fs_mount(&fs,&ram.device,false)==0 && files_mount(&fs)==0);
        uint32_t blocks=v7_disk_u32(fs.super.tfree);uint16_t inodes=v7_disk_u16(fs.super.tinode);
        const char name[]="/bin/v7-test";
        for(size_t i=0;i<sizeof(name);++i)args.bytes[i]=name[i];
        args.used=sizeof(name);args.argc=1;args.argv[0]=0;
        struct proc *p=proc_alloc();REQUIRE(p);
        const struct embedded_program *program=userland_program(name);
        struct exec_image image={0};REQUIRE(elf_load(program->start,program->end-program->start,&args,&image)==0);
        struct exec_image empty={.vm=p->p_vm};
        p->p_vm=image.vm;p->p_size=image.size;p->p_brk_base=p->p_brk=image.heap;
        p->p_user->u_exdata.data=image.data;p->p_user->u_exdata.bss=image.bss;
        exec_image_release(&empty);REQUIRE(proc_start_user(p,image.entry,image.stack));
        v7_pid_t pid=p->p_pid;proc_run();int32_t status;
        REQUIRE(proc_collect(&status)==pid);
        if(status){serial_puts("Nexix: V7 userland failure status=");serial_hex(status);serial_puts("\r\n");}
        REQUIRE(status==0 && proc_collect(NULL)==-NX_ECHILD);
        REQUIRE(files_live()==0 && tty_open_count()==0 && page_free_count()==before);
        REQUIRE(v7_disk_u32(fs.super.tfree)==blocks && v7_disk_u16(fs.super.tinode)==inodes);
        REQUIRE(files_mount(NULL)==0 && v7fs_unmount(&fs)==0);
    }
    serial_puts("NEXIX_V7_SHELL_OK\r\nNEXIX_V7_UTILITIES_OK\r\nNEXIX_V7_USERLAND_CLEANUP_OK\r\n");
}
