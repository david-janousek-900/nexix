#include "console.h"
#include "libc_test.h"
#include "exec.h"
#include "serial.h"
#include <nexix/errno.h>
#define REQUIRE(c) do { if (!(c)) panic("libc self-test: " #c); } while (0)
extern const uint8_t _binary_build_libc_test_elf_start[], _binary_build_libc_test_elf_end[];
static struct exec_args args;
static bool active, checked;
static v7_pid_t root_pid;
static uint64_t fp_preemptions;
bool libc_test_gate(struct interrupt_frame *frame)
{
    if (!active || frame->rax < 0x300 || frame->rax > 0x302 || !proc_current()) return false;
    struct proc *p = proc_current();
    REQUIRE(p->p_pid == root_pid);
    if (frame->rax == 0x301) { fp_preemptions = p->p_preemptions; frame->rax = 0; return true; }
    if (frame->rax == 0x302) {
        REQUIRE(p->p_preemptions > fp_preemptions);
        console_puts("FPU PREEMPT OK\r\n");
        frame->rax = 0; return true;
    }
    REQUIRE(p->p_pid == root_pid && !checked);
    const uint64_t values[] = {0x1111222233334444UL, 0x2222333344445555UL,
        0x3333444455556666UL, 0x4444555566667777UL, 0x5555666677778888UL, 0x6666777788889999UL};
    for (unsigned i = 0; i < 6; ++i) REQUIRE(p->p_user->u_arg[i] == values[i]);
    REQUIRE(p->p_user->u_error == 0 && p->p_user->u_r.r_val1 == root_pid);
    frame->rax = 0;
    checked = true;
    console_puts("LIBC ABI OK\r\n");
    return true;
}
void libc_selftest(void)
{
    const char strings[] = "/bin/libc-test\0start\0NEXIX_TEST=ok";
    for (size_t i = 0; i < sizeof(strings); ++i) args.bytes[i] = strings[i];
    args.used = sizeof(strings); args.argc = 2; args.envc = 1;
    args.argv[0] = 0; args.argv[1] = 15; args.envp[0] = 21;
    REQUIRE(args.bytes[15] == 's' && args.bytes[21] == 'N');
    uint32_t before = page_free_count();
    size_t length = (uintptr_t)_binary_build_libc_test_elf_end - (uintptr_t)_binary_build_libc_test_elf_start;
    for (unsigned pass = 0; pass < 3; ++pass) {
        if (pass == 1) args.argc = args.envc = 0;
        if (pass == 2) {
            args.argc = args.envc = EXEC_ARG_COUNT;
            for (unsigned i = 0; i < EXEC_ARG_COUNT; ++i) { args.argv[i] = 0; args.envp[i] = 21; }
        }
        struct proc *p = proc_alloc();
        REQUIRE(p);
        struct exec_image image = {0};
        REQUIRE(elf_load(_binary_build_libc_test_elf_start, length, &args, &image) == 0);
        struct exec_image empty = {.vm = p->p_vm};
        p->p_vm = image.vm; p->p_size = image.size; p->p_brk_base = p->p_brk = image.heap;
        exec_image_release(&empty);
        p->p_uid = p->p_user->u_uid = 321; p->p_user->u_ruid = 123;
        p->p_user->u_gid = 456; p->p_user->u_rgid = 654; p->p_user->u_cmask = 022;
        REQUIRE(proc_start_user(p, image.entry, image.stack));
        root_pid = p->p_pid; active = true; checked = false;
        proc_run();
        active = false;
        int32_t status;
        REQUIRE(proc_collect(&status) == root_pid);
        if (status) { console_puts("C libc test failure status="); console_hex(status); console_puts("\r\n"); }
        REQUIRE(checked == (pass == 0) && status == 0);
        REQUIRE(proc_collect(NULL) == -NX_ECHILD && page_free_count() == before);
        for (unsigned i = 1; i < V7_NPROC; ++i) REQUIRE(proc_table()[i].p_stat == V7_UNUSED);
    }
    console_puts("LIBC OK\r\n");
    console_puts("FPU USER OK\r\n");
}
