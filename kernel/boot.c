#include "boot.h"

static bool string_valid(const char *s, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
        if (!s[i])
            return true;
    return false;
}

static bool string_equal(const char *a, const char *b)
{
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

const struct boot_tag *boot_first(const struct boot_info *info)
{
    return (const struct boot_tag *)(info->address + 8);
}

const struct boot_tag *boot_next(const struct boot_tag *tag)
{
    if (!tag->type)
        return NULL;
    return (const struct boot_tag *)((uintptr_t)tag + (((uint64_t)tag->size + 7) & ~7ULL));
}

bool boot_parse(uintptr_t address, size_t accessible_bytes, struct boot_info *info)
{
    if (!address || (address & 7) || accessible_bytes < 16)
        return false;
    const uint32_t *header = (const uint32_t *)address;
    uint32_t total = header[0];
    if (total < 16 || (total & 7) || total > accessible_bytes ||
        total > UINTPTR_MAX - address || header[1])
        return false;
    *info = (struct boot_info) { .address = address, .size = total };
    bool command_seen = false;
    for (uint32_t offset = 8; offset <= total - 8;) {
        const struct boot_tag *tag = (const struct boot_tag *)(address + offset);
        uint32_t size = tag->size;
        if (size < 8 || size > total - offset)
            return false;
        if (!tag->type)
            return size == 8 && offset == total - 8 && info->mmap;
        if (tag->type == 1) {
            const char *s = (const char *)(tag + 1);
            if (command_seen || !string_valid(s, size - 8))
                return false;
            command_seen = true;
            if (string_equal(s, "test=ud")) info->test = BOOT_TEST_UD;
            if (string_equal(s, "test=pf")) info->test = BOOT_TEST_PF;
            if (string_equal(s, "test=memory")) info->test = BOOT_TEST_MEMORY;
            if (string_equal(s, "test=wp")) info->test = BOOT_TEST_WP;
            if (string_equal(s, "test=nx")) info->test = BOOT_TEST_NX;
            if (string_equal(s, "test=unmap")) info->test = BOOT_TEST_UNMAP;
            if (string_equal(s, "test=null")) info->test = BOOT_TEST_NULL;
            if (string_equal(s, "test=paging-oom")) info->test = BOOT_TEST_PAGING_OOM;
            if (string_equal(s, "test=protect")) info->test = BOOT_TEST_PROTECT;
            if (string_equal(s, "test=user-kernel")) info->test = BOOT_TEST_USER_KERNEL;
            if (string_equal(s, "test=user-ro")) info->test = BOOT_TEST_USER_RO;
            if (string_equal(s, "test=user-nx")) info->test = BOOT_TEST_USER_NX;
            if (string_equal(s, "test=user-priv")) info->test = BOOT_TEST_USER_PRIV;
            if (string_equal(s, "test=user-io")) info->test = BOOT_TEST_USER_IO;
            if (string_equal(s, "test=user-int")) info->test = BOOT_TEST_USER_INT;
            if (string_equal(s, "test=user-bad-stack")) info->test = BOOT_TEST_USER_BAD_STACK;
            if (string_equal(s, "test=user-df")) info->test = BOOT_TEST_USER_DF;
            if (string_equal(s, "test=tty")) info->test = BOOT_TEST_TTY;
            if (string_equal(s, "test=tty-demo")) info->test = BOOT_TEST_TTY_DEMO;
        } else if (tag->type == 3) {
            if (size < 17)
                return false;
            const struct boot_module *module = (const struct boot_module *)tag;
            if (module->end < module->start ||
                !string_valid((const char *)(module + 1), size - 16))
                return false;
        } else if (tag->type == 6) {
            if (size < 16 || info->mmap)
                return false;
            const struct boot_mmap *map = (const struct boot_mmap *)tag;
            if (map->entry_size < sizeof(struct boot_mmap_entry) ||
                map->entry_version || (size - 16) % map->entry_size)
                return false;
            for (uint32_t n = 16; n < size; n += map->entry_size) {
                const struct boot_mmap_entry *entry =
                    (const struct boot_mmap_entry *)((uintptr_t)tag + n);
                if (entry->length > UINT64_MAX - entry->address)
                    return false;
            }
            info->mmap = map;
        }
        uint64_t next = (uint64_t)offset + (((uint64_t)size + 7) & ~7ULL);
        if (next > total - 8)
            return false;
        offset = (uint32_t)next;
    }
    return false;
}
