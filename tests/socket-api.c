/* Live socket descriptors on a normal boot (task 425), run as root through
 * the private guest harness. Loopback only: the first socket starts the stack
 * with lo0 at 127.0.0.1. */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#define CHECK(c) do { if (!(c)) { printf("socket-api failure line %d errno %d\n", __LINE__, errno); exit(1); } } while (0)
#define FAILS(call, e) (errno = 0, (call) == -1 && errno == (e))
#define BYTES 200000

static volatile sig_atomic_t pipes, alarms;
static void on_pipe(int n) { (void)n; ++pipes; signal(SIGPIPE, on_pipe); }
static void on_alarm(int n) { (void)n; ++alarms; signal(SIGALRM, on_alarm); }
static unsigned char pattern(size_t i) { return (unsigned char)(i * 31 + 7); }

static struct sockaddr_in loopback(unsigned port)
{
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return a;
}
/* A listener on an ephemeral loopback port; *port receives it. */
static int listener(unsigned *port, int backlog)
{
    int l = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(l >= 0);
    struct sockaddr_in a = loopback(0);
    CHECK(bind(l, (struct sockaddr *)&a, sizeof(a)) == 0 && listen(l, backlog) == 0);
    socklen_t length = sizeof(a);
    CHECK(getsockname(l, (struct sockaddr *)&a, &length) == 0 && length == sizeof(a) && a.sin_family == AF_INET);
    CHECK(a.sin_addr.s_addr == htonl(INADDR_LOOPBACK) && ntohs(a.sin_port) >= 49152);
    *port = ntohs(a.sin_port);
    return l;
}
/* A connected pair over loopback: *client and the accepted *server. */
static void pair(int *client, int *server)
{
    unsigned port;
    int l = listener(&port, 1);
    *client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CHECK(*client >= 0);
    struct sockaddr_in a = loopback(port), peer;
    CHECK(connect(*client, (struct sockaddr *)&a, sizeof(a)) == 0);
    socklen_t length = sizeof(peer);
    *server = accept(l, (struct sockaddr *)&peer, &length);
    CHECK(*server >= 0 && length == sizeof(peer) && peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    struct sockaddr_in name;
    length = sizeof(name);
    CHECK(getsockname(*client, (struct sockaddr *)&name, &length) == 0 && name.sin_port == peer.sin_port);
    length = sizeof(name);
    CHECK(getpeername(*client, (struct sockaddr *)&name, &length) == 0 && ntohs(name.sin_port) == port);
    CHECK(close(l) == 0);
}
static size_t read_all(int fd, unsigned char *buffer, size_t max)
{
    size_t total = 0;
    for (;;) {
        ssize_t n = read(fd, buffer + total, max - total);
        CHECK(n >= 0);
        if (!n) return total;
        total += (size_t)n;
        CHECK(total <= max);
    }
}

static void transfer(void)
{
    /* Heap, not BSS: limits() forks before this exists, and forks copy eagerly. */
    static unsigned char *out, *in;
    if (!out) { out = malloc(BYTES); in = malloc(BYTES); CHECK(out && in); }
    for (size_t i = 0; i < BYTES; ++i) out[i] = pattern(i);
    int c, s;
    pair(&c, &s);
    /* The writer is a child sharing the descriptor; its exit closes its copy. */
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        CHECK(close(s) == 0);
        size_t done = 0;
        while (done < BYTES) {   /* partial writes are allowed; keep going */
            ssize_t n = write(c, out + done, BYTES - done);
            CHECK(n > 0);
            done += (size_t)n;
        }
        _exit(0);
    }
    CHECK(close(c) == 0);   /* the child's copy keeps the connection open */
    int status;
    size_t got = read_all(s, in, BYTES);   /* EOF when the child's final close sends FIN */
    CHECK(got == BYTES && !memcmp(in, out, BYTES));
    CHECK(wait(&status) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    CHECK(read(s, in, 1) == 0 && close(s) == 0);
}

static void half_close_and_pipe(void)
{
    int c, s;
    unsigned char b[16];
    pair(&c, &s);
    /* dup: closing one copy changes nothing. */
    int d = dup(c);
    CHECK(d >= 0 && close(c) == 0);
    CHECK(write(d, "hello", 5) == 5 && shutdown(d, SHUT_WR) == 0);
    CHECK(read_all(s, b, sizeof(b)) == 5 && !memcmp(b, "hello", 5));   /* then end of stream */
    CHECK(write(s, "back", 4) == 4 && read(d, b, sizeof(b)) == 4 && !memcmp(b, "back", 4));   /* half-closed */
    /* Writing after shutdown is EPIPE with SIGPIPE, caught here... */
    pipes = 0;
    CHECK(signal(SIGPIPE, on_pipe) != SIG_ERR);
    CHECK(FAILS(write(d, "x", 1), EPIPE) && pipes == 1);
    CHECK(signal(SIGPIPE, SIG_DFL) != SIG_ERR);
    /* ...and fatal by default. */
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) { (void)write(d, "x", 1); _exit(0); }
    int status;
    CHECK(wait(&status) == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGPIPE);
    CHECK(shutdown(s, SHUT_WR) == 0 && read(d, b, 1) == 0 && close(d) == 0 && close(s) == 0);

    /* Closing with unread data resets the peer: ECONNRESET once, then EPIPE. */
    pair(&c, &s);
    CHECK(write(c, "unread", 6) == 6);
    sleep(1);
    CHECK(close(s) == 0);
    CHECK(FAILS(read(c, b, sizeof(b)), ECONNRESET));
    pipes = 0;
    CHECK(signal(SIGPIPE, on_pipe) != SIG_ERR);
    CHECK(FAILS(write(c, "y", 1), EPIPE) && pipes == 1 && read(c, b, 1) == 0);
    CHECK(signal(SIGPIPE, SIG_DFL) != SIG_ERR && close(c) == 0);
}

