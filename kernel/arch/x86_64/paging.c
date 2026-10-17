#include "paging.h"
#include "interrupts.h"
#include <nexix/phys.h>

#define PRESENT 1ULL
#define WRITE 2ULL
#define USER 4ULL
#define LARGE (1ULL << 7)
#define PHYS (1ULL << 9)
#define UNCACHED ((1ULL << 4) | (1ULL << 3))
#define NX (1ULL << 63)
#define ADDRESS 0x000ffffffffff000ULL
#define ENTRIES 512
static struct address_space kernel_space;
static uintptr_t kernel_directory;
extern char kernel_start[], kernel_text_start[], kernel_text_end[];
extern char kernel_rodata_start[], kernel_rodata_end[];

static uintptr_t active_root(void)
{
    uintptr_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static uint64_t *table(uintptr_t address) { return (uint64_t *)address; }
static unsigned index_at(uintptr_t address, unsigned level)
{
    return (address >> (12 + 9 * (level - 1))) & 511;
}

static uintptr_t new_table(void)
{
    uintptr_t address = page_alloc();
    if (address)
        for (unsigned i = 0; i < ENTRIES; ++i)
            table(address)[i] = 0;
    return address;
}

static void release_table(uintptr_t address)
{
    if (!page_free(address))
        panic("invalid page-table release");
}

static bool valid_user(uintptr_t address)
{
    return address >= USER_BASE && address < USER_LIMIT && !(address & (PAGE_SIZE - 1));
}

static bool valid_permissions(unsigned permissions)
{
    return !(permissions & ~(VM_WRITE | VM_EXEC)) &&
        permissions != (VM_WRITE | VM_EXEC);
}

static uint64_t leaf_flags(unsigned permissions)
{
    return PRESENT | USER | (permissions & VM_WRITE ? WRITE : 0) |
        (permissions & VM_EXEC ? 0 : NX);
}

static void invalidate(const struct address_space *space, uintptr_t address)
{
    if (space->root == active_root())
        __asm__ volatile ("invlpg (%0)" : : "r"(address) : "memory");
}

/* Return a leaf slot; create intermediate tables transactionally if requested. */
static uint64_t *walk(const struct address_space *space, uintptr_t address, bool create)
{
    if (!space || !space->root)
        return NULL;
    uint64_t *current = table(space->root);
    uint64_t *links[3];
    uintptr_t pages[3];
    unsigned added = 0;
    for (unsigned level = 4; level > 1; --level) {
        uint64_t *entry = &current[index_at(address, level)];
        if (!(*entry & PRESENT)) {
            if (!create)
                return NULL;
            uintptr_t page = new_table();
            if (!page) {
                while (added) {
                    --added;
                    *links[added] = 0;
                    release_table(pages[added]);
                }
                return NULL;
            }
            links[added] = entry;
            pages[added++] = page;
            *entry = page | PRESENT | WRITE | USER;
        }
        if (*entry & LARGE)
            panic("unexpected large page in managed address space");
        current = table(*entry & ADDRESS);
    }
    return &current[index_at(address, 1)];
}

const struct address_space *paging_kernel_space(void) { return &kernel_space; }

void vm_activate(const struct address_space *space)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(space->root) : "memory");
}

void paging_init(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                      : "a"(0x80000001U), "c"(0));
    if (!(d & (1U << 20)))
        panic("CPU lacks NX support");
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(0xc0000080U));
    __asm__ volatile ("wrmsr" : : "a"(low | (1U << 11)), "d"(high), "c"(0xc0000080U));
    kernel_space.root = new_table();
    uintptr_t pdpt = new_table();
    kernel_directory = new_table();
    if (!kernel_space.root || !pdpt || !kernel_directory)
        panic("insufficient memory for kernel page tables");
    table(kernel_space.root)[0] = pdpt | PRESENT | WRITE;
    table(pdpt)[0] = kernel_directory | PRESENT | WRITE;
    for (unsigned directory = 0; directory < ENTRIES; ++directory) {
        uintptr_t pt = new_table();
        if (!pt)
            panic("insufficient memory for 4 KiB identity mapping");
        table(kernel_directory)[directory] = pt | PRESENT | WRITE;
        for (unsigned i = 0; i < ENTRIES; ++i) {
            uintptr_t address = ((uintptr_t)directory * ENTRIES + i) * PAGE_SIZE;
            if (!address)
                continue; /* Null-page guard. */
            uint64_t flags = PRESENT | WRITE | NX;
            if (address >= (uintptr_t)kernel_text_start && address < (uintptr_t)kernel_text_end)
                flags = PRESENT;
            else if ((address >= (uintptr_t)kernel_start && address < (uintptr_t)kernel_text_start) ||
                     (address >= (uintptr_t)kernel_rodata_start && address < (uintptr_t)kernel_rodata_end))
                flags = PRESENT | NX;
            if(address>=NX_PHYS_VGA_BASE && address<NX_PHYS_VGA_END)flags|=UNCACHED;
            table(pt)[i] = address | flags;
        }
    }
    vm_activate(&kernel_space);
    uintptr_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0 | (1UL << 16)) : "memory");
}

