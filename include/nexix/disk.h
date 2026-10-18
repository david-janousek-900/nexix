#ifndef NEXIX_ABI_DISK_H
#define NEXIX_ABI_DISK_H
#define NX_DISK_HP 0U
#define NX_DISK_RF 1U
#define NX_DISK_RK 2U
#define NX_DISK_RL 3U
#define NX_DISK_RP 4U
#define NX_DISK_KINDS 5U
#define NX_DISK_BLOCK_MAJOR(kind) (7U+(kind))
#define NX_DISK_RAW_MAJOR(kind) (12U+(kind))
#define NX_DISK_DEV(kind,minor,raw) (((raw)?NX_DISK_RAW_MAJOR(kind):NX_DISK_BLOCK_MAJOR(kind))*256U+(minor))
#endif
