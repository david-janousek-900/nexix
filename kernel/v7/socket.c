/* Socket syscall boundary. Task 424 decodes and validates every argument
 * before any descriptor is consulted, so malformed calls fail the same way
 * whatever the descriptor. Task 425 backs descriptors with socket objects,
 * connection setup and stream I/O; task 426 adds send/recv/sendto/recvfrom,
 * datagram and raw I/O, flags, options, timeouts and non-blocking use. */
#include "socket.h"
#include "process.h"
#include "accounting.h"
#include "uaccess.h"
#include "files.h"
#include "interrupts.h"
#include "signals.h"
#include "v7/core.h"
#include "net/socket.h"
#include <nexix/socket.h>
#include <nexix/file.h>
#include <nexix/signal.h>
#include <nexix/syscall.h>
#include <nexix/errno.h>
#define SOCKET_PRIORITY 30
#define CHUNK 1024

/* Sleep until a protocol event newer than seen: 0 to retry, -EINTR on a
 * signal, -EAGAIN once the deadline (0: none) has passed. The worker cannot
 * run between the caller's check and this sleep. */
static int wait_event(struct nx_socket *s, uint32_t seen, uint64_t deadline)
{
    if (s->events != seen) return 0;
    int error = proc_sleep_until(s, SOCKET_PRIORITY, deadline);
    return error < 0 ? -NX_EINTR : error > 0 ? -NX_EAGAIN : 0;
}
static uint64_t deadline_after(uint64_t ticks) { return ticks ? timer_ticks() + ticks : 0; }

static int64_t socket_create(struct user *u, int64_t domain, int64_t type, int64_t protocol)
{
    if (domain != NX_AF_INET) return -NX_EAFNOSUPPORT;
    switch (type) {
    case NX_SOCK_STREAM: if (protocol && protocol != NX_IPPROTO_TCP) return -NX_EPROTONOSUPPORT; break;
    case NX_SOCK_DGRAM: if (protocol && protocol != NX_IPPROTO_UDP) return -NX_EPROTONOSUPPORT; break;
    case NX_SOCK_RAW:
        if (protocol != NX_IPPROTO_ICMP) return -NX_EPROTONOSUPPORT;
        if (!proc_superuser(u)) return -NX_EACCES;   /* BSD rip_usrreq: raw sockets are root's */
        break;
    default: return -NX_EPROTOTYPE;
    }
    int error = files_socket_reserve(u);
    if (error < 0) return error;
    struct nx_socket *s;
    if ((error = net_socket_create((int)type, type == NX_SOCK_RAW, &s))) return error;
    int fd = files_socket_install(u, s);   /* tables may have filled while we slept */
    if (fd < 0) net_socket_release(s);
    return fd;
}

/* An input address: length within [16, 128], readable, AF_INET (or, where
 * allowed, AF_UNSPEC). */
static int64_t address_in(const struct address_space *vm, uintptr_t address, uint64_t length, bool unspec,
                          struct nx_sockaddr_in *out)
{
    if (length < sizeof(*out) || length > NX_SOCKADDR_MAX) return -NX_EINVAL;
    if (!address || !user_access_ok(vm, address, length, false)) return -NX_EFAULT;
    if (copyin(vm, address, out, sizeof(*out))) return -NX_EFAULT;
    if (out->family == NX_AF_INET || (unspec && out->family == NX_AF_UNSPEC)) return 0;
    return -NX_EAFNOSUPPORT;
}
/* An output address and its value-result length: the length must be
 * readable and writable, and the buffer writable for what it states (up to
 * one sockaddr_in, the most that is ever written). */
static int64_t address_out(const struct address_space *vm, uintptr_t address, uintptr_t length_pointer,
                           bool required)
{
    if (!address && !required) return 0;
    nx_socklen_t length;
    if (!address || !length_pointer || !user_access_ok(vm, length_pointer, sizeof(length), true) ||
        copyin(vm, length_pointer, &length, sizeof(length)))
        return -NX_EFAULT;
    if (length > NX_SOCKADDR_MAX) return -NX_EINVAL;
    size_t used = length < sizeof(struct nx_sockaddr_in) ? length : sizeof(struct nx_sockaddr_in);
    if (!user_access_ok(vm, address, used, true)) return -NX_EFAULT;
    return 0;
}
static int64_t flags_ok(uint64_t flags) { return flags & ~(uint64_t)NX_MSG_SUPPORTED ? -NX_EOPNOTSUPP : 0; }
/* A data buffer: readable (send) or writable (receive), at most SSIZE_MAX. */
static int64_t buffer_ok(const struct address_space *vm, uintptr_t buffer, uint64_t length, bool write)
{
    if (length > INT64_MAX) return -NX_EINVAL;
    return user_access_ok(vm, buffer, length, write) ? 0 : -NX_EFAULT;
}