bool vm_create(struct address_space *space)
{
    if (!space || space->root || !kernel_directory)
        return false;
    uintptr_t root = new_table();
    if (!root)
        return false;
    uintptr_t pdpt = new_table();
    if (!pdpt) {
        release_table(root);
        return false;
    }
    table(root)[0] = pdpt | PRESENT | WRITE | USER;
    /* Supervisor-only at this level, regardless of parent U/S bits. */
    table(pdpt)[0] = kernel_directory | PRESENT | WRITE;
    space->root = root;
    return true;
}

bool vm_map(struct address_space *space, uintptr_t address, uintptr_t physical, unsigned permissions)
{
    if (!space || space->root == kernel_space.root || !valid_user(address) ||
        !physical || physical & (PAGE_SIZE - 1) || !page_is_allocated(physical) ||
        !valid_permissions(permissions))
        return false;
    uint64_t *entry = walk(space, address, true);
    if (!entry || (*entry & PRESENT))
        return false;
    *entry = physical | leaf_flags(permissions);
    invalidate(space, address);
    return true;
}

bool vm_protect(struct address_space *space, uintptr_t address, unsigned permissions)
{
    if (!space || space->root == kernel_space.root || !valid_user(address) || !valid_permissions(permissions))
        return false;
    uint64_t *entry = walk(space, address, false);
    if (!entry || !(*entry & PRESENT))
        return false;
    if((*entry&PHYS) && permissions!=VM_WRITE)return false;
    *entry = (*entry & (ADDRESS|PHYS|UNCACHED)) | leaf_flags(permissions);
    invalidate(space, address);
    return true;
}

static bool empty_table(uintptr_t address)
{
    for (unsigned i = 0; i < ENTRIES; ++i)
        if (table(address)[i] & PRESENT)
            return false;
    return true;
}

bool vm_unmap(struct address_space *space, uintptr_t address)
{
    if (!space || !space->root || space->root == kernel_space.root || !valid_user(address))
        return false;
    uintptr_t pages[4] = { space->root };
    uint64_t *links[3];
    for (unsigned depth = 0; depth < 3; ++depth) {
        links[depth] = &table(pages[depth])[index_at(address, 4 - depth)];
        if (!(*links[depth] & PRESENT))
            return false;
        pages[depth + 1] = *links[depth] & ADDRESS;
    }
    uint64_t *entry = &table(pages[3])[index_at(address, 1)];
    if (!(*entry & PRESENT))
        return false;
    *entry = 0;
    invalidate(space, address);
    for (unsigned depth = 3; depth; --depth) {
        if (!empty_table(pages[depth]))
            break;
        *links[depth - 1] = 0;
        release_table(pages[depth]);
    }
    return true;
}

static void destroy_table(uintptr_t address, unsigned level)
{
    if (address == kernel_directory)
        return;
    if (level > 1)
        for (unsigned i = 0; i < ENTRIES; ++i)
            if (table(address)[i] & PRESENT)
                destroy_table(table(address)[i] & ADDRESS, level - 1);
    release_table(address);
}

bool vm_destroy(struct address_space *space)
{
    if (!space || !space->root || space->root == kernel_space.root || space->root == active_root())
        return false;
    destroy_table(space->root, 4);
    space->root = 0;
    return true;
}

