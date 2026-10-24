/* Ring-3 checks of the socket ABI boundary (task 424): every argument error
 * is reported before descriptors are consulted and invalid pointers are
 * EFAULT. Runs as a non-root user inside the boot self-test, so it never
 * creates a socket (that would start the stack before its own self-test);
 * live sockets are tested on a normal boot by tests/socket-api.c. */
#include <resolv.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>
#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)
#define FAILS(call, e) (errno = 0, (call) == -1 && errno == (e))
static const socklen_t read_only_length = sizeof(struct sockaddr_in);
static const struct sockaddr_in read_only_address = { AF_INET, 0, {0}, {0} };
int socket_test(void)
{
    void *unmapped = (void *)(uintptr_t)0x700000000000ull, *kernel = (void *)(uintptr_t)0x100000;
    void *noncanonical = (void *)(uintptr_t)0x800000000000ull;
    /* socket(): family, type, protocol and privilege. */
    CHECK(FAILS(socket(AF_UNSPEC, SOCK_STREAM, 0), EAFNOSUPPORT) && FAILS(socket(10, SOCK_STREAM, 0), EAFNOSUPPORT));
    CHECK(FAILS(socket(AF_INET, 0, 0), EPROTOTYPE) && FAILS(socket(AF_INET, 5, 0), EPROTOTYPE));
    CHECK(FAILS(socket(AF_INET, SOCK_STREAM, IPPROTO_UDP), EPROTONOSUPPORT));
    CHECK(FAILS(socket(AF_INET, SOCK_DGRAM, IPPROTO_TCP), EPROTONOSUPPORT));
    CHECK(FAILS(socket(AF_INET, SOCK_RAW, 0), EPROTONOSUPPORT) && FAILS(socket(AF_INET, SOCK_RAW, IPPROTO_ICMP), EACCES));
    /* High bits of the 64-bit argument slots are ignored for int arguments. */
    CHECK(nx_syscall6(NX_SYS_SOCKET, 0x100000002L, 0x700000000L, 0x500000000L, -1, -1, -1) == -EPROTOTYPE);
    CHECK(nx_syscall6(NX_SYS_SOCKET, 0x10000000aL, 0x700000001L, 0, -1, -1, -1) == -EAFNOSUPPORT);

    struct sockaddr_in in = { AF_INET, htons(7), { htonl(INADDR_LOOPBACK) }, {0} };
    const struct sockaddr *a = (const struct sockaddr *)&in;
    int pair[2];
    CHECK(pipe(pair) == 0);
    int file = pair[0];   /* an open descriptor that is not a socket */
    CHECK(FAILS(bind(file, a, sizeof(in)), ENOTSOCK) && FAILS(bind(-1, a, sizeof(in)), EBADF));
    CHECK(FAILS(bind(19, a, sizeof(in)), EBADF) && FAILS(bind(20, a, sizeof(in)), EBADF));
    CHECK(FAILS(connect(file, a, sizeof(in)), ENOTSOCK) && FAILS(listen(file, 5), ENOTSOCK) && FAILS(listen(99, 5), EBADF));
    /* Input addresses: length, pointer, family; checked before the descriptor. */
    CHECK(FAILS(bind(-1, a, sizeof(in) - 1), EINVAL) && FAILS(bind(-1, a, NX_SOCKADDR_MAX + 1), EINVAL));
    CHECK(FAILS(bind(-1, NULL, sizeof(in)), EFAULT) && FAILS(bind(-1, (void *)1, sizeof(in)), EFAULT));
    CHECK(FAILS(bind(-1, unmapped, sizeof(in)), EFAULT) && FAILS(bind(-1, kernel, sizeof(in)), EFAULT));
    CHECK(FAILS(bind(-1, noncanonical, sizeof(in)), EFAULT));
    CHECK(FAILS(bind(-1, (const struct sockaddr *)&read_only_address, sizeof(in)), EBADF));   /* reading is fine */
    static struct sockaddr_storage big;
    big.ss_family = AF_INET;
    CHECK(FAILS(bind(file, (struct sockaddr *)&big, sizeof(big)), ENOTSOCK));   /* up to 128 bytes accepted */
    in.sin_family = AF_UNSPEC;
    CHECK(FAILS(bind(file, a, sizeof(in)), EAFNOSUPPORT) && FAILS(connect(file, a, sizeof(in)), ENOTSOCK));
    in.sin_family = 10;
    CHECK(FAILS(connect(file, a, sizeof(in)), EAFNOSUPPORT));
    in.sin_family = AF_INET;

    /* Output addresses: value-result length must be writable. */
    struct sockaddr_in out;
    socklen_t length = sizeof(out);
    struct sockaddr *o = (struct sockaddr *)&out;
    CHECK(FAILS(accept(file, o, &length), ENOTSOCK) && FAILS(accept(file, NULL, NULL), ENOTSOCK));
    CHECK(FAILS(accept(file, o, NULL), EFAULT) && FAILS(accept(file, o, (socklen_t *)&read_only_length), EFAULT));
    CHECK(FAILS(accept(file, o, unmapped), EFAULT) && FAILS(accept(file, (void *)&read_only_address, &length), EFAULT));
    length = NX_SOCKADDR_MAX + 1;
    CHECK(FAILS(accept(file, o, &length), EINVAL));
    length = 0;
    CHECK(FAILS(accept(file, (void *)&read_only_address, &length), ENOTSOCK));   /* nothing would be written */
    length = sizeof(out);
    CHECK(FAILS(getsockname(file, o, &length), ENOTSOCK) && FAILS(getpeername(-1, o, &length), EBADF));
    CHECK(FAILS(getsockname(file, NULL, &length), EFAULT) && FAILS(getpeername(file, o, NULL), EFAULT));
    CHECK(FAILS(getsockname(file, kernel, &length), EFAULT));

    /* shutdown */
    CHECK(FAILS(shutdown(file, SHUT_RDWR), ENOTSOCK) && FAILS(shutdown(file, 3), EINVAL) && FAILS(shutdown(file, -1), EINVAL));

    /* Data transfer: flags, buffers, optional addresses. */
    char data[64] = "payload";
    CHECK(FAILS(send(file, data, 7, 0), ENOTSOCK) && FAILS(recv(file, data, sizeof(data), MSG_PEEK | MSG_DONTWAIT), ENOTSOCK));
    CHECK(FAILS(send(file, data, 7, MSG_NOSIGNAL | MSG_WAITALL), ENOTSOCK));
    CHECK(FAILS(send(file, data, 7, 1), EOPNOTSUPP) && FAILS(recv(file, data, 7, 0x100), EOPNOTSUPP));
    CHECK(FAILS(send(file, unmapped, 7, 0), EFAULT) && FAILS(recv(file, (void *)"const", 5, 0), EFAULT));
    CHECK(FAILS(send(file, NULL, 0, 0), ENOTSOCK) && FAILS(recv(file, NULL, 0, 0), ENOTSOCK));
    CHECK(FAILS(send(file, data, (size_t)-1, 0), EINVAL));
    CHECK(FAILS(sendto(file, data, 7, 0, a, sizeof(in)), ENOTSOCK) && FAILS(sendto(file, data, 7, 0, a, 3), EINVAL));
    CHECK(FAILS(sendto(file, data, 7, 0, unmapped, sizeof(in)), EFAULT));
    in.sin_family = AF_UNSPEC;
    CHECK(FAILS(sendto(file, data, 7, 0, a, sizeof(in)), EAFNOSUPPORT));
    in.sin_family = AF_INET;
    length = sizeof(out);
    CHECK(FAILS(recvfrom(file, data, 7, 0, o, &length), ENOTSOCK) && FAILS(recvfrom(file, data, 7, 0, o, NULL), EFAULT));
    CHECK(FAILS(recvfrom(file, data, 7, 0, NULL, NULL), ENOTSOCK));

    /* Options: level/name, value size and range, pointers; SO_ERROR/SO_TYPE are read-only. */
    int one = 1, zero = 0;
    struct timeval timeout = { 1, 500000 };
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)), ENOTSOCK));
    CHECK(FAILS(setsockopt(file, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)), ENOTSOCK));
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), ENOTSOCK));
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, SO_ERROR, &one, sizeof(one)), ENOPROTOOPT));
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, 0x0008, &one, sizeof(one)), ENOPROTOOPT));   /* SO_KEEPALIVE */
    CHECK(FAILS(setsockopt(file, IPPROTO_IP, 1, &one, sizeof(one)), ENOPROTOOPT));
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, SO_REUSEADDR, &one, 2), EINVAL));
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, SO_SNDBUF, &zero, sizeof(zero)), EINVAL));
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, SO_SNDTIMEO, &timeout, 8), EINVAL));
    timeout.tv_usec = 1000000;
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)), EDOM));
    timeout.tv_usec = 0; timeout.tv_sec = -1;
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)), EDOM));
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, SO_REUSEADDR, NULL, sizeof(one)), EFAULT));
    CHECK(FAILS(setsockopt(file, SOL_SOCKET, SO_REUSEADDR, unmapped, sizeof(one)), EFAULT));
    int value;
    socklen_t value_length = sizeof(value);
    CHECK(FAILS(getsockopt(file, SOL_SOCKET, SO_ERROR, &value, &value_length), ENOTSOCK));
    CHECK(FAILS(getsockopt(file, SOL_SOCKET, SO_TYPE, &value, &value_length), ENOTSOCK));
    CHECK(FAILS(getsockopt(file, SOL_SOCKET, 0x0008, &value, &value_length), ENOPROTOOPT));
    CHECK(FAILS(getsockopt(file, SOL_SOCKET, SO_ERROR, &value, NULL), EFAULT));
    CHECK(FAILS(getsockopt(file, SOL_SOCKET, SO_ERROR, (void *)&read_only_length, &value_length), EFAULT));
    CHECK(FAILS(getsockopt(file, SOL_SOCKET, SO_ERROR, &value, (socklen_t *)&read_only_length), EFAULT));
    value_length = 3;
    CHECK(FAILS(getsockopt(file, SOL_SOCKET, SO_ERROR, &value, &value_length), EINVAL));
    value_length = sizeof(timeout);
    CHECK(FAILS(getsockopt(-1, SOL_SOCKET, SO_RCVTIMEO, &timeout, &value_length), EBADF));

    /* Error numbers and messages. */
    CHECK(ENOTSOCK == 36 && EINPROGRESS == 57 && sys_nerr == 58 && !strcmp(sys_errlist[ECONNRESET], "Connection reset by peer"));
    /* Byte order and conversions in the real library. */
    struct in_addr addr;
    char text[INET_ADDRSTRLEN];
    CHECK(htons(0x1234) == 0x3412 && ntohl(htonl(0x0a00020f)) == 0x0a00020f);
    CHECK(inet_aton("10.0.2.15", &addr) == 1 && addr.s_addr == htonl(0x0a00020f) && !strcmp(inet_ntoa(addr), "10.0.2.15"));
    CHECK(inet_addr("127.1") == htonl(INADDR_LOOPBACK) && inet_addr("x") == INADDR_NONE);
    CHECK(inet_pton(AF_INET, "192.0.2.1", &addr) == 1 && inet_pton(AF_INET, "192.0.2.01", &addr) == 0);
    CHECK(FAILS(inet_pton(AF_UNSPEC, "1.2.3.4", &addr), EAFNOSUPPORT));
    CHECK(inet_ntop(AF_INET, &addr, text, sizeof(text)) == text && !strcmp(text, "192.0.2.1"));
    CHECK(!inet_ntop(AF_INET, &addr, text, 9) && errno == ENOSPC);
    CHECK(close(pair[0]) == 0 && close(pair[1]) == 0 && FAILS(listen(file, 1), EBADF));
    CHECK(inet_network("192.0.2")==0xc00002);
    addr=inet_makeaddr(127,1);
    CHECK(inet_netof(addr)==127&&inet_lnaof(addr)==1);
    CHECK(!strcmp(hstrerror(HOST_NOT_FOUND),"Unknown host"));
    CHECK(!getservbyname(NULL,NULL)&&errno==EINVAL);
    CHECK(!getprotobyname(NULL)&&errno==EINVAL);
    CHECK(!getnetbyaddr(127,AF_UNSPEC)&&errno==EAFNOSUPPORT);
    sethostent(1); endhostent();
    errno=0;
    struct hostent *host_entry=gethostent();
    CHECK(host_entry ? (host_entry->h_addrtype==AF_INET&&host_entry->h_length==4) : errno!=0);
    endhostent();
    unsigned char dns[256]; char expanded[256];
    int dns_size=dn_comp("www.example.test",dns,sizeof(dns),NULL,NULL);
    CHECK(dns_size==18&&dn_expand(dns,dns+dns_size,dns,expanded,sizeof(expanded))==dns_size);
    CHECK(!strcmp(expanded,"www.example.test"));
    CHECK(res_mkquery(QUERY,"example.test",C_IN,T_A,NULL,0,NULL,dns,sizeof(dns))>0);
    /* Unassigned numbers after the socket calls stay invalid. */
    CHECK(nx_syscall6(NX_SYS_LAST + 1, 0, 0, 0, 0, 0, 0) == -EINVAL);
    return 0;
}