/* Size of a known option's value, or 0 when the level/name pair is unknown.
 * SO_ERROR and SO_TYPE can only be read. */
static size_t option_size(int64_t level, int64_t name, bool set)
{
    if (level == NX_SOL_SOCKET)
        switch (name) {
        case NX_SO_REUSEADDR: case NX_SO_SNDBUF: case NX_SO_RCVBUF: return sizeof(int32_t);
        case NX_SO_SNDTIMEO: case NX_SO_RCVTIMEO: return sizeof(struct nx_timeval);
        case NX_SO_ERROR: case NX_SO_TYPE: return set ? 0 : sizeof(int32_t);
        default: return 0;
        }
    if (level == NX_IPPROTO_TCP && name == NX_TCP_NODELAY) return sizeof(int32_t);
    return 0;
}
static int64_t option_in(const struct address_space *vm, int64_t level, int64_t name, uintptr_t value,
                         uint64_t length)
{
    size_t size = option_size(level, name, true);
    if (!size) return -NX_ENOPROTOOPT;
    if (length < size || length > NX_SOCKADDR_MAX) return -NX_EINVAL;
    union { int32_t integer; struct nx_timeval time; } v;
    if (!value || copyin(vm, value, &v, size)) return -NX_EFAULT;
    if (size == sizeof(struct nx_timeval)) {
        if (v.time.sec < 0 || v.time.usec < 0 || v.time.usec >= 1000000) return -NX_EDOM;
    } else if ((name == NX_SO_SNDBUF || name == NX_SO_RCVBUF) && v.integer <= 0) {
        return -NX_EINVAL;
    }
    return 0;
}
static int64_t option_out(const struct address_space *vm, int64_t level, int64_t name, uintptr_t value,
                          uintptr_t length_pointer)
{
    size_t size = option_size(level, name, false);
    if (!size) return -NX_ENOPROTOOPT;
    nx_socklen_t length;
    if (!length_pointer || !user_access_ok(vm, length_pointer, sizeof(length), true) ||
        copyin(vm, length_pointer, &length, sizeof(length)))
        return -NX_EFAULT;
    if (length < size) return -NX_EINVAL;
    if (!value || !user_access_ok(vm, value, size, true)) return -NX_EFAULT;
    return 0;
}

static void to_record(struct nx_sockaddr_in *out, nx_net_addr_t addr, nx_net_port_t port)
{
    *out = (struct nx_sockaddr_in){ .family = NX_AF_INET, .port = __builtin_bswap16(port),
                                    .addr = __builtin_bswap32(addr) };
}
/* Publish an address through a prevalidated value-result pair: at most the
 * stated length is written, and the length becomes the full record size. */
static int64_t address_publish(const struct address_space *vm, uintptr_t address, uintptr_t length_pointer,
                               nx_net_addr_t addr, nx_net_port_t port)
{
    if (!address) return 0;
    struct nx_sockaddr_in record;
    to_record(&record, addr, port);
    nx_socklen_t length;
    if (copyin(vm, length_pointer, &length, sizeof(length))) return -NX_EFAULT;
    size_t used = length < sizeof(record) ? length : sizeof(record);
    nx_socklen_t full = sizeof(record);
    if (copyout(vm, address, &record, used) || copyout(vm, length_pointer, &full, sizeof(full))) return -NX_EFAULT;
    return 0;
}

static int64_t do_accept(struct user *u, const struct address_space *vm, struct nx_socket *s, bool nonblock,
                         uintptr_t address, uintptr_t length_pointer)
{
    uint64_t deadline = deadline_after(s->receive_timeout);
    for (;;) {
        int error = files_socket_reserve(u);
        if (error < 0) return error;
        uint32_t seen = s->events;
        struct nx_socket *child;
        nx_net_addr_t addr;
        nx_net_port_t port;
        error = net_socket_accept(s, &child, &addr, &port);
        if (error == -NX_EAGAIN) {
            if (nonblock) return error;
            if ((error = wait_event(s, seen, deadline))) return error;
            continue;
        }
        if (error) return error;
        int fd = files_socket_install(u, child);
        if (fd < 0) { net_socket_release(child); return fd; }
        error = (int)address_publish(vm, address, length_pointer, addr, port);
        return error ? error : fd;   /* a fault after the accept still leaves the descriptor */
    }
}
/* A non-blocking connect, or one whose SO_SNDTIMEO expires, is EINPROGRESS;
 * the outcome is then read from SO_ERROR. */
