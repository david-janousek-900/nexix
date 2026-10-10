/* Native sgtty/device acceptance tests. Test-only int 0x81 injects input in
 * normal boots; the uart mode receives every byte through QEMU's real COM1. */
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sgtty.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#define REQUIRE(c) do { if (!(c)) return __LINE__ % 250 + 1; } while (0)
static char buffer[512];
static volatile sig_atomic_t terminal_signal;
static void terminal_handler(int n) { terminal_signal = n; }
static long gate(long command, uintptr_t a, uintptr_t b)
{
    __asm__ volatile ("int $0x81" : "+a"(command) : "D"(a), "S"(b) : "memory", "cc");
    return command;
}
static void feed(const void *bytes, size_t count)
{ if (gate(0x500, (uintptr_t)bytes, count)) _exit(250); }
static int collect(pid_t child, int code)
{
    int status;
    return wait(&status) == child && WIFEXITED(status) && WEXITSTATUS(status) == code;
}
static int uart(void)
{
    struct sgttyb settings;
    REQUIRE(open("/dev/console", O_RDWR) == 0 && dup(0) == 1);
    const char cooked_prompt[] = "NEXIX_TTY_UART_COOKED_READY\n";
    REQUIRE(write(1, cooked_prompt, sizeof(cooked_prompt) - 1) == sizeof(cooked_prompt) - 1);
    REQUIRE(read(0, buffer, sizeof(buffer)) == 6 && !memcmp(buffer, "hello\n", 6));
    REQUIRE(gtty(0, &settings) == 0);
    settings.sg_flags = RAW;
    REQUIRE(stty(0, &settings) == 0);
    const char raw_prompt[] = "NEXIX_TTY_UART_RAW_READY\r\n";
    REQUIRE(write(1, raw_prompt, sizeof(raw_prompt) - 1) == sizeof(raw_prompt) - 1);
    size_t done = 0;
    while (done < 4) { ssize_t n = read(0, buffer + done, 4 - done); REQUIRE(n > 0); done += n; }
    const unsigned char raw[] = {0, 0x80, 0xff, 'R'};
    REQUIRE(!memcmp(buffer, raw, sizeof(raw)));
    settings.sg_flags = CBREAK | CRMOD;
    REQUIRE(stty(0, &settings) == 0);
    const char cbreak_prompt[] = "NEXIX_TTY_UART_CBREAK_READY\n";
    REQUIRE(write(1, cbreak_prompt, sizeof(cbreak_prompt) - 1) == sizeof(cbreak_prompt) - 1);
    REQUIRE(read(0, buffer, 1) == 1 && buffer[0] == 'c');
    /* Wait for the child to sleep before asking the harness for controls.
     * Both parent and child receive the controlling-terminal group signal. */
    for (unsigned pass = 0; pass < 2; ++pass) {
        int n = pass ? SIGQUIT : SIGINT;
        terminal_signal = 0;
        REQUIRE(signal(n, terminal_handler) == SIG_DFL);
        pid_t target = fork(); REQUIRE(target >= 0);
        if (!target) _exit(read(0, buffer, 1) == -1 && errno == EINTR && terminal_signal == n ? 0 : 1);
        REQUIRE(gate(pass ? 0x505 : 0x504, target, 0) == 0);
        int status; pid_t reaped;
        do { reaped = wait(&status); } while (reaped == -1 && errno == EINTR);
        REQUIRE(reaped == target && WIFEXITED(status) && WEXITSTATUS(status) == 0 && terminal_signal == n);
    }
    const char signal_ok[] = "NEXIX_TTY_UART_SIGNALS_OK\n";
    REQUIRE(write(1, signal_ok, sizeof(signal_ok) - 1) == sizeof(signal_ok) - 1);
    const char stop_prompt[] = "NEXIX_TTY_UART_STOP_READY\n";
    REQUIRE(write(1, stop_prompt, sizeof(stop_prompt) - 1) == sizeof(stop_prompt) - 1);
    REQUIRE(read(0, buffer, 1) == 1 && buffer[0] == 's');
    pid_t child = fork(); REQUIRE(child >= 0);
    if (!child) {
        const char resumed[] = "NEXIX_TTY_UART_RESUMED\n";
        _exit(write(1, resumed, sizeof(resumed) - 1) == sizeof(resumed) - 1 ? 0 : 1);
    }
    REQUIRE(gate(0x503, child, 0) == 0 && collect(child, 0));
    const char ok[] = "NEXIX_TTY_UART_USER_OK\n";
    REQUIRE(write(1, ok, sizeof(ok) - 1) == sizeof(ok) - 1);
    return 0;
}
static int demo(void)
{
    REQUIRE(open("/dev/console", O_RDWR) == 0 && dup(0) == 1 && dup(0) == 2);
    const char intro[] = "Nexix: serial TTY demo; type a line, or quit to finish.\nNEXIX_TTY_DEMO_READY\n";
    REQUIRE(write(1, intro, sizeof(intro) - 1) == sizeof(intro) - 1);
    for (;;) {
        ssize_t n = read(0, buffer, sizeof(buffer)); REQUIRE(n >= 0);
        if (!n) break;
        if (n == 5 && !memcmp(buffer, "quit\n", 5)) break;
        REQUIRE(write(1, "received: ", 10) == 10 && write(1, buffer, n) == n);
    }
    return 0;
}
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "uart")) return uart();
    if (argc == 2 && !strcmp(argv[1], "demo")) return demo();
    if (argc == 2 && !strcmp(argv[1], "exec")) {
        REQUIRE(isatty(0) && !isatty(1) && errno == EBADF);
        REQUIRE(open("/dev/tty", O_RDWR) == 1 && isatty(1));
        return 42;
    }
    struct sgttyb settings, saved;
    struct tchars chars;
    struct stat st;
    REQUIRE(open("/dev/tty", O_RDWR) == -1 && errno == ENXIO);
    REQUIRE(open("/dev/console", O_RDWR) == 0);
    REQUIRE(gate(0x502, 0, 0) == 1);
    REQUIRE(isatty(0) && fstat(0, &st) == 0 && (st.st_mode & S_IFMT) == S_IFCHR && st.st_rdev == 0x100);
    REQUIRE(lseek(0, 0, SEEK_SET) == -1 && errno == ESPIPE);
    REQUIRE(dup(0) == 1 && dup2(0, 2) == 2 && gate(0x502, 0, 0) == 1);
    REQUIRE(open("/dev/ttyS0", O_RDONLY) == 3 && gate(0x502, 0, 0) == 2);
    REQUIRE(write(3, "x", 1) == -1 && errno == EBADF);
    REQUIRE(close(3) == 0);
    REQUIRE(open("/hello", O_RDONLY) == 3 && !isatty(3) && errno == ENOTTY);
    REQUIRE(ioctl(3, FIOCLEX, 0) == 0 && fcntl(3, F_GETFD) == FD_CLOEXEC);
    REQUIRE(ioctl(3, FIONCLEX, 0) == 0 && fcntl(3, F_GETFD) == 0 && close(3) == 0);
    REQUIRE(gtty(0, &saved) == 0 && saved.sg_ispeed == B115200);
    settings = saved; settings.sg_flags &= ~ECHO;
    REQUIRE(stty(0, &settings) == 0);
    REQUIRE(ioctl(0, TIOCGETC, &chars) == 0 && chars.t_eofc == 4);
    int discipline = -1;
    REQUIRE(ioctl(0, TIOCGETD, &discipline) == 0 && discipline == 0);
    discipline = 1;
    REQUIRE(ioctl(0, TIOCSETD, &discipline) == -1 && errno == ENXIO);
    REQUIRE(ioctl(0, 0xdead, 0) == -1 && errno == ENOTTY);
    feed("ab\bcd\r", 6);
    REQUIRE(read(0, (void *)1, 4) == -1 && errno == EFAULT);
    REQUIRE(read(0, buffer, sizeof(buffer)) == 4 && !memcmp(buffer, "acd\n", 4));
    feed("one\ntwo\n", 8);
    REQUIRE(read(0, buffer, sizeof(buffer)) == 4 && !memcmp(buffer, "one\n", 4));
    REQUIRE(read(0, buffer, 2) == 2 && !memcmp(buffer, "tw", 2));
    REQUIRE(read(0, buffer, sizeof(buffer)) == 2 && !memcmp(buffer, "o\n", 2));
    feed("x\025ok\n", 5);
    REQUIRE(read(0, buffer, sizeof(buffer)) == 3 && !memcmp(buffer, "ok\n", 3));
    feed("\004xy\004", 4);
    REQUIRE(read(0, buffer, sizeof(buffer)) == 0);
    REQUIRE(read(0, buffer, sizeof(buffer)) == 2 && !memcmp(buffer, "xy", 2));
    feed("stay\n", 5);
    REQUIRE(stty(0, (void *)1) == -1 && errno == EFAULT);
    settings.sg_flags |= 0x80000000U;
    REQUIRE(stty(0, &settings) == -1 && errno == EINVAL);
    settings.sg_flags &= ~0x80000000U;
    REQUIRE(read(0, buffer, sizeof(buffer)) == 5 && !memcmp(buffer, "stay\n", 5));
    settings.sg_erase = '#'; settings.sg_kill = '@';
    REQUIRE(ioctl(0, TIOCSETN, &settings) == 0);
    feed("ab#c\ndrop@ok\n", 13);
    REQUIRE(read(0, buffer, sizeof(buffer)) == 3 && !memcmp(buffer, "ac\n", 3));
    REQUIRE(read(0, buffer, sizeof(buffer)) == 3 && !memcmp(buffer, "ok\n", 3));
    feed("old\n", 4);
    REQUIRE(stty(0, &settings) == 0); /* successful SETP flushes */
    feed("new\n", 4);
    REQUIRE(read(0, buffer, sizeof(buffer)) == 4 && !memcmp(buffer, "new\n", 4));
    settings.sg_flags = CBREAK | CRMOD;
    REQUIRE(stty(0, &settings) == 0);
    feed("x\004", 2);
    REQUIRE(read(0, buffer, sizeof(buffer)) == 2 && buffer[0] == 'x' && buffer[1] == 4);
    settings.sg_flags = RAW;
    REQUIRE(stty(0, &settings) == 0);
    const unsigned char raw[] = {0, 0x80, 0xff, 4, 17, 19, '\r'};
    feed(raw, sizeof(raw));
    REQUIRE(read(0, buffer, sizeof(buffer)) == sizeof(raw) && !memcmp(buffer, raw, sizeof(raw)));
    REQUIRE(stty(0, &saved) == 0);
    settings = saved; settings.sg_flags &= ~ECHO;
    REQUIRE(stty(0, &settings) == 0);
    pid_t child = fork(); REQUIRE(child >= 0);
    if (!child) _exit(read(0, buffer, sizeof(buffer)) == 5 && !memcmp(buffer, "wake\n", 5) ? 0 : 1);
    REQUIRE(gate(0x501, child, 0) == 0);
    feed("wake\r", 5);
    REQUIRE(collect(child, 0));
    REQUIRE(fcntl(1, F_SETFD, FD_CLOEXEC) == 0 && close(2) == 0);
    child = fork(); REQUIRE(child >= 0);
    if (!child) { execv("/bin/tty-test", (char *const[]){"tty-test", "exec", 0}); _exit(1); }
    REQUIRE(collect(child, 42) && gate(0x502, 0, 0) == 1);
    const char marker[] = "NEXIX_TTY_USER_WRITE_OK\n";
    REQUIRE(write(0, marker, sizeof(marker) - 1) == sizeof(marker) - 1);
    /* Exit releases the dup'd terminal description and controlling-tty pointer. */
    return 0;
}
