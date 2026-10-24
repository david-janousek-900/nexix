/* Native numeric-address ping over a raw ICMP socket (task 428). Installed
 * setuid root: the socket is opened first, then privilege is dropped. */
#include "../netconfig.h"
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/timeb.h>
#define MAXDATA 1472   /* one Ethernet frame */
static volatile sig_atomic_t stop;
static int numeric;
static void interrupt(int n) { (void)n; stop = 1; }
static int usage(void)
{
    fputs("usage: ping [-n] [-c count] [-i seconds] [-W seconds] [-s size] address\n", stderr);
    return 2;
}
static long now_ms(void)
{
    struct timeb t;
    ftime(&t);
    return (long)t.time * 1000 + t.millitm;
}
static int number(const char *text, long low, long high, long *out)
{
    char *end;
    long value = strtol(text, &end, 10);
    if (end == text || *end || value < low || value > high) return -1;
    *out = value;
    return 0;
}
static unsigned short checksum(const unsigned char *p, size_t n)
{
    unsigned long sum = 0;
    for (size_t i = 0; i + 1 < n; i += 2) sum += (unsigned)(p[i] << 8 | p[i + 1]);
    if (n & 1) sum += (unsigned)p[n - 1] << 8;
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short)~sum;
}
static const char *from_text(struct in_addr address,char *text)
{
    netconfig_text(address.s_addr,text);
    if(numeric)return text;
    static uint32_t cached;static int have_cache;static char label[1200];
    if(!have_cache||cached!=address.s_addr){
        struct hostent *host=gethostbyaddr(&address,4,AF_INET);
        if(host)snprintf(label,sizeof(label),"%s (%s)",host->h_name,text);
        else snprintf(label,sizeof(label),"%s",text);
        cached=address.s_addr;have_cache=1;
    }
    return label;
}
static long sent_at[1024];
static unsigned char seen[65536 / 8];
static long transmitted, received, duplicates, rtt_min = -1, rtt_max, rtt_sum;
static unsigned short ident;

/* One received datagram (IP header included): a reply to us, or an error
 * quoting one of our requests. */