static int64_t do_connect(struct nx_socket *s, const struct nx_sockaddr_in *a, bool nonblock)
{
    nx_net_addr_t addr = a->family == NX_AF_UNSPEC ? 0 : __builtin_bswap32(a->addr);
    int error = net_socket_connect(s, addr, a->family == NX_AF_UNSPEC ? 0 : __builtin_bswap16(a->port));
    if (error) return error;
    uint64_t deadline = deadline_after(s->send_timeout);
    for (;;) {
        uint32_t seen = s->events;
        error = net_socket_connected(s);
        if (error != -NX_EAGAIN) return error;
        if (nonblock) return -NX_EINPROGRESS;
        error = wait_event(s, seen, deadline);   /* the attempt continues; retry is EALREADY */
        if (error) return error == -NX_EAGAIN ? -NX_EINPROGRESS : error;
    }
}

struct transfer {
    struct nx_socket *s;
    const struct address_space *vm;
    uintptr_t buffer;
    size_t length;
    bool nonblock, peek, waitall, nosignal;
    uint64_t deadline;
    const struct nx_sockaddr_in *to;             /* sendto destination, or NULL */
    uintptr_t from, from_length;                 /* recvfrom source, or 0 */
};
/* What a failed call returns once some bytes have moved: the count. */
static int64_t partial(size_t done, int64_t error) { return done ? (int64_t)done : error; }

static int64_t stream_send(const struct transfer *t)
{
    uint8_t buffer[CHUNK];
    size_t done = 0;
    while (done < t->length) {
        size_t part = t->length - done < CHUNK ? t->length - done : CHUNK;
        if (copyin(t->vm, t->buffer + done, buffer, part)) return partial(done, -NX_EFAULT);
        uint32_t seen = t->s->events;
        int64_t n = net_socket_send(t->s, buffer, part);
        if (n == -NX_EAGAIN) {
            if (t->nonblock) return partial(done, n);
            int error = wait_event(t->s, seen, t->deadline);
            if (error) return partial(done, error);
            continue;
        }
        if (n < 0) {
            if (n == -NX_EPIPE && !t->nosignal) signal_post(proc_current(), NX_SIGPIPE);
            return partial(done, n);
        }
        done += (size_t)n;
    }
    return done;
}
static int64_t stream_receive(const struct transfer *t)
{
    uint8_t buffer[CHUNK];
    size_t done = 0;
    while (done < t->length) {
        size_t part = t->length - done < CHUNK ? t->length - done : CHUNK;
        uint32_t seen = t->s->events;
        int64_t n = net_socket_receive(t->s, buffer, part, t->peek);
        if (n == -NX_EAGAIN) {
            if (done && !t->waitall) break;   /* return what has arrived */
            if (t->nonblock) return partial(done, n);
            int error = wait_event(t->s, seen, t->deadline);
            if (error) return partial(done, error);
            continue;
        }
        if (n < 0) return partial(done, n);
        if (!n) break;   /* end of stream */
        if (copyout(t->vm, t->buffer + done, buffer, (size_t)n)) return partial(done, -NX_EFAULT);
        done += (size_t)n;
        if (t->peek) break;   /* a peek is one look; MSG_WAITALL does not apply */
        if ((size_t)n < part && !t->waitall) break;
    }
    if (t->from) {
        nx_net_addr_t addr;
        nx_net_port_t port;
        if (!net_socket_name(t->s, true, &addr, &port) && address_publish(t->vm, t->from, t->from_length, addr, port))
            return partial(done, -NX_EFAULT);
    }
    return done;
}
static int64_t datagram_send(const struct transfer *t)
{
    if (t->length > NX_NET_IP_MAXPACKET - 20) return -NX_EMSGSIZE;   /* before copying anything */
    uint8_t buffer[CHUNK];
    struct nx_net_mbuf *chain = NULL;
    for (size_t done = 0; done < t->length;) {
        size_t part = t->length - done < CHUNK ? t->length - done : CHUNK;
        int error = copyin(t->vm, t->buffer + done, buffer, part) ? -NX_EFAULT : net_socket_stage(&chain, buffer, part);
        if (error) { net_socket_discard(chain); return error; }
        done += part;
    }
    nx_net_addr_t addr = t->to ? __builtin_bswap32(t->to->addr) : 0;
    nx_net_port_t port = t->to ? __builtin_bswap16(t->to->port) : 0;
    int64_t n = net_socket_send_datagram(t->s, chain, t->to != NULL, addr, port);
    if (n == -NX_EPIPE && !t->nosignal) signal_post(proc_current(), NX_SIGPIPE);
    return n;
}
static int64_t datagram_receive(const struct transfer *t)
{
    struct nx_net_mbuf *chain;
    nx_net_addr_t addr;
    nx_net_port_t port;
    for (;;) {
        uint32_t seen = t->s->events;
        int error = net_socket_receive_datagram(t->s, t->peek, &chain, &addr, &port);
        if (!error) break;
        if (error != -NX_EAGAIN) return error;
        if (t->nonblock) return error;
        if ((error = wait_event(t->s, seen, t->deadline))) return error;
    }
    if (!chain) return 0;   /* SHUT_RD */
    /* The chain is ours now; reading it needs no worker. A datagram longer
     * than the buffer is truncated and the rest discarded, as in BSD. */
    size_t size = nx_net_mbuf_length(chain), n = t->length < size ? t->length : size;
    uint8_t buffer[CHUNK];
    int64_t result = (int64_t)n;
    for (size_t done = 0; done < n && result >= 0;) {
        size_t part = n - done < CHUNK ? n - done : CHUNK;
        nx_net_mbuf_copydata(chain, done, part, buffer);
        if (copyout(t->vm, t->buffer + done, buffer, part)) result = -NX_EFAULT;
        done += part;
    }
    net_socket_discard(chain);
    if (result >= 0 && t->from && address_publish(t->vm, t->from, t->from_length, addr, port)) result = -NX_EFAULT;
    return result;
}
static int64_t transfer(struct transfer *t, uint32_t open_flags, uint64_t flags, bool write)
{
    t->nonblock = (open_flags & NX_O_NONBLOCK) || (flags & NX_MSG_DONTWAIT);
    t->peek = flags & NX_MSG_PEEK;
    t->waitall = flags & NX_MSG_WAITALL;
    t->nosignal = flags & NX_MSG_NOSIGNAL;
    t->deadline = deadline_after(write ? t->s->send_timeout : t->s->receive_timeout);
    if (t->s->type == NX_SOCK_STREAM) {
        if (!t->length) return 0;
        return write ? stream_send(t) : stream_receive(t);   /* a sendto address is ignored (POSIX) */
    }
    return write ? datagram_send(t) : datagram_receive(t);
}

