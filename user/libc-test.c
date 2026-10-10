#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <nexix/libc.h>
#define CHECK(c) do { if (!(c)) _exit(__LINE__); } while (0)
extern int libc_register_test(void);
extern int libc_alignment_test(void);
static int exit_order;
static int zeroed[1024];
static void first(void) { CHECK(exit_order == 1); _exit(42); }
static void second(void) { CHECK(exit_order == 0); exit_order = 1; }
static void forbidden(void) { _exit(98); }
static void strings(void)
{
    char b[32];
    CHECK(memset(b, 0xa5, sizeof(b)) == b);
    CHECK((unsigned char)b[31] == 0xa5);
    CHECK(strcpy(b, "abcdef") == b && strlen(b) == 6);
    CHECK(memmove(b + 1, b, 7) == b + 1 && !strcmp(b, "aabcdef"));
    CHECK(memmove(b, b + 1, 7) == b && !strcmp(b, "abcdef"));
    CHECK(memmove(b, b, 7) == b && !strcmp(b, "abcdef"));
    CHECK(memcpy(b + 16, b, 7) == b + 16 && !memcmp(b, b + 16, 7));
    CHECK(memcmp("\200", "\177", 1) > 0 && strcmp("\200", "\177") > 0);
    CHECK(!strncmp("abc", "abcd", 3) && strncmp("abc", "abd", 3) < 0);
    CHECK(!strncmp("abc", "abd", 0));
    CHECK(strchr(b, 'c') == b + 2 && strchr(b, 0) == b + 6 && !strchr(b, 'x'));
    CHECK(strrchr("ababa", 'a') && !strcmp(strrchr("ababa", 'a'), "a"));
    CHECK(strrchr(b, 0) == b + 6 && !strrchr(b, 'x'));
    CHECK(strncpy(b, "xy", 5) == b && b[0] == 'x' && b[1] == 'y');
    CHECK(b[2] == 0 && b[3] == 0 && b[4] == 0);
    CHECK(strncpy(b, "abcdef", 3) == b && !memcmp(b, "abc", 3));
    CHECK(memset(b, 1, 0) == b && b[0] == 'a');
}
int main(int argc, char **argv, char **envp)
{
    CHECK(libc_alignment_test() && libc_register_test());
    CHECK(errno == 0);
    if (!argc) {
        CHECK(argv[0] == NULL && envp == environ && environ[0] == NULL);
        CHECK(!getenv("NEXIX_TEST"));
        return 0;
    }
    if (argc == 64) {
        CHECK(envp == environ && argv[64] == NULL && envp[64] == NULL);
        for (int i = 0; i < 64; ++i) {
            CHECK(!strcmp(argv[i], "/bin/libc-test"));
            CHECK(!strcmp(envp[i], "NEXIX_TEST=ok"));
        }
        return 0;
    }
    CHECK(argc == 2 && argv[2] == NULL && !strcmp(argv[0], "/bin/libc-test"));
    CHECK(envp == environ && environ[0] && environ[1] == NULL);
    for (unsigned i = 0; i < sizeof(zeroed) / sizeof(*zeroed); ++i) CHECK(zeroed[i] == 0);
    CHECK(errno == 0);
    CHECK(getenv("NEXIX_TEST") != NULL && !getenv("NEXIX") && !getenv(""));
    CHECK(!getenv("NEXIX_TEST=ok") && !getenv("missing"));
    if (!strcmp(argv[1], "replacement")) {
        CHECK(!strcmp(getenv("NEXIX_TEST"), "changed"));
        CHECK(atexit(first) == 0 && atexit(second) == 0);
        return 7; /* crt0 must call exit, execute handlers in reverse order. */
    }
    if (!strcmp(argv[1], "execv")) {
        CHECK(!strcmp(getenv("NEXIX_TEST"), "ok"));
        CHECK(umask(022) == 077);
        return 43; /* ordinary return from main becomes the exit status. */
    }
    CHECK(!strcmp(argv[1], "start") && !strcmp(getenv("NEXIX_TEST"), "ok"));
    strings();
    char **saved_environment = environ;
    environ = NULL; CHECK(!getenv("NEXIX_TEST")); environ = saved_environment;
    pid_t pid = getpid();
    CHECK(pid > 0 && getppid() == 0);
    CHECK(getuid() == 123 && geteuid() == 321 && getgid() == 654 && getegid() == 456);
    CHECK(nx_syscall6(NX_SYS_GETPID, 0x1111222233334444L, 0x2222333344445555L,
        0x3333444455556666L, 0x4444555566667777L, 0x5555666677778888L,
        0x6666777788889999L) == pid);
    long gate = 0x300;
    __asm__ volatile ("int $0x81" : "+a"(gate) : : "memory", "cc");
    CHECK(gate == 0);
    errno = EDOM;
    CHECK(nx_syscall6(0x100000002L, 0, 0, 0, 0, 0, 0) == -ENOSYS && errno == EDOM);
    CHECK(times(NULL) == -1 && errno == EFAULT);
    struct tms ticks;
    CHECK(times(&ticks) >= 0 && ticks.tms_cutime == 0 && ticks.tms_cstime == 0);
    CHECK(errno == EFAULT); /* Success does not clear errno. */
    CHECK(wait(NULL) == -1 && errno == ECHILD);
    CHECK(execve("/missing", argv, environ) == -1 && errno == ENOENT);
    volatile uintptr_t invalid_vector = 1;
    CHECK(execve("/bin/libc-test", (char *const *)invalid_vector, environ) == -1 && errno == EFAULT);
    CHECK(umask(077) == 022 && sched_yield() == 0);
    errno = EDOM;
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        CHECK(getppid() == pid && getpid() != pid && errno == EDOM);
        errno = ERANGE;
        zeroed[0] = 99;
        CHECK(atexit(forbidden) == 0); /* exec replaces callback state. */
        char *newargv[] = {argv[0], "replacement", NULL};
        char *newenv[] = {"NEXIX_TEST=changed", NULL};
        execve(argv[0], newargv, newenv);
        _exit(99);
    }
    int status;
    CHECK(wait(&status) == child && WIFEXITED(status) && WEXITSTATUS(status) == 42);
    CHECK(errno == EDOM && zeroed[0] == 0 && getpid() == pid);
    child = fork();
    CHECK(child >= 0);
    if (!child) {
        char *newargv[] = {argv[0], "execv", NULL};
        execv(argv[0], newargv);
        _exit(99);
    }
    CHECK(wait(&status) == child && WIFEXITED(status) && WEXITSTATUS(status) == 43);
    child = fork();
    CHECK(child >= 0);
    if (!child) {
        for (int i = 0; i < 32; ++i) CHECK(atexit(forbidden) == 0);
        CHECK(atexit(forbidden) != 0);
        _exit(44); /* _exit bypasses all 32 handlers. */
    }
    CHECK(wait(&status) == child && WIFEXITED(status) && WEXITSTATUS(status) == 44);
    CHECK(wait(NULL) == -1 && errno == ECHILD);
    return 0;
}
