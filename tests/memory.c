#include "boot.h"
#include "memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static _Alignas(8) unsigned char buffer[1024];
static uint32_t position;

static void begin(void)
{
    memset(buffer, 0, sizeof(buffer));
    position = 8;
}

static void *append(uint32_t type, uint32_t size)
{
    struct boot_tag *tag = (struct boot_tag *)(buffer + position);
    assert(position + size <= sizeof(buffer));
    *tag = (struct boot_tag) { type, size };
    position += (size + 7) & ~7U;
    return tag;
}

static struct boot_mmap *map_add(const struct boot_mmap_entry *entries, unsigned n,
                                 unsigned stride)
{
    struct boot_mmap *map = append(6, 16 + n * stride);
    map->entry_size = stride;
    for (unsigned i = 0; i < n; ++i)
        memcpy((unsigned char *)map + 16 + i * stride, &entries[i], sizeof(entries[i]));
    return map;
}

static void finish(void)
{
    append(0, 8);
    memcpy(buffer, &position, sizeof(position));
}

static bool parse(struct boot_info *info)
{
    return boot_parse((uintptr_t)buffer, sizeof(buffer), info);
}

static void parser_tests(void)
{
    struct boot_info info;
    struct boot_mmap_entry entry = { .address = 0, .length = 16 * PAGE_SIZE, .type = 1 };
    begin(); map_add(&entry, 1, 24); finish();
    assert(parse(&info));
    assert(!boot_parse((uintptr_t)buffer + 1, sizeof(buffer) - 1, &info));
    assert(!boot_parse((uintptr_t)buffer, position - 1, &info));
    assert(!boot_parse((uintptr_t)buffer, 8, &info));
    uint32_t saved = position;
    *(uint32_t *)buffer = 8;
    assert(!parse(&info));
    *(uint32_t *)buffer = saved;
    ((uint32_t *)buffer)[1] = 1;
    assert(!parse(&info));

    begin(); finish(); assert(!parse(&info)); /* Missing map. */
    begin(); map_add(&entry, 1, 24); map_add(&entry, 1, 24); finish();
    assert(!parse(&info));
    begin(); struct boot_mmap *map = map_add(&entry, 1, 24); finish();
    map->entry_size = 0; assert(!parse(&info));
    map->entry_size = 23; assert(!parse(&info));
    map->entry_size = 25; assert(!parse(&info));
    map->entry_size = 24; map->entry_version = 1; assert(!parse(&info));
    map->entry_version = 0; map->tag.size = 15; assert(!parse(&info));
    map->tag.size = 0; assert(!parse(&info));
    map->tag.size = 4096; assert(!parse(&info));

    entry.address = UINT64_MAX - 10; entry.length = 20;
    begin(); map_add(&entry, 1, 24); finish(); assert(!parse(&info));
    entry.address = 0; entry.length = 16 * PAGE_SIZE;
    begin(); map_add(&entry, 1, 32); /* Accept future extended entries. */
    struct boot_tag *cmd = append(1, 20);
    memcpy(cmd + 1, "test=memory", 12); finish();
    assert(parse(&info) && info.test == BOOT_TEST_MEMORY);
    memset(cmd + 1, 'x', 12); assert(!parse(&info));
    begin(); map_add(&entry, 1, 24); cmd = append(1, 22);
    memcpy(cmd + 1, "test=tty-demo", 14); finish();
    assert(parse(&info) && info.test == BOOT_TEST_TTY_DEMO);
    memset(cmd + 1, 0, 14); memcpy(cmd + 1, "test=selftest", 14);
    assert(parse(&info) && info.test == BOOT_TEST_SELFTEST);
    memset(cmd + 1, 0, 14); memcpy(cmd + 1, "test=tty", 9);
    assert(parse(&info) && info.test == BOOT_TEST_TTY);

    begin(); map_add(&entry, 1, 24);
    struct boot_module *module = append(3, 17);
    module->start = 100; module->end = 99; finish(); assert(!parse(&info));
    module->end = 200; assert(parse(&info));
    *((char *)(module + 1)) = 'x'; assert(!parse(&info));

    begin(); map_add(&entry, 1, 24); append(77, 8); finish();
    assert(parse(&info)); /* Unknown tags may be skipped. */
    ((struct boot_tag *)(buffer + position - 8))->type = 77;
    assert(!parse(&info)); /* Missing terminator. */
    puts("PASS: bounded boot tag parsing and malformed memory maps");
}