int64_t socket_file_io(void *object, uint32_t open_flags, uintptr_t address, uint64_t count, bool write)
{
    struct transfer t = { .s = object, .vm = &proc_current()->p_vm, .buffer = address, .length = count };
    return transfer(&t, open_flags, 0, write);
}
void socket_file_close(void *object) { net_socket_release(object); }

/* Configuration ioctls (task 427): any socket descriptor; changes are root's. */
int64_t socket_ioctl(void *object, uint64_t request, uintptr_t argument)
{
    (void)object;
    struct proc *p = proc_current();
    size_t size;
    bool change = false;
    switch (request) {
    case NX_SIOCGIFINFO: size = sizeof(struct nx_ifinfo); break;
    case NX_SIOCSIFCONFIG: size = sizeof(struct nx_ifconfig); change = true; break;
    case NX_SIOCGROUTE: size = sizeof(struct nx_route); break;
    case NX_SIOCSROUTE: size = sizeof(struct nx_route); change = true; break;
    case NX_SIOCLOOKUP: size = sizeof(struct nx_route_lookup); break;
    default: return -NX_ENOTTY;
    }
    union { struct nx_ifinfo info; struct nx_ifconfig config; struct nx_route route; struct nx_route_lookup lookup; } record;
    if (!user_access_ok(&p->p_vm, argument, size, !change) || copyin(&p->p_vm, argument, &record, size)) return -NX_EFAULT;
    if (change && !proc_superuser(p->p_user)) return -NX_EPERM;
    int error = net_socket_configure(request, &record);
    if (error || change) return error;
    return copyout(&p->p_vm, argument, &record, size) ? -NX_EFAULT : 0;
}