static void exec_and_exit(const char *self)
{
    int c, s;
    pair(&c, &s);
    /* Close-on-exec closes the socket in the new image; otherwise it survives. */
    CHECK(fcntl(c, F_SETFD, FD_CLOEXEC) == 0 && fcntl(s, F_GETFD) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        char cfd[8], sfd[8];
        snprintf(cfd, sizeof(cfd), "%d", c);
        snprintf(sfd, sizeof(sfd), "%d", s);
        execl(self, self, "exec", cfd, sfd, (char *)NULL);
        _exit(99);
    }
    int status;
    CHECK(wait(&status) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    unsigned char b[8];
    CHECK(read(c, b, sizeof(b)) == 4 && !memcmp(b, "exec", 4));
    CHECK(close(c) == 0 && close(s) == 0);

    /* Process exit closes every socket: the peer sees end of stream. */
    pair(&c, &s);
    child = fork();
    CHECK(child >= 0);
    if (!child) _exit(0);   /* exits holding both copies */
    CHECK(wait(&status) == child && close(c) == 0);
    CHECK(read(s, b, 1) == 0 && close(s) == 0);
}
static int exec_child(int c, int s)
{
    struct stat st;
    if (fstat(c, &st) != -1 || errno != EBADF) return 2;           /* closed by exec */
    if (fstat(s, &st) != 0 || !S_ISSOCK(st.st_mode)) return 3;     /* kept */
    return write(s, "exec", 4) == 4 ? 0 : 4;
}

static void limits(void)
{
    /* EMFILE: the per-process table. */
    int fds[20], n = 0;
    while (n < 20) {
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) break;
        fds[n++] = fd;
    }
    CHECK(errno == EMFILE && n >= 15);
    for (int i = 0; i < n; ++i) CHECK(close(fds[i]) == 0);
    /* ENOBUFS: the 32 TCP control blocks, shared by all processes. Children
     * hold sockets until told to exit; every round finds the same total, so
     * exits and closes return everything. */
    int previous = -1;
    for (int round = 0; round < 3; ++round) {
        int report[2], hold[2];
        CHECK(pipe(report) == 0 && pipe(hold) == 0);
        pid_t children[3];
        for (int k = 0; k < 3; ++k) {
            children[k] = fork();
            CHECK(children[k] >= 0);
            if (!children[k]) {
                close(report[0]); close(hold[1]);
                unsigned char count = 0, error = 0;
                for (;;) {
                    int fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (fd < 0) { error = (unsigned char)errno; break; }
                    ++count;
                }
                unsigned char message[2] = {count, error};
                (void)write(report[1], message, 2);
                char go;
                (void)read(hold[0], &go, 1);   /* EOF once the parent closes it */
                _exit(0);
            }
        }
        close(report[1]); close(hold[0]);
        int total = 0, buffers = 0;
        for (int k = 0; k < 3; ++k) {
            unsigned char message[2];
            CHECK(read(report[0], message, 2) == 2);
            total += message[0];
            buffers += message[1] == ENOBUFS;
            CHECK(message[1] == ENOBUFS || message[1] == EMFILE);
        }
        CHECK(buffers >= 1 && total == 32);
        close(hold[1]);
        for (int k = 0; k < 3; ++k) { int status; CHECK(wait(&status) > 0 && WIFEXITED(status) && !WEXITSTATUS(status)); }
        close(report[0]);
        CHECK(previous < 0 || previous == total);
        previous = total;
    }
}

static void interrupts(void)
{
    unsigned port;
    int l = listener(&port, 1);
    alarms = 0;
    CHECK(signal(SIGALRM, on_alarm) != SIG_ERR);
    alarm(1);
    CHECK(FAILS(accept(l, NULL, NULL), EINTR) && alarms == 1);
    int c, s;
    pair(&c, &s);
    alarm(1);
    unsigned char b[4];
    CHECK(FAILS(read(s, b, sizeof(b)), EINTR) && alarms == 2);
    CHECK(close(c) == 0 && close(s) == 0 && close(l) == 0);
    CHECK(signal(SIGALRM, SIG_DFL) != SIG_ERR);
}