static void allocation_tests(void)
{
    struct boot_info info;
    const struct boot_mmap_entry entries[] = {
        { .address = 5 * PAGE_SIZE + 1, .length = PAGE_SIZE, .type = 2 },
        { .address = 0, .length = 16 * PAGE_SIZE, .type = 1 },
        { .address = 0, .length = 16 * PAGE_SIZE, .type = 1 },
    };
    begin(); map_add(entries, 3, 24);
    struct boot_module *module = append(3, 17);
    module->start = 8 * PAGE_SIZE + 1; module->end = 9 * PAGE_SIZE + 1;
    finish(); assert(parse(&info));
    page_allocator_init(&info, 3 * PAGE_SIZE + 1);
    assert(page_free_count() == 8);
    uintptr_t pages[8];
    for (unsigned i = 0; i < 8; ++i) {
        pages[i] = page_alloc();
        assert(pages[i] && !(pages[i] % PAGE_SIZE));
        assert(page_is_allocated(pages[i]));
        assert(pages[i] >= 4 * PAGE_SIZE && pages[i] < 16 * PAGE_SIZE);
        assert(pages[i] != 5 * PAGE_SIZE && pages[i] != 6 * PAGE_SIZE);
        assert(pages[i] != 8 * PAGE_SIZE && pages[i] != 9 * PAGE_SIZE);
        for (unsigned j = 0; j < i; ++j) assert(pages[i] != pages[j]);
    }
    assert(!page_alloc() && !page_alloc() && !page_free_count());
    assert(!page_free(0) && !page_free(2 * PAGE_SIZE));
    assert(!page_free(5 * PAGE_SIZE) && !page_free(8 * PAGE_SIZE));
    assert(!page_free(pages[0] + 1) && !page_free(PHYSICAL_LIMIT));
    assert(!page_free(UINTPTR_MAX));
    assert(!page_is_allocated(0) && !page_is_allocated(PHYSICAL_LIMIT));
    assert(!page_is_allocated(pages[0] + 1));
    assert(page_free(pages[3])); assert(!page_free(pages[3]));
    assert(!page_is_allocated(pages[3]));
    assert(page_free_count()==1 && page_is_eligible(pages[3]));
    assert(page_claim(pages[3]) && !page_claim(pages[3]) && !page_free_count());
    assert(page_free(pages[3]) && page_alloc()==pages[3]);
    assert(!page_is_eligible(0) && !page_claim(0) && !page_claim(2*PAGE_SIZE));
    assert(!page_claim(pages[3]+1) && !page_claim(PHYSICAL_LIMIT));
    for (unsigned i = 0; i < 8; ++i) assert(page_free(pages[i]));
    assert(page_free_count() == 8);
    puts("PASS: alignment, reservations, overlap priority, exhaustion, and reuse");

    const struct boot_mmap_entry clipped[] = {
        { .address = PHYSICAL_LIMIT - 2 * PAGE_SIZE, .length = 4 * PAGE_SIZE, .type = 1 },
        { .address = PAGE_SIZE + 1, .length = 2 * PAGE_SIZE - 1, .type = 1 },
        { .address = PHYSICAL_LIMIT + PAGE_SIZE, .length = PAGE_SIZE, .type = 1 },
    };
    begin(); map_add(clipped, 3, 24); finish(); assert(parse(&info));
    page_allocator_init(&info, 0);
    assert(page_free_count() == 3);
    assert(page_alloc() == 2 * PAGE_SIZE);
    assert(page_alloc() == PHYSICAL_LIMIT - 2 * PAGE_SIZE);
    assert(page_alloc() == PHYSICAL_LIMIT - PAGE_SIZE);
    assert(!page_alloc());
    begin(); map_add(NULL, 0, 24); finish(); assert(parse(&info));
    page_allocator_init(&info, 0);
    assert(!page_alloc() && !page_free_count());
    puts("PASS: partial pages, mapping limit, extended entries, and empty map");
}

int main(void)
{
    parser_tests();
    allocation_tests();
    return 0;
}
