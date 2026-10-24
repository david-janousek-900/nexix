#ifndef NEXIX_LIBC_ARPA_INET_H
#define NEXIX_LIBC_ARPA_INET_H
#include <netinet/in.h>
/* IPv4 address and classful network conversions. */
uint32_t inet_network(const char *);
struct in_addr inet_makeaddr(uint32_t, uint32_t);
uint32_t inet_lnaof(struct in_addr);
uint32_t inet_netof(struct in_addr);
in_addr_t inet_addr(const char *);
int inet_aton(const char *, struct in_addr *);
char *inet_ntoa(struct in_addr);
int inet_pton(int family, const char *source, void *destination);
const char *inet_ntop(int family, const void *source, char *destination, socklen_t size);
#endif
