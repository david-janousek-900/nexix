/* V7 sys1.c exec replacement semantics with a native ELF64 image. */
#include "exec.h"
#include "files.h"
#include "uaccess.h"
#include "segments.h"
#include <nexix/errno.h>
#include "userland.h"
/* Single CPU, IRQs disabled throughout exec; bounded scratch avoids using the
 * guarded 16 KiB kernel stack for the argument strings. */
static struct exec_args scratch;
static int vector(struct address_space *vm, uintptr_t address, size_t *count, size_t *offsets)
{
    *count = 0;
    if (!address) return 0;
    for (size_t i = 0; i <= EXEC_ARG_COUNT; ++i) {
        uintptr_t pointer;
        if (address > UINTPTR_MAX - i * sizeof(pointer)) return -NX_EFAULT;
        int error = copyin(vm, address + i * sizeof(pointer), &pointer, sizeof(pointer));
        if (error) return error;
        if (!pointer) return 0;
        if (i == EXEC_ARG_COUNT || scratch.used == EXEC_ARG_BYTES) return -NX_E2BIG;
        size_t copied;
        offsets[i] = scratch.used;
        error = copyinstr(vm, pointer, scratch.bytes + scratch.used,
                         EXEC_ARG_BYTES - scratch.used, &copied);
        if (error) return error;
        scratch.used += copied;
        ++*count;
    }
    return -NX_E2BIG;
}
int proc_exec(uintptr_t path, uintptr_t argv, uintptr_t envp, struct interrupt_frame *frame)
{
    struct proc *p = proc_current();
    char name[256];
    int error = copyinstr(&p->p_vm, path, name, sizeof(name), NULL);
    if (error) return error;
    const struct embedded_program *program = NULL;
    if (!files_exec_enabled()) {
        program = userland_program(name);
        if (!program) return -NX_ENOENT;
    }
    const char *comm = name;
    for (const char *c = name; *c; ++c) if (*c == '/') comm = c + 1;
    struct user *u = p->p_user;
    if (p->p_textp || !files_refs_valid(u)) return -NX_EBUSY;
    scratch.used = 0;
    error = vector(&p->p_vm, argv, &scratch.argc, scratch.argv);
    if (!error) error = vector(&p->p_vm, envp, &scratch.envc, scratch.envp);
    if (error) return error;
    struct exec_image image = {0};
    error = files_exec_enabled() ? files_load_exec(name, &scratch, &image) :
        elf_load(program->start, (uintptr_t)program->end - (uintptr_t)program->start, &scratch, &image);
    if (error) return error;
    struct exec_image old = {.vm = p->p_vm};
    p->p_vm = image.vm;
    p->p_size = image.size;
    p->p_brk_base = p->p_brk = image.heap;
    vm_activate(&p->p_vm);
    exec_image_release(&old);
    files_exec(u);
    u->u_tsize = image.text;
    u->u_dsize = image.data + image.bss;
    u->u_ssize = EXEC_STACK_PAGES * PAGE_SIZE;
    u->u_exdata.entry = image.entry;
    u->u_exdata.text = image.text;
    u->u_exdata.data = image.data;
    u->u_exdata.bss = image.bss;
    unsigned n = 0;
    while (n < V7_DIRSIZ && comm[n]) { u->u_comm[n] = comm[n]; ++n; }
    while (n < V7_DIRSIZ) u->u_comm[n++] = 0;
    for (unsigned i = 0; i < V7_NSIG; ++i)
        if (u->u_signal[i] != 1) { u->u_signal[i] = 0; u->u_restorer[i] = 0; } /* Preserve ignored dispositions. */
    u->u_prof.pr_scale = 0;
    u->u_acflag &= ~1U; /* V7 AFORK cleared on successful exec. */
    *frame = (struct interrupt_frame){.rip = image.entry, .rsp = image.stack,
        .cs = USER_CS, .ss = USER_DS, .rflags = 0x202};
    return 0;
}
