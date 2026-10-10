#include <unistd.h>
#include <fcntl.h>
#include <sgtty.h>
#include <stdarg.h>
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

_Static_assert(sizeof(struct stat) == sizeof(struct nx_stat), "stat ABI size");
_Static_assert(offsetof(struct stat, st_size) == offsetof(struct nx_stat, size), "stat ABI offset");
ssize_t read(int fd, void *buffer, size_t count)
{ return call(NX_SYS_READ, fd, (long)(uintptr_t)buffer, count); }
ssize_t write(int fd, const void *buffer, size_t count)
{ return call(NX_SYS_WRITE, fd, (long)(uintptr_t)buffer, count); }
int close(int fd) { return call(NX_SYS_CLOSE, fd, 0, 0); }
off_t lseek(int fd, off_t offset, int whence) { return call(NX_SYS_LSEEK, fd, offset, whence); }
int dup(int fd) { return call(NX_SYS_DUP, fd, 0, 0); }
int dup2(int fd, int target) { return call(NX_SYS_DUP2, fd, target, 0); }
int pipe(int pair[2]) { return call(NX_SYS_PIPE, (long)(uintptr_t)pair, 0, 0); }
int chdir(const char *path) { return call(NX_SYS_CHDIR, (long)(uintptr_t)path, 0, 0); }
int link(const char *oldpath, const char *newpath)
{ return call(NX_SYS_LINK, (long)(uintptr_t)oldpath, (long)(uintptr_t)newpath, 0); }
int unlink(const char *path) { return call(NX_SYS_UNLINK, (long)(uintptr_t)path, 0, 0); }
int stat(const char *path, struct stat *buffer)
{ return call(NX_SYS_STAT, (long)(uintptr_t)path, (long)(uintptr_t)buffer, 0); }
int fstat(int fd, struct stat *buffer) { return call(NX_SYS_FSTAT, fd, (long)(uintptr_t)buffer, 0); }
int creat(const char *path, mode_t mode) { return call(NX_SYS_CREAT, (long)(uintptr_t)path, mode, 0); }
int open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = va_arg(ap, int); va_end(ap); }
    return call(NX_SYS_OPEN, (long)(uintptr_t)path, flags, mode);
}
int fcntl(int fd, int command, ...)
{
    int value = 0;
    if (command == F_SETFD || command == F_SETFL) {
        va_list ap; va_start(ap, command); value = va_arg(ap, int); va_end(ap);
    }
    return call(NX_SYS_FCNTL, fd, command, value);
}

_Static_assert(sizeof(struct sgttyb) == sizeof(struct nx_sgttyb), "sgtty ABI size");
_Static_assert(offsetof(struct sgttyb, sg_flags) == offsetof(struct nx_sgttyb, flags), "sgtty ABI offset");
_Static_assert(sizeof(struct tchars) == sizeof(struct nx_tchars), "tchars ABI size");
int ioctl(int fd, unsigned long request, void *argument)
{ return call(NX_SYS_IOCTL, fd, request, (long)(uintptr_t)argument); }
int stty(int fd, const struct sgttyb *settings)
{ return call(NX_SYS_STTY, fd, (long)(uintptr_t)settings, 0); }
int gtty(int fd, struct sgttyb *settings)
{ return call(NX_SYS_GTTY, fd, (long)(uintptr_t)settings, 0); }
int isatty(int fd) { struct sgttyb settings; return gtty(fd, &settings) == 0; }

#include <signal.h>
#include <time.h>
#include <utime.h>
extern void __nx_signal_return(void);
sighandler_t signal(int n, sighandler_t handler)
{
    long value = nx_syscall6(NX_SYS_SIGNAL, n, (long)(uintptr_t)handler,
        (long)(uintptr_t)__nx_signal_return, 0, 0, 0);
    if (value < 0) { errno = -value; return SIG_ERR; }
    return (sighandler_t)(uintptr_t)value;
}
int kill(pid_t pid, int n) { return call(NX_SYS_KILL, pid, n, 0); }
int raise(int n) { return kill(getpid(), n); }
unsigned alarm(unsigned seconds) { return call(NX_SYS_ALARM, seconds, 0, 0); }
int pause(void) { return call(NX_SYS_PAUSE, 0, 0, 0); }
int setuid(uid_t id) { return call(NX_SYS_SETUID, id, 0, 0); }
int setgid(gid_t id) { return call(NX_SYS_SETGID, id, 0, 0); }
int nice(int increment) { return call(NX_SYS_NICE, increment, 0, 0); }
pid_t getpgrp(void) { return call(NX_SYS_GETPGRP, 0, 0, 0); }
pid_t setpgrp(void) { return call(NX_SYS_SETPGRP, 0, 0, 0); }
time_t time(time_t *out) { return call(NX_SYS_TIME, (long)(uintptr_t)out, 0, 0); }
int stime(const time_t *value) { return call(NX_SYS_STIME, (long)(uintptr_t)value, 0, 0); }
int access(const char *path, int mode) { return call(NX_SYS_ACCESS, (long)(uintptr_t)path, mode, 0); }
int chmod(const char *path, mode_t mode) { return call(NX_SYS_CHMOD, (long)(uintptr_t)path, mode, 0); }
int chown(const char *path, uid_t uid, gid_t gid) { return call(NX_SYS_CHOWN, (long)(uintptr_t)path, uid, gid); }
int utime(const char *path, const struct utimbuf *value)
{ return call(NX_SYS_UTIME, (long)(uintptr_t)path, (long)(uintptr_t)value, 0); }
void sync(void) { (void)call(NX_SYS_SYNC, 0, 0, 0); }
int brk(void *address)
{
    if (!address) { errno = ENOMEM; return -1; }
    return call(NX_SYS_BRK, (long)(uintptr_t)address, 0, 0);
}
void *sbrk(intptr_t increment)
{
    long old = call(NX_SYS_BRK, 0, 0, 0);
    if (old < 0) return (void *)-1;
    uintptr_t next;
    if (increment >= 0) {
        if ((uintptr_t)increment > UINTPTR_MAX - (uintptr_t)old) { errno = ENOMEM; return (void *)-1; }
        next = (uintptr_t)old + increment;
    } else {
        uintptr_t magnitude = 0 - (uintptr_t)increment;
        if (magnitude > (uintptr_t)old) { errno = ENOMEM; return (void *)-1; }
        next = (uintptr_t)old - magnitude;
    }
    if (!next) { errno = ENOMEM; return (void *)-1; }
    if (increment && call(NX_SYS_BRK, next, 0, 0) < 0) return (void *)-1;
    return (void *)(uintptr_t)old;
}
