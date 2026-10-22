/* Host test of libc/inet.c, built against the native headers with renamed
 * symbols (task 424). */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct nx_in_addr { uint32_t s_addr; };
uint32_t nx_test_htonl(uint32_t);
uint16_t nx_test_htons(uint16_t);
uint32_t nx_test_ntohl(uint32_t);
uint16_t nx_test_ntohs(uint16_t);
uint32_t nx_test_inet_addr(const char *);
int nx_test_inet_aton(const char *, struct nx_in_addr *);
char *nx_test_inet_ntoa(struct nx_in_addr);
int nx_test_inet_pton(int, const char *, void *);
const char *nx_test_inet_ntop(int, const void *, char *, uint32_t);
int nx_test_errno;
#define AF_INET 2
#define EAFNOSUPPORT 43
#define ENOSPC 28
static uint32_t be(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    uint8_t bytes[4] = {a, b, c, d};
    uint32_t v;
    memcpy(&v, bytes, 4);
    return v;
}
static int aton(const char *s, uint32_t *out) { struct nx_in_addr a = {0xdeadbeef}; int r = nx_test_inet_aton(s, &a); *out = a.s_addr; return r; }
int main(void)
{
    assert(nx_test_htons(0x1234) == 0x3412 && nx_test_ntohs(0x3412) == 0x1234);
    assert(nx_test_htonl(0x01020304) == 0x04030201 && nx_test_ntohl(nx_test_htonl(0xdeadbeef)) == 0xdeadbeef);
    uint32_t v;
    /* BSD inet_aton forms: a.b.c.d, a.b.c (16-bit last), a.b (24-bit), a (32-bit), octal and hex parts. */
    assert(aton("127.0.0.1", &v) && v == be(127, 0, 0, 1));
    assert(aton("10.0.2.15\n", &v) && v == be(10, 0, 2, 15));
    assert(aton("127.1", &v) && v == be(127, 0, 0, 1));
    assert(aton("10.0.513", &v) && v == be(10, 0, 2, 1));
    assert(aton("0x7f.1", &v) && v == be(127, 0, 0, 1));
    assert(aton("012.0.0.010", &v) && v == be(10, 0, 0, 8));
    assert(aton("3232235777", &v) && v == be(192, 168, 1, 1));
    assert(aton("255.255.255.255", &v) && v == 0xffffffffu);
    const char *bad[] = {"", ".", "1.", "1..2", "256.0.0.1", "1.2.3.4.5", "1.2.3.256", "08", "0x", "1.2.65536",
                         "4294967296", "1.2.3.4x", "-1", " 1.2.3.4", "1.16777216", "0xg"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) assert(!aton(bad[i], &v) && v == 0xdeadbeef);
    assert(nx_test_inet_aton("1.2.3.4", NULL) == 1);   /* validation only */
    assert(nx_test_inet_addr("192.0.2.1") == be(192, 0, 2, 1) && nx_test_inet_addr("bogus") == 0xffffffffu);
    /* inet_pton is strict dotted decimal. */
    uint8_t b[4] = {9, 9, 9, 9};
    assert(nx_test_inet_pton(AF_INET, "192.0.2.255", b) == 1 && b[0] == 192 && b[3] == 255);
    assert(nx_test_inet_pton(AF_INET, "0.0.0.0", b) == 1 && !b[0] && !b[3]);
    const char *strict[] = {"127.1", "01.2.3.4", "1.2.3.04", "1.2.3", "1.2.3.4 ", "0x1.2.3.4", "256.1.1.1", "", "1.2.3.4.5"};
    for (unsigned i = 0; i < sizeof(strict) / sizeof(strict[0]); ++i) {
        memset(b, 7, 4);
        assert(nx_test_inet_pton(AF_INET, strict[i], b) == 0 && b[0] == 7 && b[3] == 7);
    }
    nx_test_errno = 0;
    assert(nx_test_inet_pton(10, "::1", b) == -1 && nx_test_errno == EAFNOSUPPORT);
    /* inet_ntop needs room for the terminator; inet_ntoa uses a static buffer. */
    char text[16];
    uint8_t a[4] = {255, 0, 10, 199};
    assert(nx_test_inet_ntop(AF_INET, a, text, sizeof(text)) == text && !strcmp(text, "255.0.10.199"));
    assert(nx_test_inet_ntop(AF_INET, a, text, 13) == text);
    nx_test_errno = 0;
    assert(!nx_test_inet_ntop(AF_INET, a, text, 12) && nx_test_errno == ENOSPC);
    nx_test_errno = 0;
    assert(!nx_test_inet_ntop(10, a, text, 16) && nx_test_errno == EAFNOSUPPORT);
    uint8_t max[4] = {255, 255, 255, 255};
    assert(nx_test_inet_ntop(AF_INET, max, text, 16) && !strcmp(text, "255.255.255.255"));
    assert(!strcmp(nx_test_inet_ntoa((struct nx_in_addr){be(10, 0, 2, 2)}), "10.0.2.2"));
    /* Round trip every value of each byte position. */
    for (unsigned x = 0; x < 256; ++x) {
        uint8_t in[4] = {(uint8_t)x, (uint8_t)(255 - x), (uint8_t)(x / 2), (uint8_t)(x ^ 0x5a)}, out[4];
        assert(nx_test_inet_ntop(AF_INET, in, text, sizeof(text)) && nx_test_inet_pton(AF_INET, text, out) == 1 &&
               !memcmp(in, out, 4) && aton(text, &v) && !memcmp(&v, in, 4));
    }
    puts("PASS: byte order, BSD inet_aton/inet_addr forms, strict inet_pton, inet_ntop/inet_ntoa bounds and round trips");
    return 0;
}
