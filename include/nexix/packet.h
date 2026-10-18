#ifndef NEXIX_ABI_PACKET_H
#define NEXIX_ABI_PACKET_H
#include <stdint.h>
#define NX_PK_SYN 020
#define NX_PK_CHECK 0125252
#define NX_PK_MAX_SIZE 512
#define NX_PK_WINDOW 2
#define NX_PK_LINES 8
#define NX_PK_LIVE 010
#define NX_PK_DOWN 02000
#define NX_DIOCGETP 0x6408
/* Native piocb: same integer field meanings, explicit padding. */
struct nx_piocb { uint32_t discipline; int16_t psize,mode,state; uint8_t window,reserved; };
_Static_assert(sizeof(struct nx_piocb)==12,"packet ioctl ABI");
#endif