static void errors(void)
{
    struct sockaddr_in a = loopback(1);
    int t = socket(AF_INET, SOCK_STREAM, 0), u = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(t >= 0 && u >= 0);
    unsigned char b[4];
    CHECK(FAILS(read(t, b, 1), ENOTCONN) && FAILS(write(t, b, 1), ENOTCONN));
    CHECK(FAILS(getpeername(t, (struct sockaddr *)&a, &(socklen_t){sizeof(a)}), ENOTCONN));
    CHECK(FAILS(accept(t, NULL, NULL), EINVAL) && FAILS(shutdown(t, SHUT_RDWR), ENOTCONN));
    CHECK(FAILS(connect(t, (struct sockaddr *)&a, sizeof(a)), ECONNREFUSED));   /* nothing listens on port 1 */
    CHECK(FAILS(connect(t, (struct sockaddr *)&a, sizeof(a)), EISCONN) || errno == EINVAL);
    CHECK(close(t) == 0);
    /* Address errors. */
    t = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in other = loopback(0);
    other.sin_addr.s_addr = inet_addr("10.9.9.9");
    CHECK(FAILS(bind(t, (struct sockaddr *)&other, sizeof(other)), EADDRNOTAVAIL));
    unsigned port;
    int l = listener(&port, 2);
    a = loopback(port);
    CHECK(FAILS(bind(t, (struct sockaddr *)&a, sizeof(a)), EADDRINUSE));
    CHECK(FAILS(listen(u, 1), EOPNOTSUPP) && listen(l, 5) == 0);   /* re-listen changes the backlog */
    struct sockaddr_in low = loopback(7);
    CHECK(bind(t, (struct sockaddr *)&low, sizeof(low)) == 0);      /* root may use a reserved port */
    CHECK(FAILS(bind(t, (struct sockaddr *)&a, sizeof(a)), EINVAL));  /* already bound */
    /* Descriptor behaviour of a socket. */
    struct stat st;
    CHECK(fstat(u, &st) == 0 && S_ISSOCK(st.st_mode) && FAILS(lseek(u, 0, SEEK_SET), ESPIPE));
    CHECK(FAILS(ioctl(u, 0x7408, NULL), ENOTTY) && FAILS(read(u, b, 1), EOPNOTSUPP));
    /* Task 426's calls: arguments are checked, then unavailable. */
    int one = 1;
    CHECK(FAILS(send(t, b, 1, 0), ENOSYS) && FAILS(recv(u, b, 1, 0), ENOSYS));
    CHECK(FAILS(setsockopt(t, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)), ENOSYS));
    CHECK(FAILS(send(t, b, 1, 1), EOPNOTSUPP));
    /* Datagram connect and names; AF_UNSPEC disconnects. */
    CHECK(connect(u, (struct sockaddr *)&a, sizeof(a)) == 0);
    struct sockaddr_in name;
    socklen_t length = sizeof(name);
    CHECK(getpeername(u, (struct sockaddr *)&name, &length) == 0 && ntohs(name.sin_port) == port);
    struct sockaddr_in unspec = a;
    unspec.sin_family = AF_UNSPEC;
    CHECK(connect(u, (struct sockaddr *)&unspec, sizeof(unspec)) == 0);
    length = sizeof(name);
    CHECK(FAILS(getpeername(u, (struct sockaddr *)&name, &length), ENOTCONN));
    length = 2;   /* a short buffer is truncated; the full length is reported */
    CHECK(getsockname(u, (struct sockaddr *)&name, &length) == 0 && length == sizeof(name));
    /* Raw ICMP sockets are root's; reserved ports too. */
    int r = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    CHECK(r >= 0 && close(r) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        if (setuid(42) != 0) _exit(2);
        if (!FAILS(socket(AF_INET, SOCK_RAW, IPPROTO_ICMP), EACCES)) _exit(3);
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in reserved = loopback(9);
        if (fd < 0 || !FAILS(bind(fd, (struct sockaddr *)&reserved, sizeof(reserved)), EACCES)) _exit(4);
        _exit(0);
    }
    int status;
    CHECK(wait(&status) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(close(t) == 0 && close(u) == 0 && close(l) == 0);
}

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "exec")) return exec_child(atoi(argv[2]), atoi(argv[3]));
    limits();     /* first: closed connections linger in TIME_WAIT holding blocks */
    transfer();
    half_close_and_pipe();
    exec_and_exit(argv[0]);
    errors();
    interrupts();
    transfer();   /* everything still works after all of the above */
    puts("SOCKET_API_OK");
    return 0;
}
