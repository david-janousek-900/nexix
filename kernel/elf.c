#include "exec.h"
#include "uaccess.h"
#include <nexix/errno.h>

struct elf_header {
    uint8_t ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct elf_program {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
};
_Static_assert(sizeof(struct elf_header) == 64, "ELF64 header");
_Static_assert(sizeof(struct elf_program) == 56, "ELF64 program header");
#define MAX_PHDR 32
#define MAX_IMAGE_BYTES (8UL * 1024 * 1024)
static void bytes_copy(void *dst, const void *src, size_t length)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    for (size_t i = 0; i < length; ++i) d[i] = s[i];
}
static bool free_leaf(uintptr_t va, uintptr_t pa, unsigned flags, void *arg)
{
    (void)va; (void)flags; (void)arg;
    if (!page_free(pa)) panic("invalid exec image frame");
    return true;
}
void exec_image_release(struct exec_image *image)
{
    if (image->vm.root && (!vm_walk_user(&image->vm, free_leaf, NULL) || !vm_destroy(&image->vm)))
        panic("cannot release exec image");
    *image = (struct exec_image){0};
}
static int map_zero(struct exec_image *image, uintptr_t va, unsigned flags)
{
    uintptr_t pa = page_alloc();
    if (!pa) return -NX_ENOMEM;
    for (size_t i = 0; i < PAGE_SIZE; ++i) ((uint8_t *)pa)[i] = 0;
    if (!vm_map(&image->vm, va, pa, flags)) {
        if (!page_free(pa)) panic("exec mapping rollback");
        return -NX_ENOMEM;
    }
    image->size += PAGE_SIZE;
    return 0;
}
/* Populate staged RX/RO pages through their supervisor physical aliases. */
static void populate(struct exec_image *image, uintptr_t va, const void *src, size_t length)
{
    const uint8_t *s = src;
    while (length) {
        uintptr_t pa;
        if (!vm_translate(&image->vm, va, &pa, NULL)) panic("missing staged exec mapping");
        size_t n = PAGE_SIZE - (va & (PAGE_SIZE - 1));
        if (n > length) n = length;
        bytes_copy((void *)pa, s, n);
        va += n; s += n; length -= n;
    }
}
int elf_load(const void *file, size_t length, const struct exec_args *args, struct exec_image *image)
{
    if (image->vm.root) return -NX_EINVAL;
    *image = (struct exec_image){0};
    struct elf_header h;
    if (length < sizeof(h)) return -NX_ENOEXEC;
    bytes_copy(&h, file, sizeof(h));
    if (h.ident[0] != 0x7f || h.ident[1] != 'E' || h.ident[2] != 'L' || h.ident[3] != 'F' ||
        h.ident[4] != 2 || h.ident[5] != 1 || h.ident[6] != 1 || h.ident[7] || h.ident[8] || h.type != 2 ||
        h.machine != 62 || h.version != 1 || h.flags || h.ehsize != sizeof(h) ||
        h.phentsize != sizeof(struct elf_program) || !h.phnum || h.phnum > MAX_PHDR ||
        h.phoff > length || h.phnum > (length - h.phoff) / sizeof(struct elf_program))
        return -NX_ENOEXEC;
    struct elf_program ph[MAX_PHDR];
    bytes_copy(ph, (const uint8_t *)file + h.phoff, h.phnum * sizeof(*ph));
    bool entry = false, load = false;
    size_t total = 0;
    for (unsigned i = 0; i < h.phnum; ++i) {
        struct elf_program *p = &ph[i];
        /* Dynamic linking, interpreter, TLS and executable stacks are unsupported. */
        if (p->type == 2 || p->type == 3 || p->type == 7 ||
            (p->type == 0x6474e551 && (p->flags & 1))) return -NX_ENOEXEC;
        if (p->type != 1) continue;
        if (p->filesz > p->memsz || p->offset > length || p->filesz > length - p->offset ||
            (p->flags & ~7U) || !(p->flags & 4) || (p->flags & 3) == 3 ||
            (p->align > 1 && ((p->align & (p->align - 1)) ||
             (p->vaddr & (p->align - 1)) != (p->offset & (p->align - 1)))) ||
            (p->vaddr & (PAGE_SIZE - 1)) != (p->offset & (PAGE_SIZE - 1)))
            return -NX_ENOEXEC;
        if (!p->memsz) continue;
        uintptr_t ceiling = EXEC_STACK_TOP - (EXEC_STACK_PAGES + 1) * PAGE_SIZE;
        if (p->vaddr < USER_BASE || p->vaddr >= ceiling || p->memsz > ceiling - p->vaddr)
            return -NX_ENOEXEC;
        uintptr_t start = p->vaddr & ~(PAGE_SIZE - 1);
        uintptr_t end = (p->vaddr + p->memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        if (end - start > MAX_IMAGE_BYTES - total) return -NX_ENOEXEC;
        total += end - start;
        for (unsigned j = 0; j < i; ++j) {
            if (ph[j].type != 1 || !ph[j].memsz) continue;
            uintptr_t other_start = ph[j].vaddr & ~(PAGE_SIZE - 1);
            uintptr_t other_end = (ph[j].vaddr + ph[j].memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            if (start < other_end && other_start < end) return -NX_ENOEXEC;
        }
        if ((p->flags & 1) && h.entry >= p->vaddr && h.entry - p->vaddr < p->filesz) entry = true;
        load = true;
    }
    if (!load || !entry) return -NX_ENOEXEC;
    if (!args || args->argc > EXEC_ARG_COUNT || args->envc > EXEC_ARG_COUNT || args->used > EXEC_ARG_BYTES)
        return -NX_E2BIG;
    /* Validate trusted-builder offsets too, before publishing any image. */
    for (size_t i = 0; i < args->argc + args->envc; ++i) {
        size_t offset = i < args->argc ? args->argv[i] : args->envp[i - args->argc];
        if (offset >= args->used) return -NX_EINVAL;
        size_t n = offset;
        while (n < args->used && args->bytes[n]) ++n;
        if (n == args->used) return -NX_EINVAL;
    }
    if (!vm_create(&image->vm)) return -NX_ENOMEM;
    int error = 0;
    for (unsigned i = 0; i < h.phnum && !error; ++i) {
        struct elf_program *p = &ph[i];
        if (p->type != 1 || !p->memsz) continue;
        uintptr_t end = (p->vaddr + p->memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        if (end > image->heap) image->heap = end;
        unsigned flags = ((p->flags & 1) ? VM_EXEC : 0) | ((p->flags & 2) ? VM_WRITE : 0);
        for (uintptr_t va = p->vaddr & ~(PAGE_SIZE - 1); va < end && !error; va += PAGE_SIZE)
            error = map_zero(image, va, flags);
        if (!error) populate(image, p->vaddr, (const uint8_t *)file + p->offset, p->filesz);
        if (p->flags & 1) image->text += p->memsz;
        else { image->data += p->filesz; image->bss += p->memsz - p->filesz; }
    }
    for (unsigned i = 0; i < EXEC_STACK_PAGES && !error; ++i)
        error = map_zero(image, EXEC_STACK_TOP - (i + 1) * PAGE_SIZE, VM_WRITE);
    if (error) { exec_image_release(image); return error; }
    uintptr_t strings = EXEC_STACK_TOP - args->used;
    if (args->used) populate(image, strings, args->bytes, args->used);
    /* argc, argv[], NULL, envp[], NULL, AT_NULL pair. All words are 64-bit. */
    uint64_t words[1 + EXEC_ARG_COUNT * 2 + 4];
    size_t n = 0;
    words[n++] = args->argc;
    for (size_t i = 0; i < args->argc; ++i) words[n++] = strings + args->argv[i];
    words[n++] = 0;
    for (size_t i = 0; i < args->envc; ++i) words[n++] = strings + args->envp[i];
    words[n++] = 0; words[n++] = 0; words[n++] = 0;
    image->stack = (strings - n * sizeof(uint64_t)) & ~15UL;
    populate(image, image->stack, words, n * sizeof(uint64_t));
    image->entry = h.entry;
    return 0;
}
