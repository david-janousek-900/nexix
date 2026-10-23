/* Configuration ioctls at the syscall boundary (task 427): root-only changes,
 * open queries, bad pointers and records. Run as root after the commands
 * have configured em0 (or on a no-NIC boot, with argument "none"). */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#define CHECK(c) do { if (!(c)) { printf("netconfig-api failure line %d errno %d\n", __LINE__, errno); return 1; } } while (0)
#define FAILS(call, e) (errno = 0, (call) == -1 && errno == (e))
int main(int argc, char **argv)
{
    int nic = !(argc == 2 && !strcmp(argv[1], "none"));
    int fd = socket(AF_INET, SOCK_DGRAM, 0), p[2];
    CHECK(fd >= 0 && pipe(p) == 0);
    struct nx_ifinfo info;
    memset(&info, 0, sizeof(info));
    CHECK(ioctl(fd, NX_SIOCGIFINFO, &info) == 0 && !strcmp(info.name, "lo0") && info.index == 0);
    memset(&info, 0, sizeof(info));   /* by index: the name must be empty */
    info.index = nic ? 2 : 1;
    CHECK(FAILS(ioctl(fd, NX_SIOCGIFINFO, &info), ENXIO));
    /* Bad pointers, requests and descriptors. */
    CHECK(FAILS(ioctl(fd, NX_SIOCGIFINFO, NULL), EFAULT) && FAILS(ioctl(fd, NX_SIOCGROUTE, (void *)1), EFAULT));
    CHECK(FAILS(ioctl(fd, 0x69ff, &info), ENOTTY) && FAILS(ioctl(p[0], NX_SIOCGIFINFO, &info), ENOTTY));
    static const struct nx_ifinfo read_only = {0};
    CHECK(FAILS(ioctl(fd, NX_SIOCGIFINFO, (void *)&read_only), EFAULT));   /* a query writes back */
    /* Malformed records change nothing. */
    struct nx_ifconfig c;
    memset(&c, 0, sizeof(c));
    memcpy(c.name, "lo0toolong", 8);
    c.change = NX_IFC_UP;
    CHECK(FAILS(ioctl(fd, NX_SIOCSIFCONFIG, &c), EINVAL));
    strcpy(c.name, "lo0");
    c.change = 0;
    CHECK(FAILS(ioctl(fd, NX_SIOCSIFCONFIG, &c), EINVAL));
    c.change = NX_IFC_UP;
    c.reserved = 1;
    CHECK(FAILS(ioctl(fd, NX_SIOCSIFCONFIG, &c), EINVAL));
    struct nx_route route = { 0, 1 };
    CHECK(FAILS(ioctl(fd, NX_SIOCSROUTE, &route), EINVAL));
    struct nx_route_lookup lookup;
    memset(&lookup, 0, sizeof(lookup));
    lookup.dst = htonl(0xe0000001);
    CHECK(FAILS(ioctl(fd, NX_SIOCLOOKUP, &lookup), EINVAL));
    lookup.dst = htonl(INADDR_LOOPBACK);
    CHECK(ioctl(fd, NX_SIOCLOOKUP, &lookup) == 0 && !strcmp(lookup.ifname, "lo0") && (lookup.flags & NX_RT_LOCAL));
    /* An ordinary user may query but not change, through ioctls or the commands. */
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        if (setuid(42) != 0) _exit(2);
        memset(&info, 0, sizeof(info));
        strcpy(info.name, "lo0");
        if (ioctl(fd, NX_SIOCGIFINFO, &info) != 0 || ioctl(fd, NX_SIOCGROUTE, &route) != 0) _exit(3);
        c.reserved = 0;
        if (!FAILS(ioctl(fd, NX_SIOCSIFCONFIG, &c), EPERM)) _exit(4);
        route.flags = 0;
        if (!FAILS(ioctl(fd, NX_SIOCSROUTE, &route), EPERM)) _exit(5);
        if (dup2(p[1], 2) < 0 || dup2(p[1], 1) < 0) _exit(6);
        execl("/sbin/ifconfig", "ifconfig", "lo0", "down", (char *)NULL);
        _exit(7);
    }
    close(p[1]);
    char out[128];
    ssize_t n = read(p[0], out, sizeof(out) - 1);
    int status;
    CHECK(wait(&status) == child && WIFEXITED(status) && WEXITSTATUS(status) == 1);
    CHECK(n > 0 && (out[n] = 0, !strcmp(out, "ifconfig: lo0: Not owner\n")));
    memset(&info, 0, sizeof(info));
    strcpy(info.name, "lo0");
    CHECK(ioctl(fd, NX_SIOCGIFINFO, &info) == 0 && (info.flags & NX_IFF_UP));   /* still up */
    puts("NETCONFIG_API_OK");
    return 0;
}
