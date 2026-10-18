#ifndef NEXIX_ABI_OUTPUT_H
#define NEXIX_ABI_OUTPUT_H
/* File-backed native output devices. Captures contain the exact device bytes. */
#include <stdint.h>
#define NX_CAT_DEV 0x1e00U
#define NX_VP_DEV 0x1e01U
#define NX_VS_DEV 0x1e02U
#define NX_VGETSTATE (('v'<<8)+0)
#define NX_VSETSTATE (('v'<<8)+1)
#define NX_VPRINT 0100U
#define NX_VPLOT 0200U
#define NX_VPPLOT 0400U
#define NX_VP_COMMAND 1U
#define NX_VP_DATA 2U
#define NX_VS_INPUT 0x6f02U
struct nx_vs_input {uint32_t count;uint8_t codes[11],reserved;};
_Static_assert(sizeof(struct nx_vs_input)==16,"VS input ABI");
#define NX_OUTPUT_ATTACH 0x6f01U /* argument: user pointer to existing empty file path */
#endif
