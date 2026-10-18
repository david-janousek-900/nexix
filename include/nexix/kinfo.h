#ifndef NEXIX_ABI_KINFO_H
#define NEXIX_ABI_KINFO_H
#include <stdint.h>
#define NX_KINFO_VERSION 1U
#define NX_KINFO_PROCESS 1U
#define NX_KINFO_FILE 2U
#define NX_KINFO_INODE 3U
#define NX_KINFO_TEXT 4U
#define NX_KINFO_MOUNT 5U
#define NX_KINFO_MEMORY 6U
#define NX_KINFO_IO 7U
#define NX_KINFO_RESIDENT 1U
#define NX_KINFO_MOUNT_ROOT 1U
#define NX_KINFO_MOUNT_RDONLY 2U
#define NX_KINFO_MOUNT_FAILED 4U
#define NX_KINFO_FILE_REGULAR 1U
#define NX_KINFO_FILE_PIPE 2U
#define NX_KINFO_FILE_TTY 3U
#define NX_KINFO_FILE_STORAGE 4U
#define NX_KINFO_FILE_MPX 5U
#define NX_KINFO_FILE_TAPE 7
#define NX_KINFO_FILE_MEMORY 6U
#define NX_KINFO_FILE_MODEM 8U
/* Process states and flags preserve V7's meanings, independent of private layouts. */
#define NX_KPROC_SLEEP 1U
#define NX_KPROC_WAIT 2U
#define NX_KPROC_RUN 3U
#define NX_KPROC_ZOMBIE 5U
#define NX_KPROC_STOP 6U
#define NX_KPROC_LOADED 01
#define NX_KPROC_SYSTEM 02
#define NX_KPROC_LOCKED 04
#define NX_KPROC_TRACED 020
#define NX_KPROC_WAITED 040
#define NX_KPROC_USERLOCK 0100
#define NX_KFILE_READ 01
#define NX_KFILE_WRITE 02
#define NX_KFILE_PIPE 04
#define NX_KINODE_LOCK 01
#define NX_KINODE_UPDATE 02
#define NX_KINODE_ACCESS 04
#define NX_KINODE_MOUNT 010
#define NX_KINODE_WANT 020
#define NX_KINODE_TEXT 040
#define NX_KINODE_CHANGE 0100
#define NX_KTEXT_TRACE 01
#define NX_KTEXT_WRITE 02
#define NX_KTEXT_LOAD 04
#define NX_KTEXT_LOCK 010
#define NX_KTEXT_WANT 020
struct nx_kinfo_header {
    uint32_t version, kind, record_size, count;
    uint32_t header_size, flags;
    uint64_t total_bytes, ticks;
    uint32_t tick_hz, reserved;
};
struct nx_kinfo_process {
    uint32_t slot;
    int32_t pid, ppid, pgrp;
    uint16_t ruid, euid, rgid, egid;
    uint8_t state, flags, priority, nice, cpu, age;
    uint16_t pending;
    int32_t status;
    uint16_t tty_dev, waiting;
    uint64_t size_bytes, text_bytes, data_bytes, stack_bytes;
    int64_t user_ticks, system_ticks, child_user_ticks, child_system_ticks, start_time;
    uint64_t alarm_ticks;
    uint32_t text_id, reserved;
    char command[16];
};
struct nx_kinfo_file {
    uint32_t slot, refs, flags, open_flags, inode_id, reserved;
    int64_t offset;
    uint16_t dev, ino;
    uint32_t kind;
};
struct nx_kinfo_inode {
    uint32_t slot, refs;
    uint16_t dev, ino, mode, uid, gid, flags;
    int32_t nlink;
    int64_t size;
    uint16_t rdev, reserved16;
    uint32_t reserved32;
};
struct nx_kinfo_text {
    uint32_t slot, refs, resident_refs, flags;
    uint64_t size;
    uint16_t dev, ino;
    uint32_t reserved;
};
struct nx_kinfo_mount {
    uint32_t slot, flags;
    uint16_t dev, point_dev, point_ino, reserved16;
    uint64_t fs_blocks, device_blocks;
    uint32_t inodes, cache_capacity, cache_used, reserved32;
};
struct nx_kinfo_memory {
    uint64_t total_pages, free_pages, allocated_pages;
    uint32_t page_size, flags;
};
struct nx_kinfo_io {
    uint64_t user_ticks, nice_ticks, system_ticks, idle_ticks;
    uint64_t read_sectors, write_sectors, read_errors, write_errors;
    uint64_t cache_lookups, cache_hits, tty_in, tty_out;
};
_Static_assert(sizeof(struct nx_kinfo_header)==48,"snapshot header");
_Static_assert(sizeof(struct nx_kinfo_process)==144,"snapshot process");
_Static_assert(sizeof(struct nx_kinfo_file)==40,"snapshot file");
_Static_assert(sizeof(struct nx_kinfo_inode)==40,"snapshot inode");
_Static_assert(sizeof(struct nx_kinfo_text)==32,"snapshot text");
_Static_assert(sizeof(struct nx_kinfo_mount)==48,"snapshot mount");
_Static_assert(sizeof(struct nx_kinfo_memory)==32,"snapshot memory");
_Static_assert(sizeof(struct nx_kinfo_io)==96,"snapshot io");
#endif
