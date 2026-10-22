#ifndef NEXIX_LIBC_SYS_SOCKET_H
#define NEXIX_LIBC_SYS_SOCKET_H
#include <sys/types.h>
#include <sys/time.h>
#include <nexix/socket.h>
/* BSD sockets over the native ABI (task 424): AF_INET stream, datagram and
 * root-only raw ICMP sockets. */
typedef nx_socklen_t socklen_t;
typedef nx_sa_family_t sa_family_t;
struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
struct sockaddr_storage { sa_family_t ss_family; unsigned short ss_pad; unsigned int ss_data[31]; };
#define AF_UNSPEC NX_AF_UNSPEC
#define AF_INET NX_AF_INET
#define PF_UNSPEC AF_UNSPEC
#define PF_INET AF_INET
#define SOCK_STREAM NX_SOCK_STREAM
#define SOCK_DGRAM NX_SOCK_DGRAM
#define SOCK_RAW NX_SOCK_RAW
#define SOL_SOCKET NX_SOL_SOCKET
#define SO_REUSEADDR NX_SO_REUSEADDR
#define SO_SNDBUF NX_SO_SNDBUF
#define SO_RCVBUF NX_SO_RCVBUF
#define SO_SNDTIMEO NX_SO_SNDTIMEO
#define SO_RCVTIMEO NX_SO_RCVTIMEO
#define SO_ERROR NX_SO_ERROR
#define SO_TYPE NX_SO_TYPE
#define MSG_PEEK NX_MSG_PEEK
#define MSG_WAITALL NX_MSG_WAITALL
#define MSG_DONTWAIT NX_MSG_DONTWAIT
#define MSG_NOSIGNAL NX_MSG_NOSIGNAL
#define SHUT_RD NX_SHUT_RD
#define SHUT_WR NX_SHUT_WR
#define SHUT_RDWR NX_SHUT_RDWR
#define SOMAXCONN NX_SOMAXCONN
int socket(int domain, int type, int protocol);
int bind(int fd, const struct sockaddr *address, socklen_t length);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr *address, socklen_t *length);
/* <sys/mx.h> maps V7's three-argument connect to mpx_connect with a macro;
 * a file uses one or the other, so the socket declaration replaces it. */
#ifdef connect
#undef connect
#endif
int connect(int fd, const struct sockaddr *address, socklen_t length);
int shutdown(int fd, int how);
ssize_t send(int fd, const void *buffer, size_t length, int flags);
ssize_t recv(int fd, void *buffer, size_t length, int flags);
ssize_t sendto(int fd, const void *buffer, size_t length, int flags, const struct sockaddr *address,
               socklen_t address_length);
ssize_t recvfrom(int fd, void *buffer, size_t length, int flags, struct sockaddr *address,
                 socklen_t *address_length);
int getsockname(int fd, struct sockaddr *address, socklen_t *length);
int getpeername(int fd, struct sockaddr *address, socklen_t *length);
int setsockopt(int fd, int level, int name, const void *value, socklen_t length);
int getsockopt(int fd, int level, int name, void *value, socklen_t *length);
_Static_assert(sizeof(struct sockaddr) == sizeof(struct nx_sockaddr), "sockaddr public layout");
_Static_assert(sizeof(struct sockaddr_storage) == sizeof(struct nx_sockaddr_storage), "sockaddr_storage public layout");
#endif
