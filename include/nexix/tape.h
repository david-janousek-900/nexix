#ifndef NEXIX_ABI_TAPE_H
#define NEXIX_ABI_TAPE_H
#include <stdint.h>
#define NX_TAPE_HT 0U
#define NX_TAPE_TM 1U
#define NX_TAPE_BLOCK_MAJOR(k) (17U+(k))
#define NX_TAPE_RAW_MAJOR(k) (19U+(k))
#define NX_TAPE_CONTROL_DEV 0x1500U
#define NX_TAPE_NOREWIND 0200U
#define NX_TAPE_800 0100U
#define NX_TAPE_DEV(k,u,raw) (((raw)?NX_TAPE_RAW_MAJOR(k):NX_TAPE_BLOCK_MAJOR(k))*256U+(u))
#define NX_TAPE_LOAD 0x6d01U
#define NX_TAPE_UNLOAD 0x6d02U
#define NX_TAPE_STATUS 0x6d03U
#define NX_TAPE_OPERATION 0x6d04U
#define NX_TAPE_READONLY 1U
#define NX_TAPE_INITIALIZE 2U
#define NX_TAPE_REWIND 1U
#define NX_TAPE_FORWARD_RECORD 2U
#define NX_TAPE_BACK_RECORD 3U
#define NX_TAPE_FORWARD_FILE 4U
#define NX_TAPE_BACK_FILE 5U
#define NX_TAPE_WRITE_MARK 6U
#define NX_TAPE_ERASE 7U
#define NX_TAPE_ONLINE 1U
#define NX_TAPE_OPEN 2U
#define NX_TAPE_WRITABLE 4U
#define NX_TAPE_ERROR 8U
#define NX_TAPE_MAX_RECORD 65534U
#define NX_TAPE_MAX_IMAGE (64ULL*1024*1024)
#define NX_TAPE_MAX_RECORDS 1000000U
struct nx_tape_load {uint32_t family,unit,flags,reserved;uint64_t path,capacity;};
struct nx_tape_operation {uint32_t operation,count;};
struct nx_tape_status {uint32_t family,unit,flags,density;uint64_t record,position,records,end,capacity,residual;};
_Static_assert(sizeof(struct nx_tape_load)==32,"tape load ABI");
_Static_assert(sizeof(struct nx_tape_status)==64,"tape status ABI");
#endif
