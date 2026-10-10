#include "uaccess.h"
#include "process.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Noncontiguous physical backing and a supervisor leaf, independent of the
 * implementation's walking logic. No privileged instructions in host tests. */
static _Alignas(PAGE_SIZE) uint8_t backing[4][PAGE_SIZE];
static const struct address_space space = { .root = 1 };
uintptr_t proc_irq_save(void) { return 0; }
void proc_irq_restore(uintptr_t flags) { assert(flags == 0); }
_Noreturn void panic(const char *message) { puts(message); abort(); }
bool vm_translate(const struct address_space *s, uintptr_t address,
                  uintptr_t *physical, unsigned *permissions)
{
    if (s != &space || address < USER_BASE) return false;
    uintptr_t base = address & ~(PAGE_SIZE - 1);
    unsigned index, flags = VM_USER | VM_WRITE;
    if (base == USER_BASE) index = 2;
    else if (base == USER_BASE + PAGE_SIZE) index = 0;
    else if (base == USER_BASE + 2 * PAGE_SIZE) { index = 3; flags = VM_USER; }
    else if (base == USER_BASE + 3 * PAGE_SIZE) { index = 1; flags = VM_WRITE; }
    else if (base == USER_LIMIT - PAGE_SIZE) index = 1;
    else return false;
    if (physical) *physical = (uintptr_t)backing[index] + (address & (PAGE_SIZE - 1));
    if (permissions) *permissions = flags;
    return true;
}
int main(void)
{
    uint8_t source[32], destination[32];
    for (unsigned i = 0; i < sizeof(source); ++i) source[i] = i + 1;
    uintptr_t cross = USER_BASE + PAGE_SIZE - 7;
    assert(copyout(&space, cross, source, sizeof(source)) == 0);
    assert(memcmp(backing[2] + PAGE_SIZE - 7, source, 7) == 0);
    assert(memcmp(backing[0], source + 7, 25) == 0);
    assert(copyin(&space, cross, destination, sizeof(destination)) == 0);
    assert(memcmp(source, destination, sizeof(source)) == 0);
    memset(backing[0] + PAGE_SIZE - 16, 0x5a, 16);
    assert(copyout(&space, USER_BASE + 2 * PAGE_SIZE - 16, source, 32) == -NX_EFAULT);
    for (unsigned i = 0; i < 16; ++i) assert(backing[0][PAGE_SIZE - 16 + i] == 0x5a);
    memset(destination, 0x6b, sizeof(destination));
    assert(copyin(&space, USER_BASE + 3 * PAGE_SIZE - 16, destination, 32) == -NX_EFAULT);
    for (unsigned i = 0; i < sizeof(destination); ++i) assert(destination[i] == 0x6b);
    assert(copyin(&space, USER_BASE + 2 * PAGE_SIZE, destination, 1) == 0); /* RO input */
    uintptr_t invalid[] = {0, USER_BASE - 1, USER_BASE + 4 * PAGE_SIZE,
        USER_LIMIT, UINT64_C(0xffff800000000000), UINTPTR_MAX - 15};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(copyin(&space, invalid[i], destination, 32) == -NX_EFAULT);
        assert(copyout(&space, invalid[i], source, 32) == -NX_EFAULT);
    }
    assert(!user_access_ok(&space, USER_BASE, SIZE_MAX, false));
    assert(!user_access_ok(&space, USER_BASE + 3 * PAGE_SIZE, 1, false));
    assert(copyout(&space, USER_LIMIT - 32, source, 32) == 0);
    assert(copyout(&space, USER_LIMIT - 16, source, 32) == -NX_EFAULT);
    assert(copyin(&space, UINTPTR_MAX, NULL, 0) == 0);
    assert(copyout(&space, 0, NULL, 0) == 0);
    assert(!user_access_ok(NULL, USER_BASE, 1, false));

    cross += 3; /* String terminator lies on the second noncontiguous page. */
    const char string[] = "abcde";
    assert(copyout(&space, cross, string, sizeof(string)) == 0);
    size_t copied = 99;
    char output[16];
    assert(copyinstr(&space, cross, output, sizeof(output), &copied) == 0);
    assert(copied == 6 && memcmp(output, string, 6) == 0);
    /* A terminator before an invalid suffix succeeds, unlike a range preflight. */
    backing[3][PAGE_SIZE - 2] = 'z'; backing[3][PAGE_SIZE - 1] = 0;
    uintptr_t end = USER_BASE + 3 * PAGE_SIZE - 2;
    assert(copyinstr(&space, end, output, 16, &copied) == 0);
    assert(copied == 2 && output[0] == 'z' && output[1] == 0);
    backing[3][PAGE_SIZE - 1] = 'x';
    assert(copyinstr(&space, end, output, 16, &copied) == -NX_EFAULT);
    assert(copied == 2);
    assert(copyinstr(&space, cross, output, 5, &copied) == -NX_E2BIG && copied == 5);
    assert(copyinstr(&space, 0, output, 16, &copied) == -NX_EFAULT && copied == 0);
    assert(copyinstr(&space, cross, output, 0, &copied) == -NX_E2BIG && copied == 0);
    backing[1][PAGE_SIZE - 1] = 'x';
    assert(copyinstr(&space, USER_LIMIT - 1, output, 16, &copied) == -NX_EFAULT && copied == 1);
    puts("PASS: user-copy boundaries, permissions, atomic rejection, and bounded strings");
}
