#include "signals.h"
#include "exec.h"
#include "files.h"
#include "tty.h"
#include "serial.h"
#include <nexix/errno.h>
#define REQUIRE(c) do { if (!(c)) panic("signals self-test: " #c); } while (0)
extern const uint8_t _binary_build_root_test_img_start[], _binary_build_root_test_img_end[];
extern const uint8_t _binary_build_signals_test_elf_start[], _binary_build_signals_test_elf_end[];
static uint8_t backing[1024 * V7_BSIZE];
static struct ramdisk ram;
static struct v7fs fs;
static struct exec_args args;
void signals_selftest(void)
{
    uint32_t before = page_free_count();
    for (unsigned pass = 0; pass < 3; ++pass) {
        for (size_t i = 0; i < sizeof(backing); ++i) backing[i] = _binary_build_root_test_img_start[i];
        REQUIRE(ramdisk_init(&ram, backing, sizeof(backing), pass == 1) == 0);
        REQUIRE(v7fs_mount(&fs, &ram.device, pass == 1) == 0 && files_mount(&fs) == 0);
        const char path[] = "/bin/signals-test\0ro\0real";
        for (size_t i = 0; i < sizeof(path); ++i) args.bytes[i] = path[i];
        args.used = sizeof(path); args.argc = pass ? 2 : 1; args.argv[0] = 0; args.argv[1] = pass == 2 ? 21 : 18;
        struct proc *p = proc_alloc(); REQUIRE(p);
        struct exec_image image = {0};
        size_t length = _binary_build_signals_test_elf_end - _binary_build_signals_test_elf_start;
        REQUIRE(elf_load(_binary_build_signals_test_elf_start, length, &args, &image) == 0);
        struct exec_image empty = {.vm = p->p_vm};
        p->p_vm = image.vm; p->p_size = image.size;
        p->p_brk_base = p->p_brk = image.heap;
        p->p_user->u_tsize = image.text; p->p_user->u_dsize = image.data + image.bss;
        p->p_user->u_ssize = EXEC_STACK_PAGES * PAGE_SIZE;
        p->p_user->u_exdata.entry = image.entry; p->p_user->u_exdata.text = image.text;
        p->p_user->u_exdata.data = image.data; p->p_user->u_exdata.bss = image.bss;
        exec_image_release(&empty);
        if (pass == 2) {
            p->p_user->u_ruid = 123;
            struct v7fs_inode inode;
            REQUIRE(v7fs_inode_get(&fs, 5, &inode) == 0);
            inode.mode &= ~0777;
            REQUIRE(v7fs_metadata(&fs, &inode) == 0);
        }
        REQUIRE(proc_start_user(p, image.entry, image.stack));
        v7_pid_t pid = p->p_pid;
        proc_run();
        int32_t status;
        REQUIRE(proc_collect(&status) == pid);
        if (status) { serial_puts("Nexix: signal test failure status="); serial_hex(status); serial_puts("\r\n"); }
        REQUIRE(status == 0 && proc_collect(NULL) == -NX_ECHILD);
        REQUIRE(files_live() == 0 && tty_open_count() == 0 && page_free_count() == before);
        REQUIRE(files_mount(NULL) == 0 && v7fs_unmount(&fs) == 0);
    }
    serial_puts("NEXIX_SIGNALS_OK\r\nNEXIX_HEAP_OK\r\nNEXIX_INIT_SYSCALLS_OK\r\nNEXIX_SIGNALS_CLEANUP_OK\r\n");
}
