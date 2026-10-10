#include "process.h"
#include "syscall.h"
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
    serial_init();
    serial_puts("Nexix: x86_64 kernel entered\r\n");
    if (magic != 0x36d76289 || !boot_info || (boot_info & 7))
        panic("invalid Multiboot2 handoff");
    segments_init();
    interrupts_init();
    struct boot_info info;
    if (boot_info >= PHYSICAL_LIMIT ||
        !boot_parse(boot_info, PHYSICAL_LIMIT - boot_info, &info))
        panic("invalid Multiboot2 information or memory map");
    enum boot_test test = info.test;
    serial_puts("Nexix: Multiboot2 handoff validated\r\n");
    serial_puts("Nexix: IDT installed; PIC remapped\r\n");
    if (test == BOOT_TEST_UD) {
        serial_puts("Nexix: testing invalid opcode\r\n");
        __asm__ volatile ("ud2");
    }
    if (test == BOOT_TEST_PF) {
        serial_puts("Nexix: testing unmapped memory\r\n");
        (void)*(volatile uint64_t *)0x40000000UL;
    }
    extern char kernel_end[];
    page_allocator_init(&info, (uintptr_t)kernel_end);
    serial_puts("Nexix: free physical pages=");
    serial_hex(page_free_count());
    serial_puts("\r\n");
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
    serial_puts("NEXIX_MEMORY_OK\r\n");
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
        serial_puts("NEXIX_RESERVATIONS_OK\r\n");
        serial_puts("NEXIX_MEMORY_EXHAUSTION_OK\r\n");
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
        serial_puts("Nexix: expected cr2="); serial_hex(address); serial_puts("\r\n");
        if (test == BOOT_TEST_WP || test == BOOT_TEST_PROTECT)
            *(volatile uint8_t *)address = 0;
        else if (test == BOOT_TEST_NX)
            ((void (*)(void))address)();
        else
            __asm__ volatile ("mov (%0), %%rax" : : "r"(address) : "rax", "memory");
        panic("expected paging fault did not occur");
    }
    timer_init();
    /* Check and sleep atomically against IRQ delivery. */
    for (;;) {
        __asm__ volatile ("cli" : : : "memory");
        if (timer_ticks() >= 10)
            break;
        __asm__ volatile ("sti; hlt" : : : "memory");
    }
    serial_puts("Nexix: 10 timer interrupts handled\r\n");
    serial_puts("NEXIX_TIMER_OK\r\n");
    user_mode_selftest(test);
    process_selftest();
    syscall_selftest();
    serial_puts("NEXIX_BOOT_OK\r\n");
    for (;;)
        __asm__ volatile ("sti; hlt" : : : "memory");
}
