#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <setjmp.h>
#include <errno.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <nexix/libc.h>
#define CHECK(c) do { if (!(c)) _exit(__LINE__); } while (0)
extern int libc_register_test(void);
extern int libc_alignment_test(void);
extern int fpu_process_test(void);
extern int libc_scan_test(void);
extern int libc_helpers_test(void);
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
static void numbers(void)
{
    int exponent;
    double integer;
    CHECK(frexp(8., &exponent) == .5 && exponent == 4);
    CHECK(frexp(0x1p-1074, &exponent) == .5 && exponent == -1073);
    CHECK(ldexp(.5, -1073) == 0x1p-1074);
    CHECK(modf(-3.25, &integer) == -.25 && integer == -3.);
    CHECK(modf(INFINITY, &integer) == 0 && integer == INFINITY);
    CHECK(fmod(5.5, 2.) == 1.5 && fmod(-5.5, 2.) == -1.5);
    CHECK(fabs(sqrt(2.) - 1.4142135623730951) < 1e-14);
    CHECK(fabs(exp(1.) - 2.718281828459045) < 1e-14);
    CHECK(fabs(log(2.) - .6931471805599453) < 1e-14);
    CHECK(fabs(sin(.5) - .479425538604203) < 1e-14);
    CHECK(fabs(cos(.5) - .8775825618903728) < 1e-14);
    CHECK(fabs(tan(.5) - .5463024898437905) < 1e-14);
    CHECK(fabs(atan(1.) - .7853981633974483) < 1e-14);
    CHECK(fabs(asin(.5) - .5235987755982989) < 1e-14);
    CHECK(fabs(acos(.5) - 1.0471975511965979) < 1e-14);
    CHECK(fabs(pow(2., 10.) - 1024.) < 1e-10);
    CHECK(fabs(hypot(3., 4.) - 5.) < 1e-14);
    CHECK(fabs(sinh(1.) - 1.1752011936438014) < 1e-14);
    CHECK(fabs(cosh(1.) - 1.5430806348152437) < 1e-14);
    CHECK(fabs(tanh(1.) - .7615941559557649) < 1e-14);
    CHECK(floor(-1.25) == -2. && ceil(-1.25) == -1.);
    CHECK(fabs(j0(1.) - .7651976865579666) < 1e-14);
    CHECK(fabs(j1(1.) - .4400505857449335) < 1e-14);
    CHECK(fabs(jn(2, 1.) - .1149034849319005) < 1e-14);
    CHECK(fabs(y0(1.) - .08825696421567696) < 1e-14);
    CHECK(fabs(y1(1.) + .7812128213002887) < 1e-14);
    CHECK(fabs(yn(2, 1.) + 1.6506826068162543) < 1e-14);
    char *end;
    CHECK(strtod(" \t-12.5e2tail", &end) == -1250 && !strcmp(end, "tail"));
    CHECK(strtod("1e+tail", &end) == 1 && !strcmp(end, "e+tail"));
    const char *none = "  +.x";
    CHECK(strtod(none, &end) == 0 && end == none);
    CHECK(atof(".000125") == .000125);
    errno = 0;
    CHECK(strtod("1e999999", &end) == INFINITY && !*end && errno == ERANGE);
    CHECK(strtod("-1e-999999", &end) == 0 && !*end && errno == ERANGE);
    errno = 0; CHECK(sqrt(-1.) == 0 && errno == EDOM); /* V7 domain rule. */
    char output[128];
    CHECK(snprintf(output, sizeof(output), "%d %.2f %.2e %.6g", 7, 12.5, .125, 1.25) == 21);
    CHECK(!strcmp(output, "7 12.50 1.25e-01 1.25"));
    CHECK(snprintf(output, sizeof(output), "%.2f %.0f %.4g %.2e", -.009, 9.9, .00001, 0.) > 0);
    CHECK(!strcmp(output, "-0.01 10 1e-05 0.00e+00"));
    CHECK(snprintf(output, sizeof(output), "%.1e %.2f", 0x1p-1074, 1e300) > 300);
    CHECK(!strncmp(output, "4.9e-324 ", 9) && output[127] == 0);
    CHECK(snprintf(output, 5, "%.2f", 12.5) == 5 && !strcmp(output, "12.5"));
    CHECK(snprintf(output, sizeof(output), "%f %g", INFINITY, NAN) == 7 && !strcmp(output, "inf nan"));
    jmp_buf environment;
    unsigned short control;
    unsigned mxcsr;
    if (!setjmp(environment)) {
        control = 0xb7f; mxcsr = 0x5f80;
        __asm__ volatile ("fldcw %0; ldmxcsr %1" : : "m"(control), "m"(mxcsr));
        longjmp(environment, 1);
    }
    __asm__ volatile ("fnstcw %0; stmxcsr %1" : "=m"(control), "=m"(mxcsr));
    CHECK(control == 0x37f && (mxcsr & ~0x3fU) == 0x1f80);
}
int main(int argc, char **argv, char **envp)
{
    /* Fresh programs, including exec replacements, receive default controls. */
    unsigned short control;
    unsigned mxcsr;
    __asm__ volatile ("fnstcw %0; stmxcsr %1" : "=m"(control), "=m"(mxcsr));
    CHECK(control == 0x37f && mxcsr == 0x1f80);
    /* QEMU boot RTC must reach userspace through the native time syscall. */
    time_t boot_time;
    CHECK(time(&boot_time) >= 946684800);
    CHECK(time(0) >= boot_time && time(0) <= boot_time + 1);
    CHECK(libc_alignment_test() && libc_register_test());
    CHECK(errno == 0);
    if (!argc) {
        CHECK(argv[0] == NULL && envp == environ && environ[0] == NULL);
        CHECK(!getenv("NEXIX_TEST"));
        return 0;
    }
    if (argc == 8) {
        CHECK(envp == environ && envp[0] && envp[1] && envp[2] == NULL);
        CHECK(!strcmp(getenv("NEXIX_TEST"), "explicit") && !strcmp(getenv("EXTRA"), "stack"));
        CHECK(argv[8] == NULL && !strcmp(argv[0], "/bin/libc-test"));
        for (int i = 1; i < 8; ++i) CHECK(argv[i][0] == '0'+i && argv[i][1] == 0);
        return 45;
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
    CHECK(fpu_process_test());
    numbers(); errno = EFAULT;
    int scan_error=libc_scan_test();if(scan_error)_exit(scan_error);errno=EFAULT;
    int helper_error = libc_helpers_test();
    if (helper_error) _exit(helper_error);
    errno = EFAULT;
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
        control = 0xb7f; mxcsr = 0x5f80;
        __asm__ volatile ("fldcw %0; ldmxcsr %1" : : "m"(control), "m"(mxcsr));
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
    /* execle must find the environment beyond register and stack arguments. */
    char *explicit_env[] = {"NEXIX_TEST=explicit", "EXTRA=stack", NULL};
    CHECK(execle("/missing", argv[0], (char *)NULL, explicit_env) == -1 && errno == ENOENT);
    CHECK(!strcmp(getenv("NEXIX_TEST"), "ok"));
    child = fork(); CHECK(child >= 0);
    if (!child) {
        execle(argv[0], argv[0], "1", "2", "3", "4", "5", "6", "7", (char *)NULL, explicit_env);
        _exit(99);
    }
    CHECK(wait(&status) == child && WIFEXITED(status) && WEXITSTATUS(status) == 45);
#define ARG8 "/bin/libc-test", "/bin/libc-test", "/bin/libc-test", "/bin/libc-test", "/bin/libc-test", "/bin/libc-test", "/bin/libc-test", "/bin/libc-test"
#define ARG64 ARG8, ARG8, ARG8, ARG8, ARG8, ARG8, ARG8, ARG8
    char *many_env[65];
    for (int i=0; i<64; ++i) many_env[i]="NEXIX_TEST=ok";
    many_env[64]=NULL;
    CHECK(execle(argv[0], ARG64, argv[0], (char *)NULL, many_env)==-1 && errno==E2BIG);
    child=fork(); CHECK(child>=0);
    if (!child) { execle(argv[0], ARG64, (char *)NULL, many_env); _exit(99); }
    CHECK(wait(&status)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    child=fork(); CHECK(child>=0);
    if (!child) { execle(argv[0], (char *)NULL, (char **)NULL); _exit(99); }
    CHECK(wait(&status)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
#undef ARG64
#undef ARG8
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
