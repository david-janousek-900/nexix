#ifndef NEXIX_ABI_SOCKET_H
#define NEXIX_ABI_SOCKET_H
#include <stdint.h>
/* Native socket ABI (task 424). Constants keep 4.4BSD values; records are
 * fixed-width native layouts, never upstream kernel structures. Ports and
 * IPv4 addresses inside records are in network byte order. */
#define NX_AF_UNSPEC 0
#define NX_AF_INET 2
#define NX_SOCK_STREAM 1
#define NX_SOCK_DGRAM 2
#define NX_SOCK_RAW 3
#define NX_IPPROTO_IP 0
#define NX_IPPROTO_ICMP 1
#define NX_IPPROTO_TCP 6
#define NX_IPPROTO_UDP 17
#define NX_INADDR_ANY 0x00000000u
#define NX_INADDR_LOOPBACK 0x7f000001u
#define NX_INADDR_BROADCAST 0xffffffffu
#define NX_INADDR_NONE 0xffffffffu

/* Options: SOL_SOCKET level, and TCP_NODELAY at IPPROTO_TCP. */
#define NX_SOL_SOCKET 0xffff
#define NX_SO_REUSEADDR 0x0004
#define NX_SO_SNDBUF 0x1001
#define NX_SO_RCVBUF 0x1002
#define NX_SO_SNDTIMEO 0x1005
#define NX_SO_RCVTIMEO 0x1006
#define NX_SO_ERROR 0x1007      /* read-only */
#define NX_SO_TYPE 0x1008       /* read-only */
#define NX_TCP_NODELAY 0x01

/* send/recv flags; any other bit is EOPNOTSUPP. */
#define NX_MSG_PEEK 0x02
#define NX_MSG_WAITALL 0x40
#define NX_MSG_DONTWAIT 0x80
#define NX_MSG_NOSIGNAL 0x400
#define NX_MSG_SUPPORTED (NX_MSG_PEEK | NX_MSG_WAITALL | NX_MSG_DONTWAIT | NX_MSG_NOSIGNAL)
#define NX_SHUT_RD 0
#define NX_SHUT_WR 1
#define NX_SHUT_RDWR 2
#define NX_SOMAXCONN 8
#define NX_SOCKADDR_MAX 128     /* largest address length accepted */

typedef uint32_t nx_socklen_t;
typedef uint16_t nx_sa_family_t;
struct nx_sockaddr { nx_sa_family_t family; uint8_t data[14]; };
struct nx_sockaddr_in {
    nx_sa_family_t family;      /* NX_AF_INET */
    uint16_t port;              /* network order */
    uint32_t addr;              /* network order */
    uint8_t zero[8];            /* ignored on input, zero on output */
};
struct nx_sockaddr_storage { nx_sa_family_t family; uint16_t pad; uint32_t data[31]; };
/* SO_SNDTIMEO/SO_RCVTIMEO value; zero means no timeout. */
struct nx_timeval { int64_t sec, usec; };
_Static_assert(sizeof(struct nx_sockaddr) == 16, "sockaddr ABI layout");
_Static_assert(sizeof(struct nx_sockaddr_in) == 16, "sockaddr_in ABI layout");
_Static_assert(sizeof(struct nx_sockaddr_storage) == NX_SOCKADDR_MAX, "sockaddr_storage ABI layout");
_Static_assert(sizeof(struct nx_timeval) == 16, "timeval ABI layout");
#endif
