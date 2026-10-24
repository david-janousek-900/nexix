/* Native static route configuration over the NX_SIOC* socket ioctls (task
 * 427): one connected route per addressed interface, plus a default. */
#include "../netconfig.h"
static int usage(void)
{
    fputs("usage: route [show]\n"
          "       route add default gateway\n"
          "       route delete default\n"
          "       route get destination\n", stderr);
    return 2;
}
static int show(int fd)
{
    char a[INET_ADDRSTRLEN], m[INET_ADDRSTRLEN];
    printf("%-16s%-16s%-16s%s\n", "Destination", "Gateway", "Netmask", "Interface");
    struct nx_ifinfo r;
    for (unsigned i = 0;; ++i) {
        memset(&r, 0, sizeof(r));
        r.index = i;
        if (ioctl(fd, NX_SIOCGIFINFO, &r) < 0) break;
        /* Only running, addressed interfaces route (task 419). */
        if (!r.addressed || !(r.flags & NX_IFF_RUNNING)) continue;
        printf("%-16s%-16s%-16s%s\n", netconfig_text(r.addr & r.mask, a), "link", netconfig_text(r.mask, m), r.name);
    }
    if (errno != ENXIO) { netconfig_fail("route", NULL); return 1; }
    struct nx_route route;
    if (ioctl(fd, NX_SIOCGROUTE, &route) < 0) { netconfig_fail("route", NULL); return 1; }
    if (route.flags & NX_RT_DEFAULT) {
        struct nx_route_lookup l;
        memset(&l, 0, sizeof(l));
        l.dst = route.gateway;
        /* The gateway is rechecked at every lookup; it may be off-link now. */
        const char *ifname = ioctl(fd, NX_SIOCLOOKUP, &l) == 0 && !(l.flags & NX_RT_LOCAL) ? l.ifname : "-";
        printf("%-16s%-16s%-16s%s\n", "default", netconfig_text(route.gateway, a), "0.0.0.0", ifname);
    }
    return 0;
}
int main(int argc, char **argv)
{
    int fd = netconfig_socket();
    if (fd < 0) { netconfig_fail("route", NULL); return 1; }
    if (argc == 1 || (argc == 2 && !strcmp(argv[1], "show"))) return show(fd);
    struct nx_route route;
    memset(&route, 0, sizeof(route));
    if (argc == 4 && !strcmp(argv[1], "add") && !strcmp(argv[2], "default")) {
        if(netconfig_resolve("route",argv[3],&route.gateway))return 2;
        if (ioctl(fd, NX_SIOCSROUTE, &route) < 0) { netconfig_fail("route", argv[3]); return 1; }
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "delete") && !strcmp(argv[2], "default")) {
        if (ioctl(fd, NX_SIOCSROUTE, &route) < 0) { netconfig_fail("route", NULL); return 1; }
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "get")) {
        struct nx_route_lookup l;
        char d[INET_ADDRSTRLEN], g[INET_ADDRSTRLEN], s[INET_ADDRSTRLEN];
        memset(&l, 0, sizeof(l));
        if(netconfig_resolve("route",argv[2],&l.dst))return 2;
        if (ioctl(fd, NX_SIOCLOOKUP, &l) < 0) { netconfig_fail("route", argv[2]); return 1; }
        printf("%s", netconfig_text(l.dst, d));
        if (l.flags & NX_RT_GATEWAY) printf(" via %s", netconfig_text(l.next_hop, g));
        printf(" dev %s src %s", l.ifname, netconfig_text(l.source, s));
        if (l.flags & NX_RT_LOCAL) printf(" local");
        if (l.flags & NX_RT_BROADCAST) printf(" broadcast");
        putchar('\n');
        return 0;
    }
    return usage();
}
