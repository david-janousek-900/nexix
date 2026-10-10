#include "memory.h"

#define PAGE_COUNT (PHYSICAL_LIMIT / PAGE_SIZE)
#define WORD_BITS 64
static uint64_t eligible[PAGE_COUNT / WORD_BITS];
static uint64_t allocated[PAGE_COUNT / WORD_BITS];
static uint32_t free_pages, next_page;

static uint64_t bit(uint32_t page) { return 1ULL << (page % WORD_BITS); }
static bool is_free(uint32_t page)
{
    uint32_t word = page / WORD_BITS;
    return (eligible[word] & bit(page)) && !(allocated[word] & bit(page));
}

/* Clip before rounding so oversized firmware ranges cannot wrap. */
static void range(uint64_t start, uint64_t end, bool available)
{
    if (start >= PHYSICAL_LIMIT || start >= end)
        return;
    if (end > PHYSICAL_LIMIT)
        end = PHYSICAL_LIMIT;
    if (available) {
        start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        end &= ~(PAGE_SIZE - 1);
    } else {
        start &= ~(PAGE_SIZE - 1);
        end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    for (uint32_t page = start / PAGE_SIZE; page < end / PAGE_SIZE; ++page) {
        if (available)
            eligible[page / WORD_BITS] |= bit(page);
        else
            eligible[page / WORD_BITS] &= ~bit(page);
    }
}

void page_allocator_init(const struct boot_info *info, uintptr_t reserved_end)
{
    for (uint32_t word = 0; word < PAGE_COUNT / WORD_BITS; ++word) {
        eligible[word] = 0;
        allocated[word] = 0;
    }
    const struct boot_mmap *map = info->mmap;
    /* Reserved ranges win even if firmware entries overlap or are unordered. */
    for (unsigned pass = 0; pass < 2; ++pass) {
        for (uint32_t offset = 16; offset < map->tag.size; offset += map->entry_size) {
            const struct boot_mmap_entry *entry =
                (const struct boot_mmap_entry *)((uintptr_t)map + offset);
            if ((entry->type == 1) == (pass == 0))
                range(entry->address, entry->address + entry->length, pass == 0);
        }
    }
    /* Keep page zero reserved even in synthetic maps with reserved_end == 0. */
    range(0, reserved_end > PAGE_SIZE ? reserved_end : PAGE_SIZE, false);
    range(info->address, info->address + info->size, false);
    for (const struct boot_tag *tag = boot_first(info); tag->type; tag = boot_next(tag)) {
        if (tag->type == 3) {
            const struct boot_module *module = (const struct boot_module *)tag;
            range(module->start, module->end, false);
        }
    }
    free_pages = 0;
    next_page = 0;
    for (uint32_t page = 0; page < PAGE_COUNT; ++page)
        if (is_free(page))
            ++free_pages;
}

uintptr_t page_alloc(void)
{
    if (!free_pages)
        return 0;
    for (uint32_t i = 0; i < PAGE_COUNT; ++i) {
        uint32_t page = next_page;
        next_page = (next_page + 1) % PAGE_COUNT;
        if (is_free(page)) {
            allocated[page / WORD_BITS] |= bit(page);
            --free_pages;
            return (uintptr_t)page * PAGE_SIZE;
        }
    }
    return 0;
}

bool page_free(uintptr_t address)
{
    if (address & (PAGE_SIZE - 1) || address >= PHYSICAL_LIMIT)
        return false;
    uint32_t page = address / PAGE_SIZE, word = page / WORD_BITS;
    if (!(eligible[word] & bit(page)) || !(allocated[word] & bit(page)))
        return false;
    allocated[word] &= ~bit(page);
    ++free_pages;
    /* Make recently freed pages promptly reusable. */
    next_page = page;
    return true;
}

uint32_t page_free_count(void) { return free_pages; }

bool page_is_allocated(uintptr_t address)
{
    if ((address & (PAGE_SIZE - 1)) || address >= PHYSICAL_LIMIT)
        return false;
    uint32_t page = address / PAGE_SIZE;
    return (eligible[page / WORD_BITS] & bit(page)) &&
        (allocated[page / WORD_BITS] & bit(page));
}
