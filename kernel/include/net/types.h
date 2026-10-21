#ifndef NEXIX_NET_TYPES_H
#define NEXIX_NET_TYPES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Private kernel types, never a BSD sys/types.h replacement or user ABI.
 * Addresses and protocol integers are host-order values; wire bytes live in
 * net/wire.h. Historical u_long protocol fields must use explicit widths.
 * Native lengths, pointer arithmetic and counters use size_t/uintptr_t/uint64_t.
 */
typedef uint32_t nx_net_addr_t;
typedef uint32_t nx_net_seq_t;
typedef uint16_t nx_net_port_t;

_Static_assert(__CHAR_BIT__ == 8, "network octets require eight-bit bytes");
_Static_assert(sizeof(int) == 4 && sizeof(long) == 8 && sizeof(void *) == 8,
               "Nexix networking targets native x86_64 LP64");
_Static_assert(sizeof(nx_net_addr_t) == 4 && sizeof(nx_net_seq_t) == 4 &&
               sizeof(nx_net_port_t) == 2, "protocol types must not widen with long");
_Static_assert(sizeof(size_t) == 8 && sizeof(uintptr_t) == 8,
               "native lengths and pointers must not narrow");
#endif
