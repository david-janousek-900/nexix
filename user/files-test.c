/* Native ring-3 file/pipe acceptance tests. Exit status identifies a failure. */
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <nexix/libc.h>
#define REQUIRE(c) do { if (!(c)) return __LINE__ % 250 + 1; } while (0)
static char payload[12000], buffer[12000];
extern int stdio_test(void);
static int child_wait(pid_t child, int code)
{
    int status = -1;
    return wait(&status) == child && WIFEXITED(status) && WEXITSTATUS(status) == code;
}
static int files(void)
{
    struct stat st;
    char byte;
    REQUIRE(open("/hello", O_RDONLY) == 0);
    REQUIRE(read(0, &byte, 1) == 1 && byte == 'H');
    REQUIRE(dup(0) == 1 && read(1, &byte, 1) == 1 && byte == 'e');
    REQUIRE(lseek(1, 0, SEEK_CUR) == 2);
    REQUIRE(read(0, (void *)1, 1) == -1 && errno == EFAULT);
    REQUIRE(lseek(0, 0, SEEK_CUR) == 2);
    REQUIRE(write(0, "x", 1) == -1 && errno == EBADF);
    REQUIRE(lseek(0, -3, SEEK_CUR) == -1 && errno == EINVAL);
    REQUIRE(open("/hello", O_WRONLY) == 2);
    REQUIRE(dup2(0, 2) == 2 && read(2, &byte, 1) == 1 && byte == 'l');
    REQUIRE(dup2(2, 2) == 2);
    REQUIRE(fstat(2, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 30);
    REQUIRE(fstat(2, (void *)1) == -1 && errno == EFAULT);
    pid_t child = fork(); REQUIRE(child >= 0);
    if (!child) _exit(read(1, &byte, 1) == 1 && byte == 'l' ? 0 : 1);
    REQUIRE(child_wait(child, 0));
    REQUIRE(read(0, &byte, 1) == 1 && byte == 'o');
    REQUIRE(close(0) == 0 && close(1) == 0 && close(2) == 0);
    REQUIRE(close(0) == -1 && errno == EBADF);
    REQUIRE(open("/dev/console", O_RDWR) == 0 && isatty(0) && close(0) == 0);
    REQUIRE(open("/", O_WRONLY) == -1 && errno == EISDIR);
    REQUIRE(open("/hello", O_RDONLY | O_TRUNC) == -1 && errno == EINVAL);
    REQUIRE(nx_syscall6(NX_SYS_OPEN, (long)(uintptr_t)"/hello", 1UL << 40, 0, 0, 0, 0) == -EINVAL);
    REQUIRE(chdir("/tmp") == 0);
    REQUIRE(open("new", O_CREAT | O_EXCL | O_RDWR, 0666) == 0);
    REQUIRE(fstat(0, &st) == 0 && (st.st_mode & 0777) == 0644 && st.st_uid == 0);
    REQUIRE(open("new", O_CREAT | O_EXCL | O_RDWR, 0666) == -1 && errno == EEXIST);
    for (unsigned i = 0; i < sizeof(payload); ++i) payload[i] = (char)(i * 17 + 3);
    REQUIRE(write(0, payload, sizeof(payload)) == sizeof(payload));
    REQUIRE(lseek(0, 0, SEEK_SET) == 0);
    REQUIRE(read(0, buffer, sizeof(buffer)) == sizeof(buffer) && !memcmp(payload, buffer, sizeof(buffer)));
    REQUIRE(read(0, &byte, 1) == 0);
    REQUIRE(dup(0) == 1);
    REQUIRE(fcntl(1, F_SETFL, O_APPEND) == 0 && fcntl(0, F_GETFL) == (O_RDWR | O_APPEND));
    REQUIRE(lseek(1, 0, SEEK_SET) == 0 && write(0, "!", 1) == 1);
    REQUIRE(fstat(1, &st) == 0 && st.st_size == sizeof(payload) + 1);
    REQUIRE(link("new", "alias") == 0);
    REQUIRE(stat("alias", &st) == 0 && st.st_nlink == 2);
    REQUIRE(unlink("new") == 0 && unlink("alias") == 0);
    REQUIRE(stat("new", &st) == -1 && errno == ENOENT);
    REQUIRE(fstat(0, &st) == 0 && st.st_nlink == 0);
    REQUIRE(lseek(1, 0, SEEK_SET) == 0 && read(1, &byte, 1) == 1 && byte == payload[0]);
    REQUIRE(close(0) == 0 && close(1) == 0);
    REQUIRE(creat("new", 0600) == 0 && write(0, "abc", 3) == 3 && close(0) == 0);
    REQUIRE(open("new", O_RDWR | O_TRUNC) == 0);
    REQUIRE(fstat(0, &st) == 0 && st.st_size == 0);
    REQUIRE(close(0) == 0 && unlink("new") == 0);
    REQUIRE(chdir("/") == 0);
    REQUIRE(stat("/", &st) == 0);
    unsigned links = st.st_nlink;
    REQUIRE(link("/", "/tmp/root") == 0);
    REQUIRE(stat("/tmp/root", &st) == 0 && st.st_nlink == links + 1);
    REQUIRE(unlink("/tmp/root") == 0 && stat("/", &st) == 0 && st.st_nlink == links);
    REQUIRE(unlink("/tmp/missing") == -1 && errno == ENOENT);
    REQUIRE(chdir("/hello") == -1 && errno == ENOTDIR);
    /* One free descriptor: pipe must leave its output and resources intact. */
    REQUIRE(open("/hello", O_RDONLY) == 0);
    for (int i = 1; i < 19; ++i) REQUIRE(dup(0) == i);
    int pair[2] = {-9, -9};
    REQUIRE(pipe(pair) == -1 && errno == EMFILE && pair[0] == -9 && pair[1] == -9);
    REQUIRE(dup(0) == 19 && dup(0) == -1 && errno == EMFILE);
    for (int i = 0; i < 20; ++i) REQUIRE(close(i) == 0);
    return 0;
}
static int pipes(void)
{
    int pair[2]; char byte; struct stat st;
    volatile uintptr_t bad = 1;
    REQUIRE(pipe((void *)bad) == -1 && errno == EFAULT);
    REQUIRE(pipe(pair) == 0 && pair[0] == 0 && pair[1] == 1);
    REQUIRE(fstat(0, &st) == 0 && S_ISFIFO(st.st_mode));
    REQUIRE(lseek(0, 0, SEEK_SET) == -1 && errno == ESPIPE);
    REQUIRE(write(1, "ok", 2) == 2);
    REQUIRE(read(0, (void *)1, 2) == -1 && errno == EFAULT);
    REQUIRE(read(0, buffer, 2) == 2 && !memcmp(buffer, "ok", 2));
    REQUIRE(close(0) == 0 && write(1, "x", 1) == -1 && errno == EPIPE && close(1) == 0);
    REQUIRE(pipe(pair) == 0);
    pid_t child = fork(); REQUIRE(child >= 0);
    if (!child) {
        if (close(0) || write(1, payload, sizeof(payload)) != sizeof(payload)) _exit(1);
        /* Exit closes the last writer and wakes the reader for EOF. */
        _exit(0);
    }
    REQUIRE(close(1) == 0);
    unsigned done = 0;
    while (done < sizeof(buffer)) {
        ssize_t n = read(0, buffer + done, sizeof(buffer) - done);
        REQUIRE(n > 0); done += n;
    }
    REQUIRE(!memcmp(buffer, payload, sizeof(buffer)));
    REQUIRE(read(0, &byte, 1) == 0 && close(0) == 0 && child_wait(child, 0));
    /* A writer sleeping on a full pipe must wake when its final reader exits. */
    REQUIRE(pipe(pair) == 0);
    REQUIRE(write(1, payload, PIPE_BUF) == PIPE_BUF);
    child = fork(); REQUIRE(child >= 0);
    if (!child) { close(0); _exit(write(1, "x", 1) == -1 && errno == EPIPE ? 0 : 1); }
    REQUIRE(close(1) == 0);
    long gate = 0x400;
    __asm__ volatile ("int $0x81" : "+a"(gate) : "D"((long)child) : "memory", "cc");
    REQUIRE(gate == 0);
    REQUIRE(close(0) == 0 && child_wait(child, 0));
    return 0;
}
static int inheritance(void)
{
    char byte;
    REQUIRE(chdir("/tmp") == 0);
    REQUIRE(open("../hello", O_RDONLY) == 0);
    REQUIRE(dup(0) == 1 && fcntl(1, F_SETFD, FD_CLOEXEC) == 0);
    REQUIRE(fcntl(0, F_GETFD) == 0 && fcntl(1, F_GETFD) == FD_CLOEXEC);
    REQUIRE(execv("/missing", (char *const[]){"missing", 0}) == -1 && errno == ENOENT);
    REQUIRE(fcntl(1, F_GETFD) == FD_CLOEXEC);
    pid_t child = fork(); REQUIRE(child >= 0);
    if (!child) { execv("/bin/files-test", (char *const[]){"files-test", "exec", 0}); _exit(1); }
    REQUIRE(child_wait(child, 42));
    REQUIRE(read(0, &byte, 1) == 1 && byte == 'e');
    /* Parent's descriptor flags and cwd survived the child's exec. */
    REQUIRE(fcntl(1, F_GETFD) == FD_CLOEXEC);
    REQUIRE(close(0) == 0 && close(1) == 0);
    REQUIRE(open("../hello", O_RDONLY | O_CLOEXEC) == 0 && fcntl(0, F_GETFD) == FD_CLOEXEC);
    /* Leave this file and cwd referenced: exit must release both. */
    return 0;
}
int main(int argc, char **argv)
{
    REQUIRE(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
    if (argc == 2 && !strcmp(argv[1], "permissions")) {
        struct stat st;
        REQUIRE(open("/hello", O_WRONLY) == -1 && errno == EACCES);
        REQUIRE(open("/dev/console", O_RDWR) == -1 && errno == EACCES);
        REQUIRE(open("/tmp/private", O_CREAT | O_WRONLY, 0600) == -1 && errno == EACCES);
        REQUIRE(unlink("/hello") == -1 && errno == EACCES);
        REQUIRE(link("/hello", "/tmp/private") == -1 && errno == EACCES);
        REQUIRE(open("/hello", O_RDONLY) == 0 && close(0) == 0);
        REQUIRE(chdir("/tmp") == 0 && stat("../hello", &st) == 0);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "stdio-exec")) return 0;
    if (argc == 2 && !strcmp(argv[1], "exec")) {
        char byte;
        REQUIRE(fcntl(1, F_GETFD) == -1 && errno == EBADF);
        REQUIRE(read(0, &byte, 1) == 1 && byte == 'H');
        REQUIRE(open("../hello", O_RDONLY) == 1);
        return 42;
    }
    int error = files(); if (error) return error;
    error=stdio_test();if(error)return error;
    error = pipes(); if (error) return error;
    return inheritance();
}
