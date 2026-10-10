/* Ring-3 acceptance: real trap delivery, blocking I/O, faults and V7 services. */
#include <signal.h>
#include <setjmp.h>
#include <sgtty.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <utime.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <nexix/libc.h>
#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)
static volatile sig_atomic_t caught, number;
static void handler(int n) { ++caught; number = n; }
static int status;
static jmp_buf jump;
extern int signal_register_test(void);
static void jump_handler(int n) { longjmp(jump, n); }
static void nested(int n) { (void)n; raise(SIGINT); }
static int reap(pid_t pid, int code)
{ return wait(&status) == pid && WIFEXITED(status) && WEXITSTATUS(status) == code; }
static int death(pid_t pid, int n)
{ return wait(&status) == pid && WIFSIGNALED(status) && WTERMSIG(status) == n; }
static int signals(void)
{
    CHECK(signal(SIGKILL, handler) == SIG_ERR && errno == EINVAL);
    CHECK(signal(0, handler) == SIG_ERR && errno == EINVAL);
    CHECK(signal(SIGTERM, SIG_IGN) != SIG_ERR);
    CHECK(raise(SIGTERM) == 0 && caught == 0);
    CHECK(signal(SIGTERM, handler) == SIG_IGN);
    CHECK(raise(SIGTERM) == 0 && caught == 1 && number == SIGTERM);
    CHECK(signal(SIGTERM, SIG_IGN) == SIG_DFL); /* One-shot V7 disposition. */
    CHECK(kill(getpid(), 0) == 0 && kill(29999, 0) == -1 && errno == ESRCH);
    CHECK(kill(getpid(), NSIG) == -1 && errno == EINVAL);
    CHECK(nx_syscall6(NX_SYS_SIGRETURN, 1, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(nx_syscall6(NX_SYS_SIGNAL, SIGTERM, (long)(uintptr_t)handler, 1, 0, 0, 0) == -EFAULT);
    CHECK(nx_syscall6(NX_SYS_SIGNAL, 1UL << 32, 0, 0, 0, 0, 0) == -EINVAL);
    struct nx_signal_context bad = {.rip = (uintptr_t)handler, .rsp = (uintptr_t)&status + sizeof(status), .cs = 0x23, .ss = 0x1b};
    const unsigned forbidden[] = {0, 0x3202, 0x4202, 0x20202};
    for (unsigned i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); ++i) {
        bad.rflags = forbidden[i];
        CHECK(nx_syscall6(NX_SYS_SIGRETURN, (long)(uintptr_t)&bad, 0, 0, 0, 0, 0) == -EINVAL);
    }
    bad.rflags = 0x202; bad.cs = 8;
    CHECK(nx_syscall6(NX_SYS_SIGRETURN, (long)(uintptr_t)&bad, 0, 0, 0, 0, 0) == -EINVAL);
    bad.cs = 0x23; bad.rsp = 1;
    CHECK(nx_syscall6(NX_SYS_SIGRETURN, (long)(uintptr_t)&bad, 0, 0, 0, 0, 0) == -EINVAL);
    bad.rsp = (uintptr_t)&status + sizeof(status); bad.rip = 1;
    CHECK(nx_syscall6(NX_SYS_SIGRETURN, (long)(uintptr_t)&bad, 0, 0, 0, 0, 0) == -EINVAL);
    CHECK(signal(SIGTERM, handler) == SIG_IGN && signal_register_test());
    CHECK(signal(SIGINT, handler) == SIG_DFL && signal(SIGTERM, nested) == SIG_DFL);
    CHECK(raise(SIGTERM) == 0 && number == SIGINT);
    int jumped = setjmp(jump);
    if (!jumped) { CHECK(signal(SIGTERM, jump_handler) == SIG_DFL); raise(SIGTERM); return __LINE__; }
    CHECK(jumped == SIGTERM);
    CHECK(signal(SIGTERM, handler) == SIG_DFL && raise(SIGTERM) == 0 && number == SIGTERM);
    CHECK(signal(SIGALRM, handler) == SIG_DFL);
    CHECK(alarm(2) == 0 && alarm(1) == 2);
    CHECK(pause() == -1 && errno == EINTR && number == SIGALRM);
    CHECK(alarm(0) == 0);
    pid_t pid = fork(); CHECK(pid >= 0);
    if (!pid) { signal(SIGTERM, SIG_DFL); raise(SIGTERM); _exit(99); }
    CHECK(death(pid, SIGTERM));
    pid = fork(); CHECK(pid >= 0);
    if (!pid) { __asm__ volatile ("ud2"); _exit(99); }
    CHECK(death(pid, SIGILL));
    pid = fork(); CHECK(pid >= 0);
    if (!pid) { __asm__ volatile ("movb $0, (%0)" : : "r"(1UL) : "memory"); _exit(99); }
    CHECK(death(pid, SIGSEGV));
    int pair[2], ready[2]; char byte;
    CHECK(pipe(pair) == 0 && pipe(ready) == 0);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        close(ready[0]); signal(SIGINT, handler); write(ready[1], "r", 1);
        _exit(read(pair[0], &byte, 1) == -1 && errno == EINTR && number == SIGINT ? 0 : 1);
    }
    close(ready[1]); CHECK(read(ready[0], &byte, 1) == 1);
    CHECK(kill(pid, SIGINT) == 0 && reap(pid, 0));
    close(ready[0]); close(pair[0]); close(pair[1]);
    CHECK(pipe(pair) == 0);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) { close(pair[0]); close(pair[1]); pause(); _exit(99); }
    CHECK(signal(SIGALRM, handler) != SIG_ERR && alarm(1) == 0);
    CHECK(wait(&status) == -1 && errno == EINTR && number == SIGALRM);
    CHECK(kill(pid, SIGKILL) == 0 && death(pid, SIGKILL));
    close(pair[0]); close(pair[1]);
    CHECK(pipe(pair) == 0 && close(pair[0]) == 0);
    CHECK(signal(SIGPIPE, handler) == SIG_DFL);
    CHECK(write(pair[1], "x", 1) == -1 && errno == EPIPE && number == SIGPIPE);
    CHECK(close(pair[1]) == 0);
    static char fill[PIPE_BUF];
    CHECK(pipe(pair) == 0 && write(pair[1], fill, sizeof(fill)) == sizeof(fill));
    CHECK(signal(SIGALRM, handler) != SIG_ERR && alarm(1) == 0);
    CHECK(write(pair[1], "x", 1) == -1 && errno == EINTR && number == SIGALRM);
    close(pair[0]); close(pair[1]);
    int tty = open("/dev/console", O_RDWR); CHECK(tty >= 0);
    CHECK(signal(SIGALRM, handler) != SIG_ERR && alarm(1) == 0);
    CHECK(read(tty, &byte, 1) == -1 && errno == EINTR && number == SIGALRM);
    CHECK(close(tty) == 0);
    CHECK(pipe(pair) == 0);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) { close(pair[0]); signal(SIGPIPE, SIG_DFL); write(pair[1], "x", 1); _exit(99); }
    close(pair[0]); close(pair[1]); CHECK(death(pid, SIGPIPE));
    CHECK(signal(SIGILL, handler) == SIG_DFL);
    CHECK(raise(SIGILL) == 0 && raise(SIGILL) == 0 && number == SIGILL);
    CHECK(signal(SIGILL, SIG_DFL) == handler); /* Persistent V7 ILL/TRAP handlers. */
    CHECK(signal(SIGALRM, handler) == SIG_DFL && alarm(10) == 0);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) _exit(alarm(0) == 0 && signal(SIGALRM, SIG_DFL) == handler ? 0 : 1);
    CHECK(reap(pid, 0) && alarm(0) > 0);
    CHECK(setpgrp() == getpid() && getpgrp() == getpid());
    CHECK(signal(SIGHUP, handler) == SIG_DFL && kill(0, SIGHUP) == 0 && number == SIGHUP);
    CHECK(signal(SIGHUP, handler) == SIG_DFL && kill(-getpgrp(), SIGHUP) == 0 && number == SIGHUP);
    return 0;
}
static int heap(void)
{
    unsigned char *base = sbrk(0); CHECK(base != (void *)-1);
    CHECK(sbrk(8193) == base);
    for (unsigned i = 0; i < 8193; ++i) CHECK(base[i] == 0);
    base[0] = 23; base[8192] = 42;
    pid_t pid = fork(); CHECK(pid >= 0);
    if (!pid) { base[0] = 99; _exit(sbrk(0) == base + 8193 && base[8192] == 42 ? 0 : 1); }
    CHECK(reap(pid, 0) && base[0] == 23);
    CHECK(sbrk(INTPTR_MAX) == (void *)-1 && errno == ENOMEM && sbrk(0) == base + 8193);
    /* Exhaust physical pages; kernel must roll back all newly mapped pages. */
    CHECK(sbrk(256 * 1024 * 1024) == (void *)-1 && errno == ENOMEM);
    CHECK(sbrk(0) == base + 8193 && base[0] == 23 && base[8192] == 42);
    CHECK(sbrk(-8193) == base + 8193 && sbrk(0) == base);
    CHECK(sbrk(-1) == (void *)-1 && errno == ENOMEM);
    CHECK(sbrk(-(intptr_t)base) == (void *)-1 && errno == ENOMEM && sbrk(0) == base);
    CHECK(brk(0) == -1 && errno == ENOMEM);
    CHECK(sbrk(4096) == base && base[0] == 0 && sbrk(-4096) == base + 4096);
    return 0;
}
static int services(int ro)
{
    time_t now, set = 123456;
    CHECK(time(&now) >= 0 && now == time(0));
    CHECK(time((void *)1) == -1 && errno == EFAULT);
    CHECK(stime(&set) == 0 && time(0) >= set && time(0) <= set + 1);
    CHECK(access("/hello", R_OK) == 0 && access("/missing", F_OK) == -1 && errno == ENOENT);
    struct stat st; struct utimbuf times = {100, 200};
    if (ro) {
        CHECK(chmod("/hello", 0600) == -1 && errno == EROFS);
        CHECK(chown("/hello", 123, 456) == -1 && errno == EROFS);
        CHECK(access("/hello", W_OK) == -1 && errno == EROFS);
        CHECK(stat("/hello", &st) == 0 && (st.st_mode & 0777) == 0644 && st.st_uid == 0);
    } else {
        CHECK(chmod("/hello", 0640) == 0 && chown("/hello", 123, 456) == 0);
        CHECK(utime("/hello", &times) == 0);
        CHECK(stat("/hello", &st) == 0 && (st.st_mode & 0777) == 0640 && st.st_uid == 123 && st.st_gid == 456);
        CHECK(st.st_atime == 100 && st.st_mtime == 200);
    }
    sync();
    pid_t pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        if (setgid(456) || setuid(123)) _exit(1);
        if (getuid() != 123 || geteuid() != 123 || getgid() != 456 || getegid() != 456) _exit(2);
        if (setuid(0) != -1 || errno != EPERM || stime(&set) != -1 || errno != EPERM) _exit(3);
        if (kill(getppid(), 0) != -1 || errno != EPERM) _exit(4);
        if (access("/hello", R_OK) || nice(1) || nice(-1) != -1 || errno != EPERM) _exit(5);
        if (chown("/hello", 0, 0) != -1 || errno != EPERM) _exit(6);
        _exit(0);
    }
    CHECK(reap(pid, 0));
    return 0;
}
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "real")) {
        CHECK(getuid() == 123 && geteuid() == 0);
        CHECK(access("/hello", R_OK) == -1 && errno == EACCES);
        int fd = open("/hello", O_RDONLY); CHECK(fd >= 0 && close(fd) == 0);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "exec")) {
        CHECK(signal(SIGTERM, SIG_IGN) == SIG_IGN);
        CHECK(signal(SIGALRM, SIG_IGN) == SIG_DFL);
        CHECK(alarm(0) > 0);
        CHECK(sbrk(0) != (void *)-1);
        return 0;
    }
    pid_t pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        signal(SIGTERM, SIG_IGN); signal(SIGALRM, handler); alarm(10);
        char *args[] = {"/bin/signals-test", "exec", 0};
        execv(args[0], args); _exit(99);
    }
    CHECK(reap(pid, 0));
    int error = signals(); if (error) return error;
    error = heap(); if (error) return error;
    return services(argc == 2 && !strcmp(argv[1], "ro"));
}
