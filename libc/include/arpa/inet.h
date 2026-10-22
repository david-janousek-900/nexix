#ifndef NEXIX_LIBC_ARPA_INET_H
#define NEXIX_LIBC_ARPA_INET_H
#include <netinet/in.h>
/* Numeric IPv4 conversions only; there is no resolver. */
in_addr_t inet_addr(const char *);
int inet_aton(const char *, struct in_addr *);
char *inet_ntoa(struct in_addr);
int inet_pton(int family, const char *source, void *destination);
const char *inet_ntop(int family, const void *source, char *destination, socklen_t size);
#endif
