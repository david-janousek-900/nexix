#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "net/compat.h"
#include "net/wire.h"

/* Independent whole-buffer checksum oracle: wider accumulator, no chunk state. */
static uint16_t oracle(const uint8_t *data, size_t length)
{
    uint64_t sum = 0;
    for (size_t i = 0; i < length; ++i)
        sum += (uint64_t)data[i] << ((i & 1) ? 0 : 8);
    while (sum >> 16) sum = (sum & 65535) + (sum >> 16);
    return (uint16_t)~sum;
}
static uint16_t checksum(const void *data, size_t length)
{
    struct nx_net_checksum state;
    nx_net_checksum_init(&state);
    nx_net_checksum_update(&state, data, length);
    uint16_t result = nx_net_checksum_finish(&state);
    assert(result == nx_net_checksum_finish(&state));
    return result;
}
static void wire(void)
{
    uint8_t bytes[10];
    memset(bytes, 0xaa, sizeof(bytes));
    nx_net_put16(bytes + 1, 0x1234);
    nx_net_put32(bytes + 3, UINT32_C(0x89abcdef));
    const uint8_t expected[] = {0xaa, 0x12, 0x34, 0x89, 0xab, 0xcd, 0xef, 0xaa, 0xaa, 0xaa};
    assert(!memcmp(bytes, expected, sizeof(bytes)));
    assert(nx_net_get16(bytes + 1) == 0x1234);
    assert(nx_net_get32(bytes + 3) == UINT32_C(0x89abcdef));
    for (uint32_t i = 0; i <= UINT16_MAX; ++i) {
        uint16_t network = nx_net_htons((uint16_t)i);
        memcpy(bytes + 1, &network, sizeof(network));
        assert(bytes[1] == (i >> 8) && bytes[2] == (i & 255));
        assert(nx_net_ntohs(network) == i);
    }
    const uint32_t values[] = {0, 1, UINT32_MAX, UINT32_C(0x80000000), UINT32_C(0x12345678)};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        uint32_t network = nx_net_htonl(values[i]);
        memcpy(bytes + 1, &network, sizeof(network));
        assert(nx_net_get32(bytes + 1) == values[i]);
        assert(nx_net_ntohl(network) == values[i]);
    }
    /* Independent field positions, including metadata-free pseudoheader. */
    assert(offsetof(struct nx_net_ether_wire, type) == 12);
    assert(offsetof(struct nx_net_arp_wire, sender_ip) == 14);
    assert(offsetof(struct nx_net_arp_wire, target_ip) == 24);
    assert(offsetof(struct nx_net_ip_wire, checksum) == 10);
    assert(offsetof(struct nx_net_tcp_wire, offset_reserved) == 12);
    assert(offsetof(struct nx_net_udp_wire, checksum) == 6);
    assert(offsetof(struct nx_net_pseudo_wire, protocol) == 9);
    assert(_Alignof(struct nx_net_ip_wire) == 1);
    assert(_Alignof(struct nx_net_tcp_wire) == 1);
}
static void sizes(void)
{
    size_t result = 123;
    assert(!nx_net_size_add(SIZE_MAX, 1, &result) && result == 123);
    assert(!nx_net_size_mul(SIZE_MAX, 2, &result) && result == 123);
    assert(!nx_net_size_align(SIZE_MAX, 2, &result) && result == 123);
    assert(!nx_net_size_align(12, 0, &result) && result == 123);
    assert(!nx_net_size_align(12, 3, &result) && result == 123);
    assert(nx_net_size_add(SIZE_MAX - 1, 1, &result) && result == SIZE_MAX);
    assert(nx_net_size_mul(SIZE_MAX, 1, &result) && result == SIZE_MAX);
    assert(nx_net_size_mul(SIZE_MAX, 0, &result) && result == 0);
    assert(nx_net_size_align(17, 16, &result) && result == 32);
    assert(nx_net_size_align(16, 16, &result) && result == 16);
    assert(nx_net_size_align(0, (size_t)1 << 63, &result) && result == 0);
    assert(nx_net_size_align(SIZE_MAX, 1, &result) && result == SIZE_MAX);
    assert(nx_net_size_add(UINT64_C(0x100000000), 7, &result));
    assert(result == UINT64_C(0x100000007));
}
static void sequences(void)
{
    const nx_net_seq_t earlier[] = {0, UINT32_MAX, UINT32_MAX - 15, 0x80000000u, 1};
    const nx_net_seq_t later[] = {1, 0, 16, 0x80000001u, 0x80000000u};
    for (size_t i = 0; i < sizeof(earlier) / sizeof(earlier[0]); ++i) {
        nx_net_seq_t a = earlier[i], b = later[i];
        assert(nx_net_seq_lt(a, b) && nx_net_seq_le(a, b));
        assert(nx_net_seq_gt(b, a) && nx_net_seq_ge(b, a));
        assert(!nx_net_seq_lt(b, a) && !nx_net_seq_ge(a, b));
        assert(!nx_net_seq_lt(a, a) && !nx_net_seq_gt(a, a));
        assert(nx_net_seq_le(a, a) && nx_net_seq_ge(a, a));
    }
    assert(!nx_net_seq_lt(0, 0x80000000u));
    assert(!nx_net_seq_lt(0x80000000u, 0));
    assert(!nx_net_seq_le(0, 0x80000000u));
    assert(!nx_net_seq_ge(0, 0x80000000u));
    nx_net_seq_t wrap = UINT32_MAX;
    ++wrap;
    assert(wrap == 0);
}
static void checksums(void)
{
    const uint8_t rfc1071[] = {0, 1, 0xf2, 3, 0xf4, 0xf5, 0xf6, 0xf7};
    assert(checksum(rfc1071, sizeof(rfc1071)) == 0x220d);
    assert(checksum(NULL, 0) == 0xffff);
    const uint8_t zero[] = {0, 0}, ones[] = {255, 255}, odd[] = {0x12};
    assert(checksum(zero, 2) == 0xffff);
    assert(checksum(ones, 2) == 0);
    assert(checksum(odd, 1) == 0xedff);
    uint8_t ip[] = {0x45, 0, 0, 0x73, 0, 0, 0x40, 0, 0x40, 0x11, 0, 0,
                    0xc0, 0xa8, 0, 1, 0xc0, 0xa8, 0, 0xc7};
    assert(checksum(ip, sizeof(ip)) == 0xb861);
    struct nx_net_ip_wire encoded = {.version_ihl = 0x45, .ttl = 64, .protocol = 17};
    nx_net_put16(encoded.length, 115);
    nx_net_put16(encoded.fragment, 0x4000);
    nx_net_put32(encoded.src, UINT32_C(0xc0a80001));
    nx_net_put32(encoded.dst, UINT32_C(0xc0a800c7));
    assert(!memcmp(&encoded, ip, sizeof(encoded)));
    nx_net_put16(encoded.checksum, checksum(&encoded, sizeof(encoded)));
    nx_net_put16(ip + 10, 0xb861);
    assert(!memcmp(&encoded, ip, sizeof(encoded)));
    assert(checksum(ip, sizeof(ip)) == 0);
    ip[12] ^= 1;
    assert(checksum(ip, sizeof(ip)) != 0);

    /* Every split, including odd-to-odd joins, unaligned spans and empty chunks.
     * Exact allocations let ASan catch reads beyond each logical packet. */
    for (size_t length = 0; length < 128; ++length) {
        for (size_t offset = 0; offset < 4; ++offset) {
            uint8_t *allocation = malloc(length + offset + 1);
            assert(allocation);
            uint8_t *p = allocation + offset + 1;
            for (size_t i = 0; i < length; ++i) p[i] = (uint8_t)(i * 97 + length);
            uint16_t expected = oracle(p, length);
            for (size_t split = 0; split <= length; ++split) {
                struct nx_net_checksum state;
                nx_net_checksum_init(&state);
                nx_net_checksum_update(&state, p, split);
                nx_net_checksum_update(&state, NULL, 0);
                nx_net_checksum_update(&state, p + split, length - split);
                assert(nx_net_checksum_finish(&state) == expected);
            }
            struct nx_net_checksum state;
            nx_net_checksum_init(&state);
            for (size_t i = 0; i < length; ++i) nx_net_checksum_update(&state, p + i, 1);
            assert(nx_net_checksum_finish(&state) == expected);
            free(allocation);
        }
    }
    size_t length = 1048577;
    uint8_t *large = malloc(length);
    assert(large);
    memset(large, 255, length);
    assert(checksum(large, length) == oracle(large, length));
    free(large);
}
static void pseudoheader(void)
{
    /* IPv4 pseudoheader + UDP datagram with a five-byte payload. */
    uint8_t packet[] = {192, 0, 2, 1, 198, 51, 100, 2, 0, 17, 0, 13,
                        0x04, 0xd2, 0x10, 0xe1, 0, 13, 0, 0, 'h', 'e', 'l', 'l', 'o'};
    struct nx_net_checksum state;
    nx_net_checksum_init(&state);
    nx_net_checksum_pseudo(&state, UINT32_C(0xc0000201), UINT32_C(0xc6336402), 17, 13);
    nx_net_checksum_update(&state, packet + 12, sizeof(packet) - 12);
    uint16_t sum = nx_net_checksum_finish(&state);
    assert(sum == oracle(packet, sizeof(packet)));
    nx_net_put16(packet + 18, sum);
    assert(checksum(packet, sizeof(packet)) == 0);
    packet[8] = 1; /* A pseudoheader corruption must invalidate the checksum. */
    assert(checksum(packet, sizeof(packet)) != 0);
}
int main(void)
{
    wire(); sizes(); sequences(); checksums(); pseudoheader();
    return 0;
}
