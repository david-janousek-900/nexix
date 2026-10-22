/* Live socket descriptors on a normal boot (tasks 425-426), run as root
 * through the private guest harness. Loopback only: the first socket starts
 * the stack with lo0 at 127.0.0.1. */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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
    CHECK(FAILS(ioctl(u, 0x7408, NULL), ENOTTY) && FAILS(recv(u, b, 1, MSG_DONTWAIT), EAGAIN));
    CHECK(FAILS(send(t, b, 1, 0), ENOTCONN) && FAILS(send(t, b, 1, 1), EOPNOTSUPP));
    CHECK(FAILS(write(u, b, 1), EDESTADDRREQ));
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


/* Task 426: datagram and raw I/O. */
static void datagrams(void)
{
    int a = socket(AF_INET, SOCK_DGRAM, 0), b = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK(a >= 0 && b >= 0);
    struct sockaddr_in any = loopback(0), name, from;
    CHECK(bind(b, (struct sockaddr *)&any, sizeof(any)) == 0);
    socklen_t length = sizeof(name);
    CHECK(getsockname(b, (struct sockaddr *)&name, &length) == 0);
    /* sendto binds an ephemeral port; recvfrom reports it. */
    unsigned char out[9216], in[9300];
    for (size_t i = 0; i < sizeof(out); ++i) out[i] = pattern(i);
    CHECK(sendto(a, out, 100, 0, (struct sockaddr *)&name, sizeof(name)) == 100);
    length = sizeof(from);
    CHECK(recvfrom(b, in, sizeof(in), 0, (struct sockaddr *)&from, &length) == 100 && !memcmp(in, out, 100));
    CHECK(length == sizeof(from) && from.sin_addr.s_addr == htonl(INADDR_LOOPBACK) && ntohs(from.sin_port) >= 49152);
    /* Datagram boundaries, peek, truncation, the 9216-byte limit and zero length. */
    CHECK(sendto(a, out, 10, 0, (struct sockaddr *)&name, sizeof(name)) == 10);
    CHECK(sendto(a, out, sizeof(out), 0, (struct sockaddr *)&name, sizeof(name)) == (ssize_t)sizeof(out));
    CHECK(sendto(a, out, 0, 0, (struct sockaddr *)&name, sizeof(name)) == 0);
    CHECK(FAILS(sendto(a, out, sizeof(out) + 1, 0, (struct sockaddr *)&name, sizeof(name)), EMSGSIZE));
    CHECK(recv(b, in, sizeof(in), MSG_PEEK) == 10 && recv(b, in, 4, 0) == 4 && !memcmp(in, out, 4));   /* rest discarded */
    CHECK(recv(b, in, sizeof(in), 0) == (ssize_t)sizeof(out) && !memcmp(in, out, sizeof(out)));
    CHECK(recv(b, in, sizeof(in), 0) == 0 && FAILS(recv(b, in, 1, MSG_DONTWAIT), EAGAIN));
    /* Connected: write/read work, other destinations are EISCONN, a closed
     * peer port comes back as ECONNREFUSED. */
    CHECK(connect(a, (struct sockaddr *)&name, sizeof(name)) == 0);
    CHECK(write(a, "hi", 2) == 2 && read(b, in, sizeof(in)) == 2 && !memcmp(in, "hi", 2));
    CHECK(FAILS(sendto(a, "x", 1, 0, (struct sockaddr *)&name, sizeof(name)), EISCONN));
    CHECK(close(b) == 0);
    CHECK(write(a, "lost", 4) == 4);
    sleep(1);   /* the ICMP port unreachable crosses lo0 */
    int error = 0;
    length = sizeof(error);
    CHECK(getsockopt(a, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == ECONNREFUSED);
    CHECK(getsockopt(a, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0);   /* read clears it */
    CHECK(write(a, "lost", 4) == 4);
    sleep(1);
    CHECK(FAILS(recv(a, in, 1, MSG_DONTWAIT), ECONNREFUSED));   /* or the next call reports it */
    /* Shutdown: no more sending (EPIPE, without a signal here); reads end. */
    CHECK(shutdown(a, SHUT_RDWR) == 0 && FAILS(send(a, "x", 1, MSG_NOSIGNAL), EPIPE) && recv(a, in, 1, 0) == 0);
    CHECK(close(a) == 0);
    /* Raw ICMP: an echo request to ourselves, answered by the kernel. */
    int r = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    CHECK(r >= 0);
    unsigned char echo[16] = {8, 0, 0, 0, 0x12, 0x34, 0, 1, 'n', 'e', 'x', 'i', 'x', 0, 0, 0};
    struct sockaddr_in self = loopback(0);
    CHECK(FAILS(send(r, echo, sizeof(echo), 0), EDESTADDRREQ) && FAILS(sendto(r, echo, 7, 0, (struct sockaddr *)&self, sizeof(self)), EINVAL));
    CHECK(sendto(r, echo, sizeof(echo), 0, (struct sockaddr *)&self, sizeof(self)) == sizeof(echo));
    struct timeval wait = {2, 0};
    CHECK(setsockopt(r, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait)) == 0);
    length = sizeof(from);
    ssize_t n = recvfrom(r, in, sizeof(in), 0, (struct sockaddr *)&from, &length);
    CHECK(n == 20 + (ssize_t)sizeof(echo) && in[20] == 0 && in[24] == 0x12 && in[27] == 1 && !memcmp(in + 28, "nexix", 5));
    CHECK(from.sin_addr.s_addr == htonl(INADDR_LOOPBACK) && from.sin_port == 0);
    CHECK(FAILS(recv(r, in, sizeof(in), 0), EAGAIN));   /* the 2 s timeout */
    CHECK(close(r) == 0);
}

static void options(void)
{
    int t = socket(AF_INET, SOCK_STREAM, 0), u = socket(AF_INET, SOCK_DGRAM, 0);
    int value;
    socklen_t length = sizeof(value);
    CHECK(getsockopt(t, SOL_SOCKET, SO_TYPE, &value, &length) == 0 && value == SOCK_STREAM && length == 4);
    CHECK(getsockopt(u, SOL_SOCKET, SO_TYPE, &value, &length) == 0 && value == SOCK_DGRAM);
    CHECK(getsockopt(t, SOL_SOCKET, SO_REUSEADDR, &value, &length) == 0 && value == 0);
    int one = 1, size = 4096;
    CHECK(setsockopt(t, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == 0);
    CHECK(getsockopt(t, SOL_SOCKET, SO_REUSEADDR, &value, &length) == 0 && value == 1);
    CHECK(getsockopt(t, SOL_SOCKET, SO_SNDBUF, &value, &length) == 0 && value == 16384);
    CHECK(setsockopt(t, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == 0);
    CHECK(getsockopt(t, SOL_SOCKET, SO_RCVBUF, &value, &length) == 0 && value == 4096);
    size = 1;
    CHECK(setsockopt(u, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == 0);   /* raised to the minimum */
    CHECK(getsockopt(u, SOL_SOCKET, SO_SNDBUF, &value, &length) == 0 && value == 512);
    size = 262145;
    CHECK(FAILS(setsockopt(t, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)), ENOBUFS));
    CHECK(getsockopt(t, IPPROTO_TCP, TCP_NODELAY, &value, &length) == 0 && value == 0);
    CHECK(setsockopt(t, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) == 0);
    CHECK(getsockopt(t, IPPROTO_TCP, TCP_NODELAY, &value, &length) == 0 && value == 1);
    CHECK(FAILS(setsockopt(u, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)), ENOPROTOOPT));
    struct timeval timeout = {1, 250000}, back;
    socklen_t tl = sizeof(back);
    CHECK(setsockopt(t, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0);
    CHECK(getsockopt(t, SOL_SOCKET, SO_SNDTIMEO, &back, &tl) == 0 && tl == sizeof(back) && back.tv_sec == 1 && back.tv_usec == 250000);
    timeout.tv_sec = 0; timeout.tv_usec = 1;   /* rounds up to one tick, never to "none" */
    CHECK(setsockopt(t, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
    CHECK(getsockopt(t, SOL_SOCKET, SO_RCVTIMEO, &back, &tl) == 0 && back.tv_sec == 0 && back.tv_usec == 10000);
    /* SO_REUSEADDR lets two sockets that both ask share a port. */
    int v = socket(AF_INET, SOCK_DGRAM, 0), w = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = loopback(0), name;
    CHECK(setsockopt(v, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == 0 && bind(v, (struct sockaddr *)&a, sizeof(a)) == 0);
    length = sizeof(name);
    CHECK(getsockname(v, (struct sockaddr *)&name, &length) == 0);
    CHECK(FAILS(bind(w, (struct sockaddr *)&name, sizeof(name)), EADDRINUSE));
    CHECK(setsockopt(w, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == 0 && bind(w, (struct sockaddr *)&name, sizeof(name)) == 0);
    CHECK(close(t) == 0 && close(u) == 0 && close(v) == 0 && close(w) == 0);
}

static void flags_and_blocking(void)
{
    int c, s;
    pair(&c, &s);
    unsigned char b[64];
    /* PEEK leaves data; WAITALL waits for all of it; DONTWAIT never waits. */
    CHECK(FAILS(recv(s, b, sizeof(b), MSG_DONTWAIT), EAGAIN));
    CHECK(send(c, "abcdef", 6, 0) == 6);
    CHECK(recv(s, b, sizeof(b), MSG_PEEK) == 6 && recv(s, b, 3, 0) == 3 && !memcmp(b, "abc", 3));
    CHECK(recv(s, b, sizeof(b), 0) == 3 && !memcmp(b, "def", 3));
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {   /* dribble 30 bytes in three writes */
        for (int i = 0; i < 3; ++i) { sleep(1); if (write(c, "0123456789", 10) != 10) _exit(1); }
        _exit(0);
    }
    CHECK(recv(s, b, 30, MSG_WAITALL) == 30 && !memcmp(b + 20, "0123456789", 10));
    int status;
    CHECK(wait(&status) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    /* O_NONBLOCK: reads, a full send buffer, accept and connect. */
    CHECK(fcntl(s, F_SETFL, O_NONBLOCK) == 0 && (fcntl(s, F_GETFL) & O_NONBLOCK));
    CHECK(FAILS(read(s, b, 1), EAGAIN));
    unsigned char *bulk = malloc(65536);   /* heap: keeps earlier forks and execs small */
    CHECK(bulk);
    CHECK(fcntl(c, F_SETFL, O_NONBLOCK) == 0);
    ssize_t total = 0, n;
    while ((n = write(c, bulk, 65536)) > 0) total += n;
    CHECK(n == -1 && errno == EAGAIN && total >= 16384);   /* our send buffer plus the peer's window */
    CHECK(fcntl(c, F_SETFL, 0) == 0);
    /* SO_SNDTIMEO: a blocked write gives up with a partial count or EAGAIN. */
    struct timeval timeout = {1, 0};
    CHECK(setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0);
    CHECK(FAILS(write(c, bulk, 65536), EAGAIN));
    CHECK(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 && fcntl(s, F_SETFL, 0) == 0);
    ssize_t drained = 0;
    while ((n = read(s, bulk, 65536)) > 0) drained += n;
    CHECK(n == -1 && errno == EAGAIN && drained == total);   /* receive timeout once empty */
    CHECK(close(c) == 0 && close(s) == 0);
    /* Pipes and other files do not take O_NONBLOCK. */
    int p[2];
    CHECK(pipe(p) == 0 && FAILS(fcntl(p[0], F_SETFL, O_NONBLOCK), EINVAL) && close(p[0]) == 0 && close(p[1]) == 0);

    /* Non-blocking accept and connect: EAGAIN, EINPROGRESS, then SO_ERROR. */
    unsigned port;
    int l = listener(&port, 1);
    CHECK(fcntl(l, F_SETFL, O_NONBLOCK) == 0 && FAILS(accept(l, NULL, NULL), EAGAIN));
    struct sockaddr_in a = loopback(port);
    c = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(fcntl(c, F_SETFL, O_NONBLOCK) == 0);
    /* Over loopback the handshake may finish before connect returns. */
    CHECK(connect(c, (struct sockaddr *)&a, sizeof(a)) == 0 || errno == EINPROGRESS);
    CHECK(FAILS(connect(c, (struct sockaddr *)&a, sizeof(a)), EALREADY) || errno == EISCONN);
    sleep(1);
    int error = -1;
    socklen_t length = sizeof(error);
    CHECK(getsockopt(c, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0);
    CHECK(FAILS(connect(c, (struct sockaddr *)&a, sizeof(a)), EISCONN));
    s = accept(l, NULL, NULL);
    CHECK(s >= 0 && close(s) == 0 && close(c) == 0 && close(l) == 0);
    /* A refused non-blocking connect reports through SO_ERROR. */
    c = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in closed = loopback(1);
    CHECK(fcntl(c, F_SETFL, O_NONBLOCK) == 0);
    errno = 0;
    CHECK(connect(c, (struct sockaddr *)&closed, sizeof(closed)) == -1);
    int immediate = errno;
    CHECK(immediate == EINPROGRESS || immediate == ECONNREFUSED);
    sleep(1);
    error = -1;
    CHECK(getsockopt(c, SOL_SOCKET, SO_ERROR, &error, &length) == 0);
    CHECK(error == (immediate == EINPROGRESS ? ECONNREFUSED : 0));   /* reported exactly once */
    CHECK(close(c) == 0);
    /* A blocking accept with SO_RCVTIMEO times out; alarm interrupts a send. */
    l = listener(&port, 1);
    CHECK(setsockopt(l, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 && FAILS(accept(l, NULL, NULL), EAGAIN));
    CHECK(close(l) == 0);
    pair(&c, &s);
    alarms = 0;
    CHECK(signal(SIGALRM, on_alarm) != SIG_ERR);
    /* Fill both buffers without blocking, then a blocking send that the
     * alarm interrupts before any byte moves. */
    CHECK(send(c, bulk, 65536, MSG_DONTWAIT) > 0);
    while (send(c, bulk, 65536, MSG_DONTWAIT) > 0) {}
    CHECK(errno == EAGAIN);
    alarm(1);
    CHECK(FAILS(send(c, bulk, 65536, 0), EINTR) && alarms == 1);
    CHECK(signal(SIGALRM, SIG_DFL) != SIG_ERR && close(c) == 0 && close(s) == 0);
}

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "exec")) return exec_child(atoi(argv[2]), atoi(argv[3]));
    limits();     /* first: closed connections linger in TIME_WAIT holding blocks */
    exec_and_exit(argv[0]);   /* before the large transfers grow the heap */
    transfer();
    half_close_and_pipe();
    errors();
    interrupts();
    datagrams();
    options();
    flags_and_blocking();
    transfer();   /* everything still works after all of the above */
    puts("SOCKET_API_OK");
    return 0;
}
