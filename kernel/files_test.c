#include "files.h"
#include "exec.h"
#include "serial.h"
#include <nexix/errno.h>
#define REQUIRE(c) do { if (!(c)) panic("files self-test: " #c); } while (0)
extern const uint8_t _binary_build_root_test_img_start[], _binary_build_root_test_img_end[];
extern const uint8_t _binary_build_files_test_elf_start[], _binary_build_files_test_elf_end[];
static uint8_t backing[1024 * V7_BSIZE];
static struct ramdisk ram;
static struct v7fs fs;
static struct exec_args args;
static bool active;
static v7_pid_t root_pid;
static unsigned blocked_checks;
bool files_test_gate(struct interrupt_frame *frame)
{
    if (!active || frame->rax != 0x400 || !proc_current()) return false;
    REQUIRE(proc_current()->p_pid == root_pid);
    const struct proc *child = NULL;
    for (unsigned i = 1; i < V7_NPROC; ++i)
        if (proc_table()[i].p_stat != V7_UNUSED && (uint64_t)proc_table()[i].p_pid == frame->rdi) child = &proc_table()[i];
    REQUIRE(child && child->p_ppid == root_pid);
    while (child->p_stat == V7_SRUN) proc_yield();
    REQUIRE(child->p_stat == V7_SSLEEP && child->p_wchan);
    ++blocked_checks;
    frame->rax = 0;
    return true;
}
void files_selftest(void)
{
    uint32_t before = page_free_count();
    size_t length = (uintptr_t)_binary_build_root_test_img_end - (uintptr_t)_binary_build_root_test_img_start;
    REQUIRE(length == sizeof(backing));
    for (size_t i = 0; i < length; ++i) backing[i] = _binary_build_root_test_img_start[i];
    REQUIRE(ramdisk_init(&ram, backing, length, false) == 0);
    REQUIRE(v7fs_mount(&fs, &ram.device, false) == 0 && files_mount(&fs) == 0);
    uint32_t blocks = v7_disk_u32(fs.super.tfree);
    uint16_t inodes = v7_disk_u16(fs.super.tinode);
    const char path[] = "/bin/files-test\0permissions";
    for (size_t i = 0; i < sizeof(path); ++i) args.bytes[i] = path[i];
    args.used = sizeof(path); args.argc = 1; args.argv[0] = 0;
    /* Repetition checks reuse of descriptor, inode and pipe pools. */
    for (unsigned pass = 0; pass < 4; ++pass) {
        args.argc = pass == 3 ? 2 : 1;
        args.argv[1] = 16;
        struct proc *p = proc_alloc(); REQUIRE(p);
        struct exec_image image = {0};
        length = (uintptr_t)_binary_build_files_test_elf_end - (uintptr_t)_binary_build_files_test_elf_start;
        REQUIRE(elf_load(_binary_build_files_test_elf_start, length, &args, &image) == 0);
        struct exec_image empty = {.vm = p->p_vm};
        p->p_vm = image.vm; p->p_size = image.size; exec_image_release(&empty);
        p->p_user->u_cmask = 022;
        if (pass == 3) {
            p->p_uid = p->p_user->u_uid = p->p_user->u_ruid = 123;
            p->p_user->u_gid = p->p_user->u_rgid = 456;
        }
        REQUIRE(proc_start_user(p, image.entry, image.stack));
        v7_pid_t pid = p->p_pid;
        root_pid = pid; active = true;
        proc_run();
        active = false;
        int32_t status;
        REQUIRE(proc_collect(&status) == pid);
        if (status) { serial_puts("Nexix: file test failure status="); serial_hex(status); serial_puts("\r\n"); }
        REQUIRE(status == 0 && proc_collect(NULL) == -NX_ECHILD);
        REQUIRE(files_live() == 0 && page_free_count() == before);
        REQUIRE(v7_disk_u32(fs.super.tfree) == blocks && v7_disk_u16(fs.super.tinode) == inodes);
    }
    REQUIRE(blocked_checks == 3);
    REQUIRE(files_mount(NULL) == 0 && v7fs_unmount(&fs) == 0);
    serial_puts("NEXIX_FILES_OK\r\nNEXIX_PIPE_OK\r\nNEXIX_FILES_CLEANUP_OK\r\n");
}
