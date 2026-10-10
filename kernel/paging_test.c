#include "paging.h"
#include "interrupts.h"
#include "serial.h"

#define REQUIRE(condition) do { if (!(condition)) panic("paging self-test: " #condition); } while (0)
extern char kernel_text_start[], kernel_rodata_start[];

static void check_mapping(const struct address_space *space, uintptr_t address,
                          uintptr_t expected, unsigned expected_flags)
{
    uintptr_t physical;
    unsigned flags;
    REQUIRE(vm_translate(space, address, &physical, &flags));
    REQUIRE(physical == expected && flags == expected_flags);
}

static void exhaust_test(void)
{
    uint32_t before = page_free_count();
    uintptr_t data = page_alloc();
    REQUIRE(data);
    uintptr_t head = 0, page;
    while ((page = page_alloc())) {
        *(uintptr_t *)page = head;
        head = page;
    }
    struct address_space space = { 0 };
    REQUIRE(!vm_create(&space) && !space.root && !page_free_count());
    /* One free page: creation must roll back its root when PDPT allocation fails. */
    REQUIRE(head);
    page = head; head = *(uintptr_t *)head; REQUIRE(page_free(page));
    REQUIRE(!vm_create(&space) && !space.root && page_free_count() == 1);
    REQUIRE(head);
    page = head; head = *(uintptr_t *)head; REQUIRE(page_free(page));
    REQUIRE(vm_create(&space) && !page_free_count());
    /* One free page: mapping allocates PD, fails on PT, and rolls back the PD. */
    REQUIRE(head);
    page = head; head = *(uintptr_t *)head; REQUIRE(page_free(page));
    REQUIRE(!vm_map(&space, USER_BASE, data, VM_WRITE));
    REQUIRE(page_free_count() == 1 && !vm_translate(&space, USER_BASE, NULL, NULL));
    REQUIRE(vm_destroy(&space) && page_free_count() == 3);
    while (head) {
        uintptr_t next = *(uintptr_t *)head;
        REQUIRE(page_free(head));
        head = next;
    }
    REQUIRE(page_free(data) && page_free_count() == before);
    serial_puts("NEXIX_PAGING_OOM_OK\r\n");
}

void paging_selftest(bool exhaustion)
{
    const struct address_space *kernel = paging_kernel_space();
    uintptr_t text = (uintptr_t)kernel_text_start;
    uintptr_t rodata = (uintptr_t)kernel_rodata_start;
    check_mapping(kernel, text, text, VM_EXEC);
    check_mapping(kernel, rodata, rodata, 0);
    REQUIRE(!vm_translate(kernel, 0, NULL, NULL));
    uint32_t before = page_free_count();
    struct address_space a = { 0 }, b = { 0 };
    REQUIRE(vm_create(&a) && vm_create(&b));
    REQUIRE(!vm_create(&a));
    uintptr_t first = page_alloc(), second = page_alloc();
    REQUIRE(first && second);
    *(uint64_t *)first = 0x11223344;
    *(uint64_t *)second = 0xaabbccdd;
    check_mapping(kernel, first, first, VM_WRITE);
    const uintptr_t address = USER_BASE;
    REQUIRE(vm_map(&a, address, first, VM_WRITE));
    REQUIRE(vm_map(&b, address, second, VM_WRITE));
    REQUIRE(!vm_map(&a, address, second, VM_WRITE));
    REQUIRE(!vm_map(&a, address + 1, first, VM_WRITE));
    REQUIRE(!vm_map(&a, 0x100000, first, VM_WRITE));
    REQUIRE(!vm_map(&a, USER_LIMIT, first, VM_WRITE));
    REQUIRE(!vm_map(&a, 0xffff800000000000ULL, first, VM_WRITE));
    REQUIRE(!vm_map(&a, address + PAGE_SIZE, first + 1, VM_WRITE));
    REQUIRE(!vm_map(&a, address + PAGE_SIZE, text, VM_EXEC));
    REQUIRE(!vm_map(&a, address + PAGE_SIZE, first, VM_WRITE | VM_EXEC));
    check_mapping(&a, address + 123, first + 123, VM_USER | VM_WRITE);
    check_mapping(&b, text, text, VM_EXEC); /* Kernel mapping remains supervisor-only. */
    REQUIRE(!vm_translate(&a, USER_LIMIT, NULL, NULL));
    REQUIRE(!vm_translate(&a, address + PAGE_SIZE, NULL, NULL));
    vm_activate(&a);
    REQUIRE(*(volatile uint64_t *)address == 0x11223344);
    *(volatile uint64_t *)address = 0x55667788;
    REQUIRE(*(volatile uint64_t *)first == 0x55667788);
    REQUIRE(!vm_destroy(&a));
    vm_activate(&b);
    REQUIRE(*(volatile uint64_t *)address == 0xaabbccdd);
    REQUIRE(vm_protect(&b, address, 0));
    check_mapping(&b, address, second, VM_USER);
    REQUIRE(!vm_protect(&b, address, VM_WRITE | VM_EXEC));
    REQUIRE(vm_protect(&b, address, VM_WRITE));
    *(volatile uint64_t *)address = 0x99;
    REQUIRE(*(volatile uint64_t *)second == 0x99);
    REQUIRE(vm_unmap(&b, address));
    REQUIRE(!vm_unmap(&b, address) && !vm_translate(&b, address, NULL, NULL));
    REQUIRE(vm_map(&b, address, first, VM_WRITE));
    REQUIRE(*(volatile uint64_t *)address == 0x55667788); /* Stale TLB must not survive. */
    REQUIRE(vm_unmap(&b, address));
    /* Exercise new PDPT and PML4 branches and empty-table reclamation. */
    const uintptr_t far[] = { 1UL << 32, 1UL << 39, USER_LIMIT - PAGE_SIZE };
    for (unsigned i = 0; i < sizeof(far) / sizeof(far[0]); ++i) {
        REQUIRE(vm_map(&a, far[i], second, 0));
        check_mapping(&a, far[i], second, VM_USER);
        REQUIRE(vm_unmap(&a, far[i]));
    }
    /* Execute a tiny function only after changing its mapping to executable. */
    uint8_t *code = (uint8_t *)second;
    code[0] = 0xb8; code[1] = 42; code[2] = 0; code[3] = 0; code[4] = 0; code[5] = 0xc3;
    REQUIRE(vm_map(&b, address, second, VM_EXEC));
    REQUIRE(((unsigned (*)(void))address)() == 42);
    REQUIRE(vm_unmap(&b, address));
    vm_activate(kernel);
    REQUIRE(vm_destroy(&a) && vm_destroy(&b));
    REQUIRE(!vm_destroy(&a));
    REQUIRE(page_is_allocated(first) && page_is_allocated(second));
    REQUIRE(page_free(first) && page_free(second));
    REQUIRE(page_free_count() == before);
    serial_puts("NEXIX_PAGING_OK\r\n");
    if (exhaustion)
        exhaust_test();
}
