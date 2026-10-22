/* Socket wrappers over native syscalls 77-88 (task 424). send and recv are
 * sendto/recvfrom without addresses, as in 4.4BSD libc. */
#include <sys/socket.h>
#include <errno.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>
static long call(long number, long a1, long a2, long a3, long a4, long a5, long a6)
{
    long value = nx_syscall6(number, a1, a2, a3, a4, a5, a6);
    if (value < 0) { errno = (int)-value; return -1; }
    return value;
}
#define P(x) ((long)(uintptr_t)(x))
int socket(int domain, int type, int protocol) { return call(NX_SYS_SOCKET, domain, type, protocol, 0, 0, 0); }
int bind(int fd, const struct sockaddr *address, socklen_t length)
{ return call(NX_SYS_BIND, fd, P(address), length, 0, 0, 0); }
int listen(int fd, int backlog) { return call(NX_SYS_LISTEN, fd, backlog, 0, 0, 0, 0); }
int accept(int fd, struct sockaddr *address, socklen_t *length)
{ return call(NX_SYS_ACCEPT, fd, P(address), P(length), 0, 0, 0); }
int connect(int fd, const struct sockaddr *address, socklen_t length)
{ return call(NX_SYS_CONNECT, fd, P(address), length, 0, 0, 0); }
int shutdown(int fd, int how) { return call(NX_SYS_SHUTDOWN, fd, how, 0, 0, 0, 0); }
ssize_t sendto(int fd, const void *buffer, size_t length, int flags, const struct sockaddr *address,
               socklen_t address_length)
{ return call(NX_SYS_SENDTO, fd, P(buffer), (long)length, flags, P(address), address_length); }
ssize_t recvfrom(int fd, void *buffer, size_t length, int flags, struct sockaddr *address,
                 socklen_t *address_length)
{ return call(NX_SYS_RECVFROM, fd, P(buffer), (long)length, flags, P(address), P(address_length)); }
ssize_t send(int fd, const void *buffer, size_t length, int flags) { return sendto(fd, buffer, length, flags, 0, 0); }
ssize_t recv(int fd, void *buffer, size_t length, int flags) { return recvfrom(fd, buffer, length, flags, 0, 0); }
int getsockname(int fd, struct sockaddr *address, socklen_t *length)
{ return call(NX_SYS_GETSOCKNAME, fd, P(address), P(length), 0, 0, 0); }
int getpeername(int fd, struct sockaddr *address, socklen_t *length)
{ return call(NX_SYS_GETPEERNAME, fd, P(address), P(length), 0, 0, 0); }
int setsockopt(int fd, int level, int name, const void *value, socklen_t length)
{ return call(NX_SYS_SETSOCKOPT, fd, level, name, P(value), length, 0); }
int getsockopt(int fd, int level, int name, void *value, socklen_t *length)
{ return call(NX_SYS_GETSOCKOPT, fd, level, name, P(value), P(length), 0); }
