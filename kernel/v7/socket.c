/* Socket syscall boundary (task 424): decode and validate every argument of
 * the native socket ABI before any descriptor is consulted, so malformed
 * calls fail the same way once sockets exist. Descriptors cannot yet refer
 * to sockets (task 425), so valid calls end in EBADF/ENOTSOCK, and a valid
 * socket() request in ENOSYS. */
#include "socket.h"
#include "process.h"
#include "accounting.h"
#include "uaccess.h"
#include "v7/core.h"
#include <nexix/socket.h>
#include <nexix/syscall.h>
#include <nexix/errno.h>

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
    return -NX_ENOSYS;   /* socket descriptors arrive with task 425 */
}

/* The descriptor check every call ends in: no descriptor is a socket yet. */
static int64_t descriptor(const struct user *u, int64_t fd)
{
    if (fd < 0 || fd >= V7_NOFILE || !u->u_ofile[fd]) return -NX_EBADF;
    return -NX_ENOTSOCK;
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

int64_t socket_syscall(uint64_t number, const uintptr_t a[6])
{
    struct proc *p = proc_current();
    struct user *u = p->p_user;
    const struct address_space *vm = &p->p_vm;
    int64_t fd = (int32_t)a[0], error = 0;
    struct nx_sockaddr_in address;
    switch (number) {
    case NX_SYS_SOCKET: return socket_create(u, (int32_t)a[0], (int32_t)a[1], (int32_t)a[2]);
    case NX_SYS_BIND: error = address_in(vm, a[1], a[2], false, &address); break;
    case NX_SYS_CONNECT: error = address_in(vm, a[1], a[2], true, &address); break;
    case NX_SYS_LISTEN: break;
    case NX_SYS_ACCEPT: error = address_out(vm, a[1], a[2], false); break;
    case NX_SYS_SHUTDOWN: if (a[1] > NX_SHUT_RDWR) error = -NX_EINVAL; break;
    case NX_SYS_SENDTO:
        if (!(error = flags_ok(a[3])) && !(error = buffer_ok(vm, a[1], a[2], false)) && a[4])
            error = address_in(vm, a[4], a[5], false, &address);
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
    return error ? error : descriptor(u, fd);
}
