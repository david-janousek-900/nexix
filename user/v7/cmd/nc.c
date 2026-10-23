/* Native numeric-address netcat (task 428): one TCP or UDP connection,
 * as a client or a one-shot listener. With no select, a child copies
 * standard input to the socket while the parent copies the socket to
 * standard output. */
#include "../netconfig.h"
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
static pid_t child;
static int usage(void)
{
    fputs("usage: nc [-u] [-w seconds] address port\n"
          "       nc -l [-u] [-w seconds] port\n", stderr);
    return 2;
}
/* A signal stops the copier too, then ends us the same way. */
static void forward(int n)
{
    if (child > 0) kill(child, SIGTERM);
    signal(n, SIG_DFL);
    kill(getpid(), n);
}
static int port_number(const char *text, unsigned short *out)
{
    char *end;
    long value = strtol(text, &end, 10);
    if (end == text || *end || value < 1 || value > 65535) {
        fprintf(stderr, "nc: bad port %s\n", text);
        return -1;
    }
    *out = htons((unsigned short)value);
    return 0;
}
static int timed_out(const char *what)
{
    fprintf(stderr, "nc: %s: timed out\n", what);
    return 1;
}
/* Standard input to the socket: each read is one write, or one datagram. */
static void copy_in(int fd, int stream)
{
    char b[1024];
    for (;;) {
        ssize_t n = read(0, b, sizeof(b));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        for (ssize_t done = 0; done < n;) {
            ssize_t w = write(fd, b + done, (size_t)(n - done));
            if (w < 0) { if (errno == EINTR) continue; _exit(1); }
            done += w;
        }
    }
    if (stream) shutdown(fd, SHUT_WR);   /* end of input: half-close */
    _exit(0);
}
/* The socket to standard output, until end of stream, or -w of silence. */
static int copy_out(int fd)
{
    char b[9216];
    for (;;) {
        ssize_t n = read(fd, b, sizeof(b));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && errno == EAGAIN) return 0;   /* idle timeout */
        if (n < 0) { netconfig_fail("nc", "read"); return 1; }
        if (!n) return 0;
        for (ssize_t done = 0; done < n;) {
            ssize_t w = write(1, b + done, (size_t)(n - done));
            if (w <= 0) return 1;
            done += w;
        }
    }
}
static int relay(int fd, int stream)
{
    child = fork();
    if (child < 0) { netconfig_fail("nc", "fork"); return 1; }
    if (!child) copy_in(fd, stream);
    int status = copy_out(fd);
    /* The network side has ended. Finite input (a file or pipe) is still
     * sent in full; an interactive terminal's copier is stopped. A UDP
     * timeout always stops it: there is nobody to send to. */
    if (!stream || isatty(0)) kill(child, SIGTERM);
    int ignored;
    while (wait(&ignored) != child && errno == EINTR) {}
    return status;
}
int main(int argc, char **argv)
{
    int listen_mode = 0, udp = 0, i = 1;
    long wait_seconds = 0;
    for (; i < argc && argv[i][0] == '-'; ++i) {
        if (!strcmp(argv[i], "-l")) listen_mode = 1;
        else if (!strcmp(argv[i], "-u")) udp = 1;
        else if (!strcmp(argv[i], "-w") && i + 1 < argc) {
            char *end;
            wait_seconds = strtol(argv[++i], &end, 10);
            if (*end || wait_seconds < 1 || wait_seconds > 3600) return usage();
        } else return usage();
    }
    if (argc - i != (listen_mode ? 1 : 2)) return usage();
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    if (!listen_mode && inet_pton(AF_INET, argv[i], &a.sin_addr) != 1) {
        fprintf(stderr, "nc: bad address %s\n", argv[i]);
        return 2;
    }
    if (port_number(argv[argc - 1], &a.sin_port) < 0) return 2;
    signal(SIGINT, forward);
    signal(SIGTERM, forward);
    signal(SIGHUP, forward);
    struct timeval wait = { wait_seconds, 0 };
    int fd = socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0);
    if (fd < 0) { netconfig_fail("nc", "socket"); return 1; }
    if (wait_seconds && (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait)) < 0 ||
                         setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &wait, sizeof(wait)) < 0)) {
        netconfig_fail("nc", "setsockopt");
        return 1;
    }
    if (!listen_mode) {
        if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0)
            return errno == EINPROGRESS ? timed_out("connect") : (netconfig_fail("nc", "connect"), 1);
        return relay(fd, !udp);
    }
    int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0 ||
        bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
        netconfig_fail("nc", "bind");
        return 1;
    }
    if (udp) {
        /* The first datagram's sender becomes the peer. */
        char b[9216];
        struct sockaddr_in peer;
        socklen_t length = sizeof(peer);
        ssize_t n;
        while ((n = recvfrom(fd, b, sizeof(b), 0, (struct sockaddr *)&peer, &length)) < 0 && errno == EINTR) {}
        if (n < 0) return errno == EAGAIN ? timed_out("recvfrom") : (netconfig_fail("nc", "recvfrom"), 1);
        if (write(1, b, (size_t)n) != n || connect(fd, (struct sockaddr *)&peer, sizeof(peer)) < 0) return 1;
        return relay(fd, 0);
    }
    if (listen(fd, 1) < 0) { netconfig_fail("nc", "listen"); return 1; }
    int c;
    while ((c = accept(fd, NULL, NULL)) < 0 && errno == EINTR) {}
    if (c < 0) return errno == EAGAIN ? timed_out("accept") : (netconfig_fail("nc", "accept"), 1);
    close(fd);
    if (wait_seconds) setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait));
    return relay(c, 1);
}
