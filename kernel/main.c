#include "console.h"
#include "local_console.h"
#include "process.h"
#include "fpu.h"
#include "clock.h"
#include "signals.h"
#include "net/kernel.h"
#include "userland.h"
#include "exec.h"
#include "libc_test.h"
#include "v7/fs.h"
#include "syscall.h"
#include "files.h"
#include "tty.h"
#include "interrupts.h"
#include "boot.h"
#include "memory.h"
#include "paging.h"
#include "serial.h"
#include "segments.h"
#include "user_test.h"
#include <stdint.h>

static uint64_t boot_checksum(const struct boot_info *info)
{
    const uint8_t *bytes = (const uint8_t *)info->address;
    uint64_t value = 0;
    for (uint32_t i = 0; i < info->size; ++i)
        value = value * 33 ^ bytes[i];
    return value;
}

static void module_selftest(const struct boot_info *info)
{
    unsigned found = 0;
    for (const struct boot_tag *tag = boot_first(info); tag->type; tag = boot_next(tag)) {
        if (tag->type != 3)
            continue;
        const struct boot_module *module = (const struct boot_module *)tag;
        if (module->end - module->start != 8193 || module->end > PHYSICAL_LIMIT)
            panic("invalid memory test module");
        const volatile uint8_t *bytes = (const volatile uint8_t *)(uintptr_t)module->start;
        for (unsigned i = 0; i < 8193; ++i)
            if (bytes[i] != 0xa5)
                panic("reserved boot module was overwritten");
        ++found;
    }
    if (found != 1)
        panic("missing memory test module");
}

