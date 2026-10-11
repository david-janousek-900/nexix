#include "v7/fs.h"
#include "exec.h"
#include "serial.h"
#include <nexix/errno.h>
#define REQUIRE(c) do { if (!(c)) panic("filesystem self-test: " #c); } while (0)
extern const uint8_t _binary_build_root_test_img_start[], _binary_build_root_test_img_end[];
static uint8_t backing[1024 * V7_BSIZE];
static uint8_t buffer[2048], executable[65536];
static struct ramdisk ram;
static struct v7fs fs;
static struct exec_args args;
static uint32_t allocated[1024];
static bool seen[1024];
static void expect_file(v7_ino_t ino, uint64_t offset, const char *text, size_t size)
{
    size_t done;
    REQUIRE(v7fs_read(&fs, ino, offset, buffer, size, &done) == 0 && done == size);
    for (size_t i = 0; i < size; ++i) REQUIRE(buffer[i] == (uint8_t)text[i]);
}
void filesystem_selftest(void)
{
    uint32_t before = page_free_count();
    size_t length = (uintptr_t)_binary_build_root_test_img_end - (uintptr_t)_binary_build_root_test_img_start;
    REQUIRE(length == sizeof(backing));
    for (size_t i = 0; i < length; ++i) backing[i] = _binary_build_root_test_img_start[i];
    REQUIRE(ramdisk_init(&ram, backing, length, false) == 0);
    REQUIRE(block_read(&ram.device, 1024, 0, NULL) == 0);
    REQUIRE(block_read(&ram.device, 1024, 1, buffer) == -NX_EIO);
    REQUIRE(block_write(&ram.device, UINT32_MAX, UINT32_MAX, buffer) == -NX_EIO);
    REQUIRE(block_read(&ram.device, 0, 1, NULL) == -NX_EINVAL);
    serial_puts("NEXIX_BLOCK_OK\r\n");
    REQUIRE(v7fs_mount(&fs, &ram.device, true) == 0);
    REQUIRE(v7fs_mount(&fs, &ram.device, false) == -NX_EBUSY);
    v7_ino_t ino;
    REQUIRE(v7fs_lookup(&fs, 0, "/", &ino) == 0 && ino == 2);
    struct v7fs_inode root;
    REQUIRE(v7fs_inode_get(&fs, 2, &root) == 0 && root.nlink == 5);
    REQUIRE(v7fs_lookup(&fs, 2, "//bin/../hello", &ino) == 0 && ino == 5);
    REQUIRE(v7fs_lookup(&fs, 3, "../hello", &ino) == 0 && ino == 5);
    REQUIRE(v7fs_lookup(&fs, 2, "/../../hello", &ino) == 0 && ino == 5);
    REQUIRE(v7fs_lookup(&fs, 2, "/abcdefghijklmn-extra", &ino) == 0 && ino == 9);
    REQUIRE(v7fs_lookup(&fs, 2, "/hello/", &ino) == -NX_ENOTDIR);
    REQUIRE(v7fs_lookup(&fs, 2, "/hello/x", &ino) == -NX_ENOTDIR);
    REQUIRE(v7fs_lookup(&fs, 2, "/missing", &ino) == -NX_ENOENT);
    REQUIRE(v7fs_lookup(&fs, 2, "", &ino) == 0 && ino == 2); /* V7 stat("") is cwd. */
    const char hello[] = "Hello from the Nexix V7 filesystem!\n";
    expect_file(5, 0, hello, sizeof(hello) - 1);
    size_t done;
    REQUIRE(v7fs_read(&fs, 5, sizeof(hello), buffer, sizeof(buffer), &done) == 0 && done == 0);
    REQUIRE(v7fs_read(&fs, 5, -1, buffer, 1, &done) == -NX_EINVAL);
    REQUIRE(v7fs_read(&fs, 13, 0, buffer, 1, &done) == -NX_ENODEV);
    struct v7fs_inode inode;
    REQUIRE(v7fs_inode_get(&fs, 13, &inode) == 0 && inode.addr[0] == 0x100);
    const uint32_t offsets[] = {9 * 512 + 500, 137 * 512 + 500};
    for (unsigned j = 0; j < 2; ++j) {
        REQUIRE(v7fs_read(&fs, 6, offsets[j], buffer, 1500, &done) == 0 && done == 1500);
        for (unsigned i = 0; i < done; ++i) REQUIRE(buffer[i] == (uint8_t)((offsets[j] + i) * 17 + 3));
    }
    const uint64_t triple = (10 + 128 + 128 * 128) * 512UL;
    expect_file(7, triple, "TRIPLE", 6);
    REQUIRE(v7fs_read(&fs, 7, triple - 700, buffer, 700, &done) == 0 && done == 700);
    for (unsigned i = 0; i < done; ++i) REQUIRE(buffer[i] == 0);
    REQUIRE(v7fs_write(&fs, 8, 0, "test", 4, &done) == -NX_EROFS && done == 0);
    uint32_t block;
    REQUIRE(v7fs_alloc_block(&fs, &block) == -NX_EROFS);
    REQUIRE(v7fs_unmount(&fs) == 0);
    serial_puts("NEXIX_V7FS_READ_OK\r\n");
    REQUIRE(v7fs_mount(&fs, &ram.device, false) == 0);
    uint32_t total = v7_disk_u32(fs.super.tfree);
    unsigned count = 0;
    int error;
    while (!(error = v7fs_alloc_block(&fs, &block))) {
        REQUIRE(block < 1024 && !seen[block] && count < 1024);
        seen[block] = true; allocated[count++] = block;
        REQUIRE(block_read(&ram.device, block, 1, buffer) == 0);
        for (unsigned i = 0; i < V7_BSIZE; ++i) REQUIRE(buffer[i] == 0);
    }
    REQUIRE(error == -NX_ENOSPC && count == total && v7_disk_u32(fs.super.tfree) == 0);
    REQUIRE(v7fs_free_block(&fs, 1) == -NX_EINVAL);
    REQUIRE(v7fs_free_block(&fs, fs.fsize) == -NX_EINVAL);
    for (unsigned i = 0; i < count; ++i) REQUIRE(v7fs_free_block(&fs, allocated[i]) == 0);
    REQUIRE(v7fs_free_block(&fs, allocated[count - 1]) == -NX_EINVAL);
    REQUIRE(v7_disk_u32(fs.super.tfree) == total);
    REQUIRE(v7fs_unmount(&fs) == 0 && v7fs_mount(&fs, &ram.device, false) == 0);
    REQUIRE(v7fs_alloc_block(&fs, &block) == 0 && v7fs_free_block(&fs, block) == 0);
    serial_puts("NEXIX_V7FS_ALLOC_OK\r\n");
    const uint64_t positions[] = {0, 9 * 512 + 511, (10 + 128) * 512 + 7,
        triple + 7, V7FS_MAX_FILE_SIZE - 3};
    for (unsigned i = 0; i < 5; ++i) {
        REQUIRE(v7fs_write(&fs, 8, positions[i], "xyz", 3, &done) == 0 && done == 3);
        expect_file(8, positions[i], "xyz", 3);
    }
    REQUIRE(v7fs_write(&fs, 8, V7FS_MAX_FILE_SIZE, "x", 1, &done) == -NX_EFBIG && done == 0);
    REQUIRE(v7fs_write(&fs, 2, 0, "x", 1, &done) == -NX_EISDIR);
    REQUIRE(v7fs_read(&fs, 8, 3, buffer, 100, &done) == 0 && done == 100);
    for (unsigned i = 0; i < done; ++i) REQUIRE(buffer[i] == 0);
    REQUIRE(v7fs_unmount(&fs) == 0 && v7fs_mount(&fs, &ram.device, true) == 0);
    for (unsigned i = 0; i < 5; ++i) expect_file(8, positions[i], "xyz", 3);
    serial_puts("NEXIX_V7FS_WRITE_OK\r\n");
    /* Load a real executable read through directory and indirect-block APIs. */
    REQUIRE(v7fs_lookup(&fs, 2, "/bin/libc-test", &ino) == 0);
    REQUIRE(v7fs_inode_get(&fs, ino, &inode) == 0 && inode.size < sizeof(executable));
    REQUIRE(v7fs_read(&fs, ino, 0, executable, inode.size, &done) == 0 && done == inode.size);
    struct exec_image image = {0};
    REQUIRE(elf_load(executable, inode.size, &args, &image) == 0);
    struct proc *p = proc_alloc();
    REQUIRE(p);
    struct exec_image empty = {.vm = p->p_vm};
    p->p_vm = image.vm; p->p_size = image.size; p->p_brk_base = p->p_brk = image.heap; exec_image_release(&empty);
    REQUIRE(proc_start_user(p, image.entry, image.stack));
    v7_pid_t pid = p->p_pid;
    proc_run();
    int32_t status;
    REQUIRE(proc_collect(&status) == pid && status == 0 && proc_collect(NULL) == -NX_ECHILD);
    REQUIRE(page_free_count() == before && v7fs_unmount(&fs) == 0);
    serial_puts("NEXIX_V7FS_EXEC_OK\r\n");
    serial_puts("NEXIX_V7FS_OK\r\n");
}