int64_t socket_syscall(uint64_t number, const uintptr_t a[6])
{
    struct proc *p = proc_current();
    struct user *u = p->p_user;
    const struct address_space *vm = &p->p_vm;
    int64_t fd = (int32_t)a[0], error = 0;
    struct nx_sockaddr_in address;
    bool addressed = false;
    switch (number) {
    case NX_SYS_SOCKET: return socket_create(u, (int32_t)a[0], (int32_t)a[1], (int32_t)a[2]);
    case NX_SYS_BIND: error = address_in(vm, a[1], a[2], false, &address); break;
    case NX_SYS_CONNECT: error = address_in(vm, a[1], a[2], true, &address); break;
    case NX_SYS_LISTEN: break;
    case NX_SYS_ACCEPT: error = address_out(vm, a[1], a[2], false); break;
    case NX_SYS_SHUTDOWN: if (a[1] > NX_SHUT_RDWR) error = -NX_EINVAL; break;
    case NX_SYS_SENDTO:
        if (!(error = flags_ok(a[3])) && !(error = buffer_ok(vm, a[1], a[2], false)) && a[4]) {
            error = address_in(vm, a[4], a[5], false, &address);
            addressed = true;
        }
        break;
    case NX_SYS_RECVFROM:
        if (!(error = flags_ok(a[3])) && !(error = buffer_ok(vm, a[1], a[2], true)))
            error = address_out(vm, a[4], a[5], false);
        break;
    case NX_SYS_GETSOCKNAME: case NX_SYS_GETPEERNAME: error = address_out(vm, a[1], a[2], true); break;
    case NX_SYS_SETSOCKOPT: error = option_in(vm, (int32_t)a[1], (int32_t)a[2], a[3], a[4]); break;
    case NX_SYS_GETSOCKOPT: error = option_out(vm, (int32_t)a[1], (int32_t)a[2], a[3], a[4]); break;
    default: return -NX_EINVAL;
    }
    if (error) return error;
    int lookup;
    uint32_t open_flags;
    struct nx_socket *s = files_socket(u, fd, &lookup, &open_flags);
    if (!s) return lookup;
    bool nonblock = open_flags & NX_O_NONBLOCK;
    switch (number) {
    case NX_SYS_BIND: {
        nx_net_port_t port = __builtin_bswap16(address.port);
        bool privileged = port && port < 1024 && s->type != NX_SOCK_RAW && proc_superuser(u);
        return net_socket_bind(s, __builtin_bswap32(address.addr), port, privileged);
    }
    case NX_SYS_LISTEN: return net_socket_listen(s, (int32_t)a[1]);
    case NX_SYS_ACCEPT: return do_accept(u, vm, s, nonblock, a[1], a[2]);
    case NX_SYS_CONNECT: return do_connect(s, &address, nonblock);
    case NX_SYS_SHUTDOWN: return net_socket_shutdown(s, (int)a[1]);
    case NX_SYS_GETSOCKNAME: case NX_SYS_GETPEERNAME: {
        nx_net_addr_t addr;
        nx_net_port_t port;
        error = net_socket_name(s, number == NX_SYS_GETPEERNAME, &addr, &port);
        return error ? error : address_publish(vm, a[1], a[2], addr, port);
    }
    case NX_SYS_SENDTO: case NX_SYS_RECVFROM: {
        bool write = number == NX_SYS_SENDTO;
        struct transfer t = { .s = s, .vm = vm, .buffer = a[1], .length = a[2], .to = addressed ? &address : NULL,
                              .from = write ? 0 : a[4], .from_length = write ? 0 : a[5] };
        return transfer(&t, open_flags, a[3], write);
    }
    case NX_SYS_SETSOCKOPT: {
        union { int32_t integer; struct nx_timeval time; } value;
        if (copyin(vm, a[3], &value, option_size((int32_t)a[1], (int32_t)a[2], true))) return -NX_EFAULT;
        return net_socket_set_option(s, (int32_t)a[1], (int32_t)a[2], &value);
    }
    case NX_SYS_GETSOCKOPT: {
        union { int32_t integer; struct nx_timeval time; } value = {0};
        nx_socklen_t size = (nx_socklen_t)option_size((int32_t)a[1], (int32_t)a[2], false);
        if ((error = net_socket_get_option(s, (int32_t)a[1], (int32_t)a[2], &value))) return error;
        if (copyout(vm, a[3], &value, size) || copyout(vm, a[4], &size, sizeof(size))) return -NX_EFAULT;
        return 0;
    }
    default: return -NX_EINVAL;
    }
}
