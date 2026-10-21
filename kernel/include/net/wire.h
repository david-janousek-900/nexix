#ifndef NEXIX_NET_WIRE_H
#define NEXIX_NET_WIRE_H
#include "net/types.h"

/* Byte arrays avoid alignment, strict-aliasing and compiler bitfield hazards.
 * Helpers require the indicated number of accessible bytes; packet parsers
 * must validate lengths before calling. They accept unaligned addresses.
 */
static inline uint16_t nx_net_get16(const void *address)
{
    const uint8_t *p = address;
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}
static inline uint32_t nx_net_get32(const void *address)
{
    const uint8_t *p = address;
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | p[3];
}
static inline void nx_net_put16(void *address, uint16_t value)
{
    uint8_t *p = address;
    p[0] = (uint8_t)(value >> 8); p[1] = (uint8_t)value;
}
static inline void nx_net_put32(void *address, uint32_t value)
{
    uint8_t *p = address;
    p[0] = (uint8_t)(value >> 24); p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8); p[3] = (uint8_t)value;
}
static inline uint16_t nx_net_htons(uint16_t value)
{ return __builtin_bswap16(value); }
static inline uint32_t nx_net_htonl(uint32_t value)
{ return __builtin_bswap32(value); }
static inline uint16_t nx_net_ntohs(uint16_t value)
{ return nx_net_htons(value); }
static inline uint32_t nx_net_ntohl(uint32_t value)
{ return nx_net_htonl(value); }
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Nexix networking byte swaps require the supported little-endian target"
#endif

struct nx_net_ether_wire { uint8_t dst[6], src[6], type[2]; };
struct nx_net_arp_wire {
    uint8_t hardware[2], protocol[2], hardware_len, protocol_len, operation[2];
    uint8_t sender_hardware[6], sender_ip[4], target_hardware[6], target_ip[4];
};
/* Base IPv4/TCP headers only; options follow in separately bounded bytes. */
struct nx_net_ip_wire {
    uint8_t version_ihl, tos, length[2], id[2], fragment[2], ttl, protocol;
    uint8_t checksum[2], src[4], dst[4];
};
struct nx_net_tcp_wire {
    uint8_t src[2], dst[2], sequence[4], ack[4], offset_reserved, flags;
    uint8_t window[2], checksum[2], urgent[2];
};
struct nx_net_udp_wire { uint8_t src[2], dst[2], length[2], checksum[2]; };
struct nx_net_icmp_wire { uint8_t type, code, checksum[2], rest[4]; };
struct nx_net_pseudo_wire { uint8_t src[4], dst[4], zero, protocol, length[2]; };

_Static_assert(sizeof(struct nx_net_ether_wire) == 14, "Ethernet header length");
_Static_assert(sizeof(struct nx_net_arp_wire) == 28, "Ethernet/IPv4 ARP length");
_Static_assert(sizeof(struct nx_net_ip_wire) == 20, "base IPv4 header length");
_Static_assert(sizeof(struct nx_net_tcp_wire) == 20, "base TCP header length");
_Static_assert(sizeof(struct nx_net_udp_wire) == 8, "UDP header length");
_Static_assert(sizeof(struct nx_net_icmp_wire) == 8, "ICMP base message length");
_Static_assert(sizeof(struct nx_net_pseudo_wire) == 12, "IPv4 pseudoheader length");
_Static_assert(offsetof(struct nx_net_ip_wire, src) == 12 &&
               offsetof(struct nx_net_ip_wire, dst) == 16, "IPv4 address offsets");
_Static_assert(offsetof(struct nx_net_tcp_wire, sequence) == 4 &&
               offsetof(struct nx_net_tcp_wire, checksum) == 16, "TCP field offsets");
_Static_assert(offsetof(struct nx_net_pseudo_wire, length) == 10,
               "pseudoheader must contain no queue pointers");
#endif
