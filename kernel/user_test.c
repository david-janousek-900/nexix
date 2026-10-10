#include "user_test.h"
#include "segments.h"
#include "paging.h"
#include "serial.h"

#define USER_CODE USER_BASE
#define USER_STACK (USER_BASE + 2 * PAGE_SIZE)
#define REQUIRE(condition) do { if (!(condition)) panic("user-mode self-test: " #condition); } while (0)
extern const uint8_t user_blob_start[], user_blob_end[], user_blob_success[];
extern const uint8_t user_blob_kernel[], user_blob_ro[], user_blob_nx[];
extern const uint8_t user_blob_priv[], user_blob_io[], user_blob_int[], user_blob_bad_stack[];
extern char kernel_text_start[];
static struct {
    bool active, checkpoint, completed;
    enum boot_test test;
    uintptr_t stack;
    uint64_t timer_interrupts, gates;
} running;

void user_mode_interrupt(struct interrupt_frame *frame)
{
    REQUIRE(running.active && frame->cs == USER_CS && frame->ss == USER_DS);
    REQUIRE(interrupt_stack_valid(frame));
    REQUIRE((frame->rflags & 0x3202) == 0x202); /* IF=1, IOPL=0, reserved bit set. */
    if (running.test == BOOT_TEST_USER_BAD_STACK)
        REQUIRE(frame->rsp == running.stack);
    else
        REQUIRE(frame->rsp >= USER_STACK && frame->rsp <= USER_STACK + PAGE_SIZE - 16);
    if (frame->vector == 32)
        ++running.timer_interrupts;
}

static void check_registers(const struct interrupt_frame *f)
{
    REQUIRE(f->rbx == 0x1111 && f->rcx == 0x2222 && f->rdx == 0x3333);
    REQUIRE(f->rbp == 0x4444 && f->rsi == 0x5555 && f->rdi == 0x6666);
    REQUIRE(f->r8 == 0x8888 && f->r9 == 0x9999 && f->r10 == 0xaaaa && f->r11 == 0xbbbb);
    REQUIRE(f->r12 == 0xcccc && f->r13 == 0xdddd && f->r14 == 0xeeee && f->r15 == 0xffff);
}

bool user_mode_test_gate(struct interrupt_frame *frame)
{
    if (!running.active) return false;
    REQUIRE(frame->cs == USER_CS && frame->ss == USER_DS);
    ++running.gates;
    if (running.test == BOOT_TEST_USER_BAD_STACK && frame->rax == 3) {
        running.completed = true;
        arch_user_test_return(0);
    }
    REQUIRE(running.test == BOOT_NORMAL);
    check_registers(frame);
    switch (frame->rax) {
    case 0:
        REQUIRE(!running.checkpoint && (frame->rflags & 0x400));
        running.checkpoint = true;
        frame->rax = 0x1234;
        return true;
    case 1:
        REQUIRE(running.checkpoint);
        frame->rax = running.timer_interrupts;
        return true;
    case 2:
        REQUIRE(running.checkpoint && running.timer_interrupts >= 10);
        running.completed = true;
        arch_user_test_return(0);
    default:
        panic("user test reached failure trap");
    }
}

static void run_context(enum boot_test test, unsigned context)
{
    uint32_t before = page_free_count();
    struct address_space space = { 0 };
    uintptr_t code = page_alloc(), stack = page_alloc();
    REQUIRE(code && stack && vm_create(&space));
    for (unsigned i = 0; i < PAGE_SIZE; ++i) {
        ((uint8_t *)code)[i] = 0;
        ((uint8_t *)stack)[i] = 0;
    }
    size_t length = (uintptr_t)user_blob_end - (uintptr_t)user_blob_start;
    REQUIRE(length <= PAGE_SIZE);
    for (size_t i = 0; i < length; ++i)
        ((uint8_t *)code)[i] = user_blob_start[i];
    ((uint8_t *)stack)[0] = 0xc3;
    REQUIRE(vm_map(&space, USER_CODE, code, VM_EXEC));
    REQUIRE(vm_map(&space, USER_STACK, stack, VM_WRITE));
    REQUIRE(!vm_translate(&space, USER_STACK - PAGE_SIZE, NULL, NULL));
    uintptr_t user_sp = USER_STACK + PAGE_SIZE - 16;
    const uint8_t *entry = user_blob_success;
    uintptr_t argument = 0;
    switch (test) {
    case BOOT_TEST_USER_KERNEL: entry = user_blob_kernel; argument = (uintptr_t)kernel_text_start; break;
    case BOOT_TEST_USER_RO: entry = user_blob_ro; argument = USER_CODE; break;
    case BOOT_TEST_USER_NX: entry = user_blob_nx; argument = USER_STACK; break;
    case BOOT_TEST_USER_PRIV: entry = user_blob_priv; break;
    case BOOT_TEST_USER_IO: entry = user_blob_io; break;
    case BOOT_TEST_USER_INT: entry = user_blob_int; break;
    case BOOT_TEST_USER_BAD_STACK:
        entry = user_blob_bad_stack;
        user_sp = USER_STACK + 2 * PAGE_SIZE;
        break;
    case BOOT_TEST_USER_DF: entry = user_blob_bad_stack; break;
    default: REQUIRE(test == BOOT_NORMAL); break;
    }
    running.active = true;
    running.checkpoint = running.completed = false;
    running.test = test;
    running.stack = user_sp;
    running.timer_interrupts = running.gates = 0;
    kernel_stack_set(entry_stack_bottom(context), entry_stack_top(context));
    if (test == BOOT_TEST_USER_DF)
        test_invalid_entry_stack();
    if (test == BOOT_TEST_USER_KERNEL || test == BOOT_TEST_USER_RO || test == BOOT_TEST_USER_NX) {
        serial_puts("Nexix: expected cr2="); serial_hex(argument); serial_puts("\r\n");
    }
    serial_puts("Nexix: entering user mode; kernel stack=");
    serial_hex(entry_stack_top(context)); serial_puts("\r\n");
    vm_activate(&space);
    uintptr_t user_entry = USER_CODE + ((uintptr_t)entry - (uintptr_t)user_blob_start);
    REQUIRE(arch_user_test_enter(user_entry, user_sp, argument) == 0);
    REQUIRE(running.completed && running.gates);
    running.active = false;
    vm_activate(paging_kernel_space());
    REQUIRE(vm_destroy(&space) && page_free(code) && page_free(stack));
    REQUIRE(page_free_count() == before);
}

void user_mode_selftest(enum boot_test test)
{
    if (test >= BOOT_TEST_USER_KERNEL && test <= BOOT_TEST_USER_DF) {
        run_context(test, 0);
        if (test != BOOT_TEST_USER_BAD_STACK)
            panic("expected user-mode fault did not occur");
        serial_puts("NEXIX_USER_BAD_STACK_OK\r\n");
    }
    run_context(BOOT_NORMAL, 0);
    run_context(BOOT_NORMAL, 1);
    serial_puts("NEXIX_USER_MODE_OK\r\n");
    serial_puts("NEXIX_USER_STACK_OK\r\n");
    serial_puts("NEXIX_USER_TIMER_OK\r\n");
    serial_puts("NEXIX_USER_RETURN_OK\r\n");
}
