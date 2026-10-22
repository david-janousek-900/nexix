#ifndef NEXIX_LIBC_NETINET_IN_H
#define NEXIX_LIBC_NETINET_IN_H
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;
struct in_addr { in_addr_t s_addr; };      /* network order */
struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;                    /* network order */
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};
#define IPPROTO_IP NX_IPPROTO_IP
#define IPPROTO_ICMP NX_IPPROTO_ICMP
#define IPPROTO_TCP NX_IPPROTO_TCP
#define IPPROTO_UDP NX_IPPROTO_UDP
/* Host-order values, as in BSD; convert with htonl. */
#define INADDR_ANY ((in_addr_t)NX_INADDR_ANY)
#define INADDR_LOOPBACK ((in_addr_t)NX_INADDR_LOOPBACK)
#define INADDR_BROADCAST ((in_addr_t)NX_INADDR_BROADCAST)
#define INADDR_NONE ((in_addr_t)NX_INADDR_NONE)
#define INET_ADDRSTRLEN 16
uint32_t htonl(uint32_t);
uint16_t htons(uint16_t);
uint32_t ntohl(uint32_t);
uint16_t ntohs(uint16_t);
_Static_assert(sizeof(struct sockaddr_in) == sizeof(struct nx_sockaddr_in), "sockaddr_in public layout");
_Static_assert(offsetof(struct sockaddr_in, sin_port) == offsetof(struct nx_sockaddr_in, port) &&
               offsetof(struct sockaddr_in, sin_addr) == offsetof(struct nx_sockaddr_in, addr),
               "sockaddr_in public offsets");
#endif
