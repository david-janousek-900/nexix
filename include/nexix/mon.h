#ifndef NEXIX_ABI_MON_H
#define NEXIX_ABI_MON_H
#include <stdint.h>
#include <stddef.h>
#define NX_MON_MAGIC "NXMON\0\0"
#define NX_MON_VERSION 1
#define NX_MON_MACHINE 62
#define NX_MON_HEADER_SIZE 64
#define NX_MON_CALL_SIZE 16
#define NX_MON_DEFAULT_CALLS 300
/* Versioned little-endian file records; use memcpy/decoding for unaligned data. */
struct nx_mon_header {
    char magic[8];
    uint32_t version;
    uint16_t machine,header_size;
    uint64_t low_pc,high_pc;
    uint32_t tick_hz,scale,call_capacity,call_used;
    uint64_t histogram_bytes;
    uint32_t call_record_size;
    uint16_t counter_size,reserved;
};
struct nx_mon_call {uint64_t pc,count;};
_Static_assert(sizeof(struct nx_mon_header)==64,"monitor header size");
_Static_assert(offsetof(struct nx_mon_header,histogram_bytes)==48,"monitor histogram offset");
_Static_assert(sizeof(struct nx_mon_call)==16,"monitor call record size");
#endif