bool vm_translate(const struct address_space *space, uintptr_t address,
                  uintptr_t *physical, unsigned *permissions)
{
    if (!space || !space->root || address >= USER_LIMIT)
        return false;
    uint64_t *current = table(space->root);
    unsigned flags = VM_WRITE | VM_EXEC | VM_USER;
    for (unsigned level = 4; level; --level) {
        uint64_t entry = current[index_at(address, level)];
        if (!(entry & PRESENT) || (level > 1 && entry & LARGE))
            return false;
        if (!(entry & WRITE)) flags &= ~VM_WRITE;
        if (!(entry & USER)) flags &= ~VM_USER;
        if (entry & NX) flags &= ~VM_EXEC;
        if (level == 1) {
            if (physical) *physical = (entry & ADDRESS) | (address & (PAGE_SIZE - 1));
            if(entry&PHYS)flags|=VM_PHYS;
            if (permissions) *permissions = flags;
            return true;
        }
        current = table(entry & ADDRESS);
    }
    return false;
}

bool paging_guard_page(uintptr_t address)
{
    extern char kernel_bss_start[], kernel_end[];
    if ((address & (PAGE_SIZE - 1)) || address < (uintptr_t)kernel_bss_start ||
        address >= (uintptr_t)kernel_end || page_is_allocated(address))
        return false;
    uint64_t *entry = walk(&kernel_space, address, false);
    if (!entry || !(*entry & PRESENT) || !(*entry & WRITE) || !(*entry & NX))
        return false;
    *entry = 0;
    /* The kernel mapping is shared by every CR3. No globals/PCIDs are used. */
    __asm__ volatile ("invlpg (%0)" : : "r"(address) : "memory");
    return true;
}

static bool visit_table(uintptr_t address, unsigned level, uintptr_t base,
                        unsigned permissions,
                        bool (*visit)(uintptr_t, uintptr_t, unsigned, void *), void *argument)
{
    if (address == kernel_directory)
        return true;
    unsigned count = level == 4 ? 256 : ENTRIES; /* Lower canonical half only. */
    for (unsigned i = 0; i < count; ++i) {
        uint64_t entry = table(address)[i];
        if (!(entry & PRESENT)) continue;
        if (level > 1 && (entry & LARGE)) panic("large page in user walk");
        uintptr_t virtual_address = base | (uintptr_t)i << (12 + 9 * (level - 1));
        unsigned flags = permissions;
        if (!(entry & WRITE)) flags &= ~VM_WRITE;
        if (!(entry & USER)) flags &= ~VM_USER;
        if (entry & NX) flags &= ~VM_EXEC;
        if (level > 1) {
            if (!visit_table(entry & ADDRESS, level - 1, virtual_address, flags, visit, argument))
                return false;
        } else if (virtual_address >= USER_BASE && (flags & VM_USER)) {
            if(entry&PHYS)flags|=VM_PHYS;
            if (!visit(virtual_address, entry & ADDRESS, flags, argument)) return false;
        }
    }
    return true;
}

bool vm_walk_user(const struct address_space *space,
                  bool (*visit)(uintptr_t, uintptr_t, unsigned, void *), void *argument)
{
    if (!space || !space->root || space->root == kernel_space.root || !visit)
        return false;
    return visit_table(space->root, 4, 0, VM_WRITE | VM_EXEC | VM_USER, visit, argument);
}

static bool physical_leaf(struct address_space *space,uintptr_t va,uintptr_t pa,bool rebind)
{
    if(!space || !space->root || space->root==kernel_space.root || !valid_user(va) ||
       !pa || pa>=PHYSICAL_LIMIT || (pa&(PAGE_SIZE-1)))return false;
    bool device=pa>=NX_PHYS_VGA_BASE && pa<NX_PHYS_VGA_END;
    if(!device && !page_is_allocated(pa))return false;
    uint64_t *entry=walk(space,va,!rebind);
    if(!entry || (rebind?((*entry&(PRESENT|PHYS))!=(PRESENT|PHYS)):(*entry&PRESENT)))return false;
    *entry=pa|leaf_flags(VM_WRITE)|PHYS|(device?UNCACHED:0);
    invalidate(space,va);return true;
}
bool vm_map_physical(struct address_space *space,uintptr_t va,uintptr_t pa)
{return physical_leaf(space,va,pa,false);}
bool vm_rebind_physical(struct address_space *space,uintptr_t va,uintptr_t pa)
{return physical_leaf(space,va,pa,true);}
