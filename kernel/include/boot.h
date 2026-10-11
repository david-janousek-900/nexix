#ifndef NEXIX_BOOT_H
#define NEXIX_BOOT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct boot_tag { uint32_t type, size; };
struct boot_mmap_entry {
    uint64_t address, length;
    uint32_t type, reserved;
} __attribute__((packed));
struct boot_mmap {
    struct boot_tag tag;
    uint32_t entry_size, entry_version;
};
struct boot_module {
    struct boot_tag tag;
    uint32_t start, end;
};
enum boot_test {
    BOOT_NORMAL, BOOT_TEST_UD, BOOT_TEST_PF, BOOT_TEST_MEMORY,
    BOOT_TEST_WP, BOOT_TEST_NX, BOOT_TEST_UNMAP, BOOT_TEST_NULL, BOOT_TEST_PAGING_OOM,
    BOOT_TEST_PROTECT, BOOT_TEST_USER_KERNEL, BOOT_TEST_USER_RO, BOOT_TEST_USER_NX,
    BOOT_TEST_USER_PRIV, BOOT_TEST_USER_IO, BOOT_TEST_USER_INT,
    BOOT_TEST_USER_BAD_STACK, BOOT_TEST_USER_DF, BOOT_TEST_TTY, BOOT_TEST_TTY_DEMO, BOOT_TEST_SELFTEST
};
struct boot_info {
    uintptr_t address;
    uint32_t size;
    const struct boot_mmap *mmap;
    enum boot_test test;
};
/* Validate the complete handoff within accessible_bytes before using its tags. */
bool boot_parse(uintptr_t address, size_t accessible_bytes, struct boot_info *info);
const struct boot_tag *boot_first(const struct boot_info *info);
/* Iteration helpers require a successfully parsed, unchanged boot buffer. */
const struct boot_tag *boot_next(const struct boot_tag *tag);
#endif
