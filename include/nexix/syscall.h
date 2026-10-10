#ifndef NEXIX_ABI_SYSCALL_H
#define NEXIX_ABI_SYSCALL_H
/* Native x86_64 ABI, version 1. Shared with assembly and future libc. */
#define NX_ABI_VERSION 1
#define NX_SYS_EXIT 1
#define NX_SYS_FORK 2
#define NX_SYS_WAIT 7
#define NX_SYS_GETPID 20
#define NX_SYS_GETUID 24
#define NX_SYS_TIMES 43
#define NX_SYS_GETGID 47
#define NX_SYS_UMASK 60
/* Native extensions beyond the original 64-entry V7 table. */
#define NX_SYS_YIELD 64
#define NX_SYS_GETPPID 65
#define NX_SYS_GETEUID 66
#define NX_SYS_GETEGID 67
#define NX_CLK_TCK 100
#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stddef.h>
typedef int32_t nx_pid_t;
typedef uint16_t nx_uid_t;
typedef uint16_t nx_gid_t;
typedef uint16_t nx_mode_t;
typedef int64_t nx_off_t;
typedef int64_t nx_time_t;
typedef int64_t nx_clock_t;
struct nx_tms { nx_clock_t utime, stime, cutime, cstime; };
_Static_assert(sizeof(uintptr_t) == 8 && sizeof(size_t) == 8, "native x86_64 ABI");
_Static_assert(sizeof(struct nx_tms) == 32, "times ABI layout");
#endif
#endif