void kernel_main(uint32_t magic, uintptr_t boot_info)
{
    console_init(BOOT_CONSOLE_VGA);
    if (magic != 0x36d76289 || !boot_info || (boot_info & 7))
        panic("invalid Multiboot2 handoff");
    segments_init();
    interrupts_init();
    struct boot_info info;
    if (boot_info >= PHYSICAL_LIMIT ||
        !boot_parse(boot_info, PHYSICAL_LIMIT - boot_info, &info))
        panic("invalid Multiboot2 information or memory map");
    console_select(info.console);
    console_puts("x86_64 kernel entered\r\n");
    enum boot_test test = info.test;
    console_puts("Multiboot2 handoff validated\r\n");
    console_puts("IDT installed; PIC remapped\r\n");
    if (test == BOOT_TEST_UD) {
        console_puts("testing invalid opcode\r\n");
        __asm__ volatile ("ud2");
    }
    if (test == BOOT_TEST_PF) {
        console_puts("testing unmapped memory\r\n");
        (void)*(volatile uint64_t *)0x40000000UL;
    }
    extern char kernel_end[];
    page_allocator_init(&info, (uintptr_t)kernel_end);
    console_puts("free physical pages=");
    console_hex(page_free_count());
    console_puts("\r\n");
    uint32_t before = page_free_count();
    uintptr_t first = page_alloc(), second = page_alloc();
    if (!first || !second || first == second || page_free_count() != before - 2)
        panic("page allocation self-test failed");
    volatile uint64_t *a = (volatile uint64_t *)first;
    volatile uint64_t *b = (volatile uint64_t *)second;
    a[0] = 0x1122334455667788ULL;
    b[PAGE_SIZE / sizeof(uint64_t) - 1] = 0x8877665544332211ULL;
    if (a[0] != 0x1122334455667788ULL ||
        b[PAGE_SIZE / sizeof(uint64_t) - 1] != 0x8877665544332211ULL ||
        !page_free(first) || page_free(first) || !page_free(second) ||
        page_free_count() != before || page_free(0))
        panic("page freeing self-test failed");
    console_puts("MEMORY OK\r\n");
    if (test == BOOT_TEST_MEMORY) {
        uint64_t checksum = boot_checksum(&info);
        module_selftest(&info);
        /* Intrusive list in allocated pages: no heap required for exhaustion. */
        uintptr_t head = 0, page;
        uint32_t count = 0;
        while ((page = page_alloc())) {
            /* Touch every byte, proving that reservations survive exhaustion. */
            volatile uint64_t *words = (volatile uint64_t *)page;
            for (unsigned i = 0; i < PAGE_SIZE / sizeof(uint64_t); ++i)
                words[i] = 0x123456789abcdef0ULL;
            *(uintptr_t *)page = head;
            head = page;
            ++count;
        }
        if (count != before || page_free_count() || page_alloc())
            panic("page exhaustion self-test failed");
        while (head) {
            uintptr_t next = *(uintptr_t *)head;
            if (!page_free(head))
                panic("page recovery self-test failed");
            head = next;
        }
        if (page_free_count() != before)
            panic("page count recovery failed");
        if (boot_checksum(&info) != checksum)
            panic("reserved boot information was overwritten");
        module_selftest(&info);
        console_puts("RESERVATIONS OK\r\n");
        console_puts("MEMORY EXHAUSTION OK\r\n");
    }
    paging_init();
    entry_stacks_guard();
    paging_selftest(test == BOOT_TEST_PAGING_OOM);
    if (test == BOOT_TEST_WP || test == BOOT_TEST_NX ||
        test == BOOT_TEST_UNMAP || test == BOOT_TEST_NULL || test == BOOT_TEST_PROTECT) {
        extern char kernel_text_start[];
        uintptr_t address = (uintptr_t)kernel_text_start;
        if (test == BOOT_TEST_NULL)
            address = 0;
        if (test == BOOT_TEST_NX || test == BOOT_TEST_UNMAP || test == BOOT_TEST_PROTECT) {
            struct address_space space = { 0 };
            uintptr_t page = page_alloc();
            address = USER_BASE;
            if (!page || !vm_create(&space) ||
                !vm_map(&space, address, page, test == BOOT_TEST_NX ? VM_EXEC : VM_WRITE))
                panic("cannot prepare paging fault test");
            *(uint8_t *)page = 0xc3;
            vm_activate(&space);
            (void)*(volatile uint8_t *)address; /* Populate the TLB before mutation. */
            if (test == BOOT_TEST_NX) {
                ((void (*)(void))address)(); /* Cache an executable translation. */
                if (!vm_protect(&space, address, VM_WRITE))
                    panic("cannot set NX paging test permission");
            }
            if (test == BOOT_TEST_PROTECT) {
                *(volatile uint8_t *)address = 0xc3; /* Cache a writable translation. */
                if (!vm_protect(&space, address, 0))
                    panic("cannot set read-only paging test permission");
            }
            if (test == BOOT_TEST_UNMAP && !vm_unmap(&space, address))
                panic("cannot unmap paging fault test");
        }
        console_puts("expected cr2="); console_hex(address); console_puts("\r\n");
        if (test == BOOT_TEST_WP || test == BOOT_TEST_PROTECT)
            *(volatile uint8_t *)address = 0;
        else if (test == BOOT_TEST_NX)
            ((void (*)(void))address)();
        else
            __asm__ volatile ("mov (%0), %%rax" : : "r"(address) : "rax", "memory");
        panic("expected paging fault did not occur");
    }
    if (clock_init()) {
        console_puts("RTC UTC epoch="); console_hex(clock_now()); console_puts("\r\n");
        console_puts("RTC OK\r\n");
    } else {
        console_puts("RTC unavailable or invalid; clock starts at UNIX epoch\r\n");
    }
    fpu_init();
    timer_init();
    if (info.console != BOOT_CONSOLE_VGA) tty_init();
    if (info.console == BOOT_CONSOLE_MULTI) serial_secondary_init();
    if (info.console != BOOT_CONSOLE_SERIAL) local_console_init();
    tty_console_line = info.console == BOOT_CONSOLE_VGA ? 2 : 0;
    /* Check and sleep atomically against IRQ delivery. */
    for (;;) {
        __asm__ volatile ("cli" : : : "memory");
        if (timer_ticks() >= 10)
            break;
        __asm__ volatile ("sti; hlt" : : : "memory");
    }
    console_puts("10 timer interrupts handled\r\n");
    console_puts("TIMER OK\r\n");
    if (test == BOOT_NORMAL) {
        proc_init();
        rootfs_start();
    }
    user_mode_selftest(test);
    process_selftest();
    syscall_selftest();
    exec_selftest();
    libc_selftest();
    filesystem_selftest();
    files_selftest();
    tty_selftest(test);
    signals_selftest();
    net_pool_selftest();
    v7_userland_selftest();
    rootfs_selftest();
    root_exec_selftest();
    console_puts("BOOT OK\r\n");
    for (;;)
        __asm__ volatile ("sti; hlt" : : : "memory");
}
