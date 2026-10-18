#include "tty.h"
#include "files.h"
#include "exec.h"
#include "serial.h"
#include "uaccess.h"
#include <nexix/errno.h>
#define REQUIRE(c) do { if (!(c)) panic("tty self-test: " #c); } while (0)
extern const uint8_t _binary_build_root_test_img_start[], _binary_build_root_test_img_end[];
extern const uint8_t _binary_build_tty_test_elf_start[], _binary_build_tty_test_elf_end[];
extern const uint8_t _binary_build_packet_test_elf_start[], _binary_build_packet_test_elf_end[];
static uint8_t backing[1024 * V7_BSIZE], input[4096];
static struct ramdisk ram;
static struct v7fs fs;
static struct exec_args args;
static bool active, uart;
static v7_pid_t root_pid;
static unsigned sleeps;
bool tty_test_gate(struct interrupt_frame *frame)
{
    if (!active || frame->rax < 0x500 || frame->rax > 0x505 || !proc_current()) return false;
    struct proc *p = proc_current();
    REQUIRE(p->p_pid == root_pid);
    if (frame->rax == 0x500) {
        REQUIRE(!uart && frame->rsi <= sizeof(input));
        int error = copyin(&p->p_vm, frame->rdi, input, frame->rsi);
        if (error) { frame->rax = error; return true; }
        for (size_t i = 0; i < frame->rsi; ++i) tty_receive(input[i]);
    } else if (frame->rax == 0x502) {
        REQUIRE(p->p_user->u_ttyp && p->p_user->u_ttyd == NX_CONSOLE_DEV);
        frame->rax = tty_open_count(); return true;
    } else {
        const struct proc *child = NULL;
        for (unsigned i = 1; i < V7_NPROC; ++i)
            if (proc_table()[i].p_stat != V7_UNUSED && (uint64_t)proc_table()[i].p_pid == frame->rdi) child = &proc_table()[i];
        REQUIRE(child && child->p_ppid == root_pid);
        while (child->p_stat == V7_SRUN) proc_yield();
        REQUIRE(child->p_stat == V7_SSLEEP && child->p_wchan);
        ++sleeps;
        if (frame->rax == 0x504 || frame->rax == 0x505) {
            REQUIRE(uart);
            serial_puts(frame->rax == 0x504 ? "NEXIX_TTY_UART_INTR_READY\r\n" : "NEXIX_TTY_UART_QUIT_READY\r\n");
        }
        if (frame->rax == 0x503) {
            REQUIRE(uart);
            serial_puts("NEXIX_TTY_UART_START_READY\r\n");
        }
    }
    frame->rax = 0; return true;
}
void tty_selftest(enum boot_test test)
{
    uint32_t before = page_free_count();
    size_t length = (uintptr_t)_binary_build_root_test_img_end - (uintptr_t)_binary_build_root_test_img_start;
    REQUIRE(length == sizeof(backing));
    for (size_t i = 0; i < length; ++i) backing[i] = _binary_build_root_test_img_start[i];
    REQUIRE(ramdisk_init(&ram, backing, length, false) == 0);
    const char strings[] = "/bin/tty-test\0uart\0demo";
    for (size_t i = 0; i < sizeof(strings); ++i) args.bytes[i] = strings[i];
    args.used = sizeof(strings); args.argv[0] = 0;
    unsigned passes = (test == BOOT_TEST_TTY || test == BOOT_TEST_TTY_DEMO || test == BOOT_TEST_PACKET) ? 3 : 2;
    for (unsigned pass = 0; pass < passes; ++pass) {
        REQUIRE(v7fs_mount(&fs, &ram.device, pass == 1) == 0 && files_mount(&fs) == 0);
        args.argc = pass == 2 ? 2 : 1;
        args.argv[1] = test == BOOT_TEST_TTY_DEMO ? 19 : 14;
        uart = pass == 2;
        struct proc *p = proc_alloc(); REQUIRE(p);
        struct exec_image image = {0};
        length = (uintptr_t)_binary_build_tty_test_elf_end - (uintptr_t)_binary_build_tty_test_elf_start;
        const uint8_t *program=_binary_build_tty_test_elf_start;
        if(pass==2 && test==BOOT_TEST_PACKET){program=_binary_build_packet_test_elf_start;
            length=(uintptr_t)_binary_build_packet_test_elf_end-(uintptr_t)program;}
        REQUIRE(elf_load(program, length, &args, &image) == 0);
        struct exec_image empty = {.vm = p->p_vm};
        p->p_vm = image.vm; p->p_size = image.size; p->p_brk_base = p->p_brk = image.heap; exec_image_release(&empty);
        REQUIRE(proc_start_user(p, image.entry, image.stack));
        root_pid = p->p_pid; active = true;
        proc_run(); active = false;
        int32_t status;
        REQUIRE(proc_collect(&status) == root_pid);
        if (status) { serial_puts("Nexix: tty test failure status="); serial_hex(status); serial_puts("\r\n"); }
        REQUIRE(status == 0 && proc_collect(NULL) == -NX_ECHILD);
        REQUIRE(files_live() == 0 && tty_open_count() == 0 && page_free_count() == before);
        REQUIRE(files_mount(NULL) == 0 && v7fs_unmount(&fs) == 0);
    }
    if(test==BOOT_TEST_PACKET)serial_puts("NEXIX_PACKET_CLEANUP_OK\r\n");
    REQUIRE(sleeps == (test == BOOT_TEST_TTY ? 5U : 2U));
    serial_puts("NEXIX_TTY_OK\r\nNEXIX_TTY_CLEANUP_OK\r\n");
    if (test == BOOT_TEST_TTY) serial_puts("NEXIX_TTY_UART_OK\r\n");
    if (test == BOOT_TEST_TTY_DEMO) serial_puts("NEXIX_TTY_DEMO_OK\r\n");
}
