#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <nexix/libc.h>
int errno;
char **environ;
_Static_assert(sizeof(long) == 8 && sizeof(int) == 4, "Nexix LP64 syscall ABI");
static long result(long value)
{
    if (value < 0) { errno = (int)-value; return -1; }
    return value;
}
static long call(long number, long a1, long a2, long a3)
{ return result(nx_syscall6(number, a1, a2, a3, 0, 0, 0)); }
pid_t fork(void) { return call(NX_SYS_FORK, 0, 0, 0); }
pid_t wait(int *status) { return call(NX_SYS_WAIT, (long)(uintptr_t)status, 0, 0); }
pid_t getpid(void) { return call(NX_SYS_GETPID, 0, 0, 0); }
pid_t getppid(void) { return call(NX_SYS_GETPPID, 0, 0, 0); }
uid_t getuid(void) { return call(NX_SYS_GETUID, 0, 0, 0); }
uid_t geteuid(void) { return call(NX_SYS_GETEUID, 0, 0, 0); }
gid_t getgid(void) { return call(NX_SYS_GETGID, 0, 0, 0); }
gid_t getegid(void) { return call(NX_SYS_GETEGID, 0, 0, 0); }
mode_t umask(mode_t mask) { return call(NX_SYS_UMASK, mask, 0, 0); }
clock_t times(struct tms *buffer) { return call(NX_SYS_TIMES, (long)(uintptr_t)buffer, 0, 0); }
int sched_yield(void) { return call(NX_SYS_YIELD, 0, 0, 0); }
int execve(const char *path, char *const argv[], char *const envp[])
{ return call(NX_SYS_EXECVE, (long)(uintptr_t)path, (long)(uintptr_t)argv, (long)(uintptr_t)envp); }
int execv(const char *path, char *const argv[]) { return execve(path, argv, environ); }
_Noreturn void _exit(int status)
{
    (void)nx_syscall6(NX_SYS_EXIT, status, 0, 0, 0, 0, 0);
    __asm__ volatile ("ud2");
    __builtin_unreachable();
}
