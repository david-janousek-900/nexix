#include "console.h"
#include "exec.h"
#include "uaccess.h"
#include "serial.h"
#include <nexix/errno.h>
#define REQUIRE(c) do { if (!(c)) panic("exec self-test: " #c); } while (0)
extern const uint8_t _binary_build_exec_test_elf_start[], _binary_build_exec_test_elf_end[];
static struct exec_args args;
static uint8_t corrupted[32768];
static void set(uint8_t *p, uint64_t value, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) p[i] = value >> (i * 8);
}
static uint64_t get(const uint8_t *p, unsigned n)
{
    uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= (uint64_t)p[i] << (i * 8);
    return v;
}
static void reject(size_t length, size_t offset, uint64_t value, unsigned width)
{
    const uint8_t *file = _binary_build_exec_test_elf_start;
    for (size_t i = 0; i < length; ++i) corrupted[i] = file[i];
    set(corrupted + offset, value, width);
    struct exec_image image = {0};
    uint32_t before = page_free_count();
    REQUIRE(elf_load(corrupted, length, &args, &image) == -NX_ENOEXEC);
    REQUIRE(!image.vm.root && page_free_count() == before);
}
static int read_failure(void *context, size_t offset, void *buffer, size_t count)
{
    unsigned *remaining=context;
    if (!(*remaining)--) return -NX_EIO;
    const uint8_t *source=_binary_build_exec_test_elf_start+offset;
    for(size_t i=0;i<count;++i)((uint8_t *)buffer)[i]=source[i];
    return 0;
}
void exec_selftest(void)
{
    const uint8_t *file = _binary_build_exec_test_elf_start;
    size_t length = (uintptr_t)_binary_build_exec_test_elf_end - (uintptr_t)file;
    REQUIRE(length <= sizeof(corrupted));
    const char strings[] = "/bin/exec-test\0start\0NXY";
    for (size_t i = 0; i < sizeof(strings); ++i) args.bytes[i] = strings[i];
    args.used = sizeof(strings);
    args.argc = 2; args.envc = 1;
    args.argv[0] = 0; args.argv[1] = 15; args.envp[0] = 21;
    /* Offsets are explicit checks against the actual packed string layout. */
    REQUIRE(args.bytes[15] == 's' && args.bytes[21] == 'N');
    uint32_t before = page_free_count();
    struct exec_image image = {0};
    REQUIRE(elf_load(file, 63, &args, &image) == -NX_ENOEXEC);
    /* Header, program headers and segment reads fail independently; staged
     * address spaces/pages must be rolled back after any read failure. */
    for(unsigned fail=0;fail<4;++fail){
        unsigned remaining=fail;
        REQUIRE(elf_load_reader(read_failure,&remaining,length,&args,&image)==-NX_EIO);
        REQUIRE(!image.vm.root && page_free_count()==before);
    }
    size_t phoff = get(file + 32, 8);
    reject(length, 0, 0, 1);
    reject(length, 4, 1, 1); /* ELF32 */
    reject(length, 5, 2, 1); /* big endian */
    reject(length, 16, 3, 2); /* PIE */
    reject(length, 18, 3, 2); /* non-AMD64 */
    reject(length, 32, UINT64_MAX, 8);
    reject(length, 56, 33, 2);
    reject(length, 54, 55, 2);
    reject(length, 24, USER_BASE + 4096, 8); /* entry in data */
    reject(length, phoff, 3, 4); /* interpreter */
    reject(length, phoff + 4, 7, 4); /* W+X */
    reject(length, phoff + 8, UINT64_MAX, 8);
    reject(length, phoff + 16, 0, 8);
    reject(length, phoff + 40, UINT64_MAX, 8);
    reject(length, phoff + 48, 3, 8);
    reject(length, phoff + 16, USER_BASE + 1, 8);
    reject(length, phoff + 32, UINT64_MAX, 8);
    reject(length, phoff + 56 + 16, USER_BASE, 8); /* page overlap */
    reject(length, phoff + 56 + 40, 16UL * 1024 * 1024, 8);
    REQUIRE(elf_load(file, length, &args, &image) == 0);
    unsigned perms;
    REQUIRE(vm_translate(&image.vm, image.entry, NULL, &perms));
    REQUIRE(perms == (VM_USER | VM_EXEC));
    REQUIRE(vm_translate(&image.vm, USER_BASE + PAGE_SIZE, NULL, &perms));
    REQUIRE(perms == (VM_USER | VM_WRITE));
    REQUIRE(!vm_translate(&image.vm, EXEC_STACK_TOP - (EXEC_STACK_PAGES + 1) * PAGE_SIZE, NULL, NULL));
    REQUIRE(!vm_translate(&image.vm, EXEC_STACK_TOP, NULL, NULL));
    uint64_t words[8];
    REQUIRE(copyin(&image.vm, image.stack, words, sizeof(words)) == 0);
    REQUIRE(!(image.stack & 15) && words[0] == 2 && words[3] == 0 && words[5] == 0);
    REQUIRE(words[6] == 0 && words[7] == 0);
    char string[32]; size_t copied;
    REQUIRE(copyinstr(&image.vm, words[2], string, sizeof(string), &copied) == 0);
    REQUIRE(copied == 6 && string[0] == 's');
    exec_image_release(&image);
    REQUIRE(page_free_count() == before);
    /* Exercise a legal segment beginning inside a page, not just ld's aligned
     * fixture. Its prefix and BSS tail must remain zero. */
    for (size_t i = 0; i < length; ++i) corrupted[i] = file[i];
    uint64_t offset = get(file + phoff + 8, 8);
    uint64_t filesz = get(file + phoff + 32, 8);
    REQUIRE(filesz > 7 && filesz <= PAGE_SIZE);
    set(corrupted + phoff + 8, offset + 7, 8);
    set(corrupted + phoff + 16, USER_BASE + 7, 8);
    set(corrupted + phoff + 32, filesz - 7, 8);
    set(corrupted + 24, USER_BASE + 7, 8);
    REQUIRE(elf_load(corrupted, length, &args, &image) == 0);
    uintptr_t pa;
    REQUIRE(vm_translate(&image.vm, USER_BASE, &pa, NULL));
    for (unsigned i = 0; i < 7; ++i) REQUIRE(((uint8_t *)pa)[i] == 0);
    for (size_t i = 7; i < filesz; ++i) REQUIRE(((uint8_t *)pa)[i] == file[offset + i]);
    for (size_t i = filesz; i < PAGE_SIZE; ++i) REQUIRE(((uint8_t *)pa)[i] == 0);
    exec_image_release(&image);
    REQUIRE(page_free_count() == before);
    console_puts("ELF OK\r\n");
    /* Force partial construction failure after three successful allocations. */
    uintptr_t head = 0, page;
    while ((page = page_alloc())) { *(uintptr_t *)page = head; head = page; }
    for (unsigned i = 0; i < 3; ++i) {
        REQUIRE(head);
        uintptr_t next = *(uintptr_t *)head;
        REQUIRE(page_free(head)); head = next;
    }
    REQUIRE(elf_load(file, length, &args, &image) == -NX_ENOMEM);
    REQUIRE(!image.vm.root && page_free_count() == 3);
    while (head) { uintptr_t next = *(uintptr_t *)head; REQUIRE(page_free(head)); head = next; }
    REQUIRE(page_free_count() == before);
    console_puts("EXEC ROLLBACK OK\r\n");
    /* Full bounded argument set and zero-count vectors are valid layouts. */
    args.argc = args.envc = EXEC_ARG_COUNT;
    for (unsigned i = 0; i < EXEC_ARG_COUNT; ++i) args.argv[i] = args.envp[i] = 0;
    REQUIRE(elf_load(file, length, &args, &image) == 0);
    exec_image_release(&image);
    args.argc = args.envc = 0;
    REQUIRE(elf_load(file, length, &args, &image) == 0);
    REQUIRE(copyin(&image.vm, image.stack, words, 5 * sizeof(uint64_t)) == 0);
    for (unsigned i = 0; i < 5; ++i) REQUIRE(words[i] == 0);
    exec_image_release(&image);
    args.argc = EXEC_ARG_COUNT + 1;
    REQUIRE(elf_load(file, length, &args, &image) == -NX_E2BIG && !image.vm.root);
    args.argc = 2; args.envc = 1;
    args.argv[0] = 0; args.argv[1] = 15; args.envp[0] = 21;
    REQUIRE(page_free_count() == before);
    struct proc *p = proc_alloc();
    REQUIRE(p && elf_load(file, length, &args, &image) == 0);
    struct exec_image empty = {.vm = p->p_vm};
    p->p_vm = image.vm; p->p_size = image.size; p->p_brk_base = p->p_brk = image.heap;
    exec_image_release(&empty);
    REQUIRE(proc_start_user(p, image.entry, image.stack));
    v7_pid_t pid = p->p_pid;
    proc_run();
    int32_t status;
    REQUIRE(proc_collect(&status) == pid && status == 0);
    REQUIRE(proc_collect(NULL) == -NX_ECHILD);
    REQUIRE(page_free_count() == before);
    for (unsigned i = 1; i < V7_NPROC; ++i) REQUIRE(proc_table()[i].p_stat == V7_UNUSED);
    console_puts("EXEC OK\r\n");
}