static void reply(const unsigned char *b, ssize_t n, const struct sockaddr_in *from)
{
    char text[INET_ADDRSTRLEN];
    size_t hl = (size_t)(b[0] & 0xf) * 4;
    if (n < (ssize_t)hl + 8) return;
    const unsigned char *icmp = b + hl;
    if (icmp[0] == 0 && (icmp[4] << 8 | icmp[5]) == ident) {
        unsigned seq = (unsigned)(icmp[6] << 8 | icmp[7]);
        int duplicate = seen[seq / 8] & (1 << seq % 8);
        long rtt = now_ms() - sent_at[seq % 1024];
        seen[seq / 8] |= (unsigned char)(1 << seq % 8);
        if (duplicate) ++duplicates;
        else {
            ++received;
            rtt_sum += rtt;
            if (rtt_min < 0 || rtt < rtt_min) rtt_min = rtt;
            if (rtt > rtt_max) rtt_max = rtt;
        }
        printf("%ld bytes from %s: icmp_seq=%u ttl=%d time=%ld ms%s\n", (long)(n - (ssize_t)hl),
               from_text(from->sin_addr, text), seq, b[8], rtt, duplicate ? " (DUP!)" : "");
        return;
    }
    /* Unreachable or time exceeded, quoting our echo request. */
    if ((icmp[0] == 3 || icmp[0] == 11) && n >= (ssize_t)hl + 8 + 20 + 8) {
        const unsigned char *inner = icmp + 8;
        size_t ihl = (size_t)(inner[0] & 0xf) * 4;
        if (n < (ssize_t)(hl + 8 + ihl + 8) || inner[9] != 1) return;
        const unsigned char *echo = inner + ihl;
        if (echo[0] != 8 || (echo[4] << 8 | echo[5]) != ident) return;
        static const char *unreachable[] = {"Net Unreachable", "Host Unreachable", "Protocol Unreachable",
                                            "Port Unreachable", "Fragmentation Needed"};
        const char *what = icmp[0] == 11 ? "Time to live exceeded"
                         : icmp[1] < 5 ? unreachable[icmp[1]] : "Destination Unreachable";
        printf("From %s icmp_seq=%u %s\n", from_text(from->sin_addr, text),
               (unsigned)(echo[6] << 8 | echo[7]), what);
    }
}
/* Receive until the deadline (ms), handling every reply that arrives. */
static void collect(int fd, long deadline)
{
    unsigned char b[20 + 8 + MAXDATA + 64];
    for (long left; !stop && (left = deadline - now_ms()) > 0;) {
        struct timeval wait = { left / 1000, left % 1000 * 1000 };
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait)) < 0) return;
        struct sockaddr_in from;
        socklen_t length = sizeof(from);
        ssize_t n = recvfrom(fd, b, sizeof(b), 0, (struct sockaddr *)&from, &length);
        if (n < 0) { if (errno == EAGAIN) return; if (errno == EINTR) continue; return; }
        reply(b, n, &from);
    }
}
int main(int argc, char **argv)
{
    long count = -1, interval = 1, linger = 1, size = 56;
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; ++i) {
        if(!strcmp(argv[i],"-n")){numeric=1;continue;}
        long *target = !strcmp(argv[i], "-c") ? &count : !strcmp(argv[i], "-i") ? &interval
                     : !strcmp(argv[i], "-W") ? &linger : !strcmp(argv[i], "-s") ? &size : NULL;
        long high = target == &size ? MAXDATA : target == &count ? 65535 : 60;
        if (!target || ++i == argc || number(argv[i], target == &size ? 0 : 1, high, target) < 0) return usage();
    }
    if (i != argc - 1) return usage();
    struct sockaddr_in to;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    int error = errno;
    if (setuid(getuid()) < 0) { netconfig_fail("ping", "setuid"); return 2; }   /* drop root at once */
    if (fd < 0) { errno = error; netconfig_fail("ping", "socket"); return 2; }
    if(netconfig_resolve("ping",argv[i],&to.sin_addr.s_addr))return 2;
    ident = (unsigned short)getpid();
    signal(SIGINT, interrupt);
    char text[INET_ADDRSTRLEN];
    printf("PING %s: %ld data bytes\n", netconfig_text(to.sin_addr.s_addr, text), size);
    fflush(stdout);
    unsigned char packet[8 + MAXDATA];
    for (long seq = 0; !stop && (count < 0 || seq < count); ++seq) {
        memset(packet, 0, 8);
        packet[0] = 8;
        packet[4] = (unsigned char)(ident >> 8); packet[5] = (unsigned char)ident;
        packet[6] = (unsigned char)(seq >> 8); packet[7] = (unsigned char)seq;
        for (long k = 0; k < size; ++k) packet[8 + k] = (unsigned char)k;
        unsigned short sum = checksum(packet, (size_t)(8 + size));
        packet[2] = (unsigned char)(sum >> 8); packet[3] = (unsigned char)sum;
        sent_at[seq % 1024] = now_ms();
        if (sendto(fd, packet, (size_t)(8 + size), 0, (struct sockaddr *)&to, sizeof(to)) < 0) {
            if (errno == EINTR) break;
            netconfig_fail("ping", "sendto");
            return 2;
        }
        ++transmitted;
        int last = count >= 0 && seq == count - 1;
        collect(fd, sent_at[seq % 1024] + (last ? linger : interval) * 1000);
        fflush(stdout);
    }
    printf("--- %s ping statistics ---\n", text);
    long lost = transmitted ? (transmitted - received) * 1000 / transmitted : 0;
    printf("%ld packets transmitted, %ld packets received, ", transmitted, received);
    if (duplicates) printf("+%ld duplicates, ", duplicates);
    printf("%ld.%ld%% packet loss\n", lost / 10, lost % 10);
    if (received)   /* 10 ms clock resolution */
        printf("round-trip min/avg/max = %ld/%ld/%ld ms\n", rtt_min, rtt_sum / received, rtt_max);
    return received ? 0 : 1;
}
