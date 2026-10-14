#ifndef NEXIX_ABI_SYSCALL_H
#define NEXIX_ABI_SYSCALL_H
/* Native x86_64 ABI, version 2 (signal FP context). Shared with libc. */
#define NX_ABI_VERSION 2
#define NX_SYS_EXIT 1
#define NX_SYS_FORK 2
#define NX_SYS_READ 3
#define NX_SYS_WRITE 4
#define NX_SYS_OPEN 5
#define NX_SYS_CLOSE 6
#define NX_SYS_WAIT 7
#define NX_SYS_CREAT 8
#define NX_SYS_LINK 9
#define NX_SYS_UNLINK 10
#define NX_SYS_EXEC 11
#define NX_SYS_CHDIR 12
#define NX_SYS_TIME 13
#define NX_SYS_MKNOD 14
#define NX_SYS_CHMOD 15
#define NX_SYS_CHOWN 16
#define NX_SYS_BRK 17
#define NX_SYS_MOUNT 21
#define NX_SYS_SETUID 23
#define NX_SYS_STIME 25
#define NX_SYS_ALARM 27
#define NX_SYS_PAUSE 29
#define NX_SYS_UTIME 30
#define NX_SYS_ACCESS 33
#define NX_SYS_NICE 34
#define NX_SYS_SYNC 36
#define NX_SYS_KILL 37
#define NX_SYS_SETPGRP 39
#define NX_SYS_SETGID 46
#define NX_SYS_SIGNAL 48
#define NX_SYS_STAT 18
#define NX_SYS_LSEEK 19
#define NX_SYS_GETPID 20
#define NX_SYS_GETUID 24
#define NX_SYS_FSTAT 28
#define NX_SYS_STTY 31
#define NX_SYS_GTTY 32
#define NX_SYS_DUP 41
#define NX_SYS_PIPE 42
#define NX_SYS_TIMES 43
#define NX_SYS_GETGID 47
#define NX_SYS_IOCTL 54
#define NX_SYS_EXECVE 59
#define NX_SYS_UMASK 60
#define NX_SYS_CHROOT 61
/* Native extensions beyond the original 64-entry V7 table. */
#define NX_SYS_YIELD 64
#define NX_SYS_GETPPID 65
#define NX_SYS_GETEUID 66
#define NX_SYS_GETEGID 67
#define NX_SYS_DUP2 68
#define NX_SYS_FCNTL 69
#define NX_SYS_SIGRETURN 70
#define NX_SYS_GETPGRP 71
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
