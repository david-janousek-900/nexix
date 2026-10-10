#include "v7/core.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    /* Golden PDP-11 bytes, not merely encoder/decoder round trips. */
    uint8_t bytes[4];
    const uint8_t word[] = {0x34, 0x12};
    const uint8_t longword[] = {0x34, 0x12, 0x78, 0x56};
    const uint8_t address[] = {0x12, 0x56, 0x34};
    v7_disk_put16(bytes, 0x1234);
    assert(memcmp(bytes, word, 2) == 0);
    assert(v7_disk_u16(word) == 0x1234);
    v7_disk_put32(bytes, 0x12345678);
    assert(memcmp(bytes, longword, 4) == 0);
    assert(v7_disk_u32(longword) == 0x12345678);
    v7_disk_put32(bytes, UINT32_MAX);
    assert(v7_disk_u32(bytes) == UINT32_MAX);
    assert(v7_disk_putaddr(bytes, 0x123456));
    assert(memcmp(bytes, address, 3) == 0);
    assert(v7_disk_addr(address) == 0x123456);
    assert(v7_disk_putaddr(bytes, 0xffffff));
    assert(v7_disk_addr(bytes) == 0xffffff);
    assert(!v7_disk_putaddr(bytes, 0x1000000));
    assert(v7_disk_addr(bytes) == 0xffffff); /* rejected write is atomic */
    assert(v7_itod(1) == 2 && v7_itoo(1) == 0);
    assert(v7_itod(8) == 2 && v7_itoo(8) == 7);
    assert(v7_itod(9) == 3 && v7_itoo(9) == 0);
    assert(v7_itod(UINT16_MAX) == 8193 && v7_itoo(UINT16_MAX) == 6);
    for (unsigned major = 0; major < 256; ++major)
        for (unsigned minor = 0; minor < 256; ++minor) {
            v7_dev_t dev = v7_makedev(major, minor);
            assert(v7_major(dev) == major && v7_minor(dev) == minor);
        }
    struct user user = {0};
    struct proc proc = {.p_user = &user};
    user.u_procp = &proc;
    user.u_arg[0] = user.u_signal[1] = UINT64_C(0x123456789ab);
    user.u_offset = INT64_C(0x123456789ab);
    assert(proc.p_user->u_procp == &proc);
    assert(user.u_arg[0] == UINT64_C(0x123456789ab));
    assert(user.u_signal[1] == UINT64_C(0x123456789ab));
    assert(user.u_offset == INT64_C(0x123456789ab));
    puts("PASS: V7 native widths and PDP-11 disk encodings");
}
