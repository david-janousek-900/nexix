/* Native interface configuration over the NX_SIOC* socket ioctls (task 427).
 * Queries are open to everyone; changes need root and are applied whole. */
#include "../netconfig.h"
static int usage(void)
{
    fputs("usage: ifconfig [-a]\n"
          "       ifconfig interface\n"
          "       ifconfig interface [address[/bits]] [netmask mask] [up|down]\n"
          "       ifconfig interface delete [up|down]\n", stderr);
    return 2;
}
static void show(const struct nx_ifinfo *r)
{
    static const struct { unsigned bit; const char *name; } names[] = {
        {NX_IFF_UP, "UP"}, {NX_IFF_BROADCAST, "BROADCAST"}, {NX_IFF_LOOPBACK, "LOOPBACK"}, {NX_IFF_RUNNING, "RUNNING"}};
    char a[INET_ADDRSTRLEN], m[INET_ADDRSTRLEN], b[INET_ADDRSTRLEN];
    const char *separator = "";
    printf("%s: flags=%x<", r->name, (unsigned)r->flags);
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (r->flags & names[i].bit) { printf("%s%s", separator, names[i].name); separator = ","; }
    printf("> mtu %u\n", (unsigned)r->mtu);
    if (!(r->flags & NX_IFF_LOOPBACK))
        printf("\tether %02x:%02x:%02x:%02x:%02x:%02x\n", r->lladdr[0], r->lladdr[1], r->lladdr[2],
               r->lladdr[3], r->lladdr[4], r->lladdr[5]);
    if (r->addressed) {
        printf("\tinet %s netmask %s", netconfig_text(r->addr, a), netconfig_text(r->mask, m));
        if (r->broadcast) printf(" broadcast %s", netconfig_text(r->broadcast, b));
        putchar('\n');
    }
    printf("\tpackets in %llu out %llu, errors in %llu out %llu, drops %llu\n",
           (unsigned long long)r->ipackets, (unsigned long long)r->opackets, (unsigned long long)r->ierrors,
           (unsigned long long)r->oerrors, (unsigned long long)r->iqdrops);
}
/* BSD's classful default when no mask is given. */
static uint32_t classful(uint32_t address)
{
    uint32_t a = ntohl(address);
    return htonl(a >> 31 == 0 ? 0xff000000u : a >> 30 == 2 ? 0xffff0000u : 0xffffff00u);
}
int main(int argc, char **argv)
{
    int fd = netconfig_socket();
    if (fd < 0) { netconfig_fail("ifconfig", NULL); return 1; }
    if (argc == 1 || (argc == 2 && !strcmp(argv[1], "-a"))) {
        struct nx_ifinfo r;
        for (unsigned i = 0;; ++i) {
            memset(&r, 0, sizeof(r));
            r.index = i;
            if (ioctl(fd, NX_SIOCGIFINFO, &r) < 0) break;
            show(&r);
        }
        return errno == ENXIO ? 0 : (netconfig_fail("ifconfig", NULL), 1);
    }
    if (argv[1][0] == '-' || strlen(argv[1]) >= NX_IFNAMSIZ) return usage();
    struct nx_ifconfig c;
    memset(&c, 0, sizeof(c));
    strcpy(c.name, argv[1]);
    int have_mask = 0;
    for (int i = 2; i < argc; ++i) {
        const char *word = argv[i];
        if (!strcmp(word, "up") || !strcmp(word, "down")) {
            if (c.change & (NX_IFC_UP | NX_IFC_DOWN)) return usage();
            c.change |= word[0] == 'u' ? NX_IFC_UP : NX_IFC_DOWN;
        } else if (!strcmp(word, "delete")) {
            if (c.change & (NX_IFC_ADDRESS | NX_IFC_CLEAR)) return usage();
            c.change |= NX_IFC_CLEAR;
        } else if (!strcmp(word, "netmask")) {
            if (have_mask || ++i == argc || netconfig_mask(argv[i], &c.mask) < 0) {
                fprintf(stderr, "ifconfig: bad netmask %s\n", i < argc ? argv[i] : "");
                return 2;
            }
            have_mask = 1;
        } else {
            char address[INET_ADDRSTRLEN + 4], *slash;
            if (c.change & (NX_IFC_ADDRESS | NX_IFC_CLEAR) || strlen(word) >= sizeof(address)) return usage();
            strcpy(address, word);
            if ((slash = strchr(address, '/'))) {
                if (have_mask || netconfig_mask(slash, &c.mask) < 0) {
                    fprintf(stderr, "ifconfig: bad prefix %s\n", slash);
                    return 2;
                }
                *slash = 0;
                have_mask = 1;
            }
            if (inet_pton(AF_INET, address, &c.addr) != 1) {
                fprintf(stderr, "ifconfig: bad address %s\n", address);
                return 2;
            }
            c.change |= NX_IFC_ADDRESS;
        }
    }
    if (have_mask && !(c.change & NX_IFC_ADDRESS)) return usage();
    if (!c.change) {   /* show one interface */
        struct nx_ifinfo r;
        memset(&r, 0, sizeof(r));
        strcpy(r.name, argv[1]);
        if (ioctl(fd, NX_SIOCGIFINFO, &r) < 0) { netconfig_fail("ifconfig", argv[1]); return 1; }
        show(&r);
        return 0;
    }
    if ((c.change & NX_IFC_ADDRESS) && !have_mask) c.mask = classful(c.addr);
    if (ioctl(fd, NX_SIOCSIFCONFIG, &c) < 0) { netconfig_fail("ifconfig", argv[1]); return 1; }
    return 0;
}
