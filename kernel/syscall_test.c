#include "console.h"
#include "syscall.h"
#include "process.h"
#include "uaccess.h"
#include "serial.h"
#include <nexix/syscall.h>
#include <nexix/errno.h>
#define REQUIRE(c) do { if (!(c)) panic("syscall self-test: " #c); } while (0)
extern const uint8_t syscall_blob_start[], syscall_blob_end[];
static uint8_t code[PAGE_SIZE];
static bool active, abi_checked, copy_checked, wait_blocked;
static v7_pid_t root_pid;

bool syscall_test_gate(struct interrupt_frame *frame)
{
    if (!active || !proc_current()) return false;
    struct proc *p = proc_current();
    struct user *u = p->p_user;
    if (frame->rax == 0x200) {
        REQUIRE(p->p_pid == root_pid && !abi_checked);
        REQUIRE(u->u_ap == u->u_arg && u->u_ar0 == frame);
        REQUIRE(u->u_error == 0 && u->u_r.r_val1 == root_pid);
        const uint64_t expected[] = {UINT64_C(0x1111222233334444), UINT64_C(0x2222333344445555),
            UINT64_C(0x3333444455556666), UINT64_C(0x4444555566667777),
            UINT64_C(0x5555666677778888), UINT64_C(0x6666777788889999)};
        for (unsigned i = 0; i < 6; ++i) REQUIRE(u->u_arg[i] == expected[i]);
        REQUIRE(frame->rflags & 0x400);
        abi_checked = true;
        frame->rax = 0;
        console_puts("NEXIX_SYSCALL_ABI_OK\r\n");
        return true;
    }
    if (frame->rax == 0x201) {
        REQUIRE(p->p_pid == root_pid && abi_checked && !copy_checked);
        char buffer[16];
        size_t copied;
        REQUIRE(copyinstr(&p->p_vm, frame->rdi, buffer, sizeof(buffer), &copied) == 0);
        REQUIRE(copied == 6);
        const char expected[] = "abcde";
        for (unsigned i = 0; i < 6; ++i) REQUIRE(buffer[i] == expected[i]);
        REQUIRE(copyin(&p->p_vm, frame->rdi, buffer, 6) == 0);
        for (unsigned i = 0; i < 6; ++i) REQUIRE(buffer[i] == expected[i]);
        REQUIRE(copyinstr(&p->p_vm, frame->rdi, buffer, 5, &copied) == -NX_E2BIG && copied == 5);
        REQUIRE(copyin(&p->p_vm, 0, buffer, 1) == -NX_EFAULT);
        uint64_t value = UINT64_C(0xabcdef0123456789);
        REQUIRE(copyout(&p->p_vm, frame->rsi, &value, sizeof(value)) == 0);
        copy_checked = true;
        frame->rax = 0;
        console_puts("NEXIX_UACCESS_OK\r\n");
        return true;
    }
    if (frame->rax == 0x202) {
        REQUIRE(p->p_pid != root_pid && p->p_ppid == root_pid);
        const struct proc *table = proc_table();
        frame->rax = 0;
        for (unsigned i = 1; i < V7_NPROC; ++i)
            if (table[i].p_pid == root_pid && table[i].p_stat == V7_SSLEEP) {
                REQUIRE(table[i].p_wchan == &table[i]);
                REQUIRE(table[i].p_user->u_arg[0] == (1UL << 39) + 129);
                wait_blocked = true;
                frame->rax = 1;
            }
        return true;
    }
    return false;
}
void syscall_selftest(void)
{
    uint32_t before = page_free_count();
    size_t length = (uintptr_t)syscall_blob_end - (uintptr_t)syscall_blob_start;
    REQUIRE(length <= PAGE_SIZE);
    for (size_t i = 0; i < length; ++i) code[i] = syscall_blob_start[i];
    struct proc *p = proc_alloc();
    REQUIRE(p);
    REQUIRE(proc_map_page(p, USER_BASE, VM_EXEC, code));
    REQUIRE(proc_map_page(p, USER_BASE + 2 * PAGE_SIZE, VM_WRITE, NULL));
    REQUIRE(proc_map_page(p, USER_BASE + 4 * PAGE_SIZE, VM_WRITE, NULL));
    REQUIRE(proc_map_page(p, USER_BASE + 5 * PAGE_SIZE, VM_WRITE, NULL));
    REQUIRE(proc_map_page(p, USER_BASE + 6 * PAGE_SIZE, 0, NULL));
    REQUIRE(proc_map_page(p, 1UL << 39, VM_WRITE, NULL));
    p->p_uid = p->p_user->u_uid = 321;
    p->p_user->u_ruid = 123;
    p->p_user->u_gid = 456;
    p->p_user->u_rgid = 654;
    p->p_user->u_cmask = 022;
    REQUIRE(proc_start_user(p, USER_BASE, USER_BASE + 3 * PAGE_SIZE));
    root_pid = p->p_pid;
    active = true;
    proc_run();
    active = false;
    int32_t status;
    REQUIRE(abi_checked && copy_checked && wait_blocked);
    REQUIRE(proc_collect(&status) == root_pid && status == 0);
    REQUIRE(proc_collect(NULL) == -NX_ECHILD);
    REQUIRE(page_free_count() == before);
    for (unsigned i = 1; i < V7_NPROC; ++i) REQUIRE(proc_table()[i].p_stat == V7_UNUSED);
    console_puts("NEXIX_SYSCALL_ERRORS_OK\r\n");
    console_puts("NEXIX_SYSCALL_FORK_OK\r\n");
    console_puts("NEXIX_SYSCALL_OK\r\n");
}
