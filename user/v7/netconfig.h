/* Shared helpers for the native ifconfig and route commands (task 427). */
#ifndef NEXIX_NETCONFIG_H
#define NEXIX_NETCONFIG_H
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* Any socket carries the configuration ioctls; a datagram one is cheapest. */
static inline int netconfig_socket(void)
{
    return socket(AF_INET, SOCK_DGRAM, 0);
}
static inline const char *netconfig_error(int error)
{
    return error > 0 && error < sys_nerr ? sys_errlist[error] : "Unknown error";
}
static inline void netconfig_fail(const char *command, const char *subject)
{
    if (subject) fprintf(stderr, "%s: %s: %s\n", command, subject, netconfig_error(errno));
    else fprintf(stderr, "%s: %s\n", command, netconfig_error(errno));
}
/* Resolve a host, copying the static libc result before the next lookup. */
static inline int netconfig_resolve(const char *command,const char *name,uint32_t *address)
{
    if(inet_pton(AF_INET,name,address)==1)return 0;
    struct hostent *host=gethostbyname(name);
    if(!host){fprintf(stderr,"%s: ",command);herror(name);return -1;}
    if(host->h_addrtype!=AF_INET||host->h_length!=4||!host->h_addr){h_errno=NO_DATA;fprintf(stderr,"%s: ",command);herror(name);return -1;}
    memcpy(address,host->h_addr,4);return 0;
}
/* Dotted-quad text of a network-order address. */
static inline const char *netconfig_text(uint32_t address, char text[INET_ADDRSTRLEN])
{
    return inet_ntop(AF_INET, &address, text, INET_ADDRSTRLEN);
}
/* A network-order mask from /bits, dotted quad or 0x hex; -1 if malformed. */
static inline int netconfig_mask(const char *text, uint32_t *mask)
{
    char *end;
    if (text[0] == '/') {
        long bits = strtol(text + 1, &end, 10);
        if (end == text + 1 || *end || bits < 1 || bits > 32) return -1;
        *mask = htonl(bits == 32 ? 0xffffffffu : ~(0xffffffffu >> bits));
        return 0;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        unsigned long value = strtoul(text + 2, &end, 16);
        if (end == text + 2 || *end || value > 0xffffffffu) return -1;
        *mask = htonl((uint32_t)value);
        return 0;
    }
    return inet_pton(AF_INET, text, mask) == 1 ? 0 : -1;
}
#endif
