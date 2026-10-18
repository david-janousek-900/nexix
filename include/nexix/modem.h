#ifndef NEXIX_ABI_MODEM_H
#define NEXIX_ABI_MODEM_H
#include <stdint.h>
/* Native peer interface for historical DN/DU protocols, not PDP-11 registers. */
#define NX_DN_DEV(n) (0x1b00U+(n))
#define NX_DU_DEV 0x1c00U
#define NX_MODEMCTL_DEV 0x1d00U
#define NX_MDM_GET 0x6d01U
#define NX_MDM_SET 0x6d02U
#define NX_MDM_PEER 0x6d03U
#define NX_MDM_LOCAL 0U
#define NX_MDM_UART 1U
#define NX_MDM_VIRTUAL 2U
#define NX_MDM_DCD 1U
#define NX_MDM_CTS 2U
#define NX_MDM_DSR 4U
#define NX_MDM_RI 8U
#define NX_MDM_SECONDARY 16U
#define NX_MDM_DTR 0x100U
#define NX_MDM_RTS 0x200U
#define NX_PEER_STATUS 0U
#define NX_PEER_SIGNALS 1U
#define NX_PEER_TAKE 2U
#define NX_PEER_RECEIVE 3U
#define NX_PEER_END 4U
#define NX_PEER_TTY 5U
#define NX_DN_POWER 1U
#define NX_DN_ABANDON 2U
#define NX_DN_OCCUPIED 4U
#define NX_DN_CONNECTED 8U
#define NX_DN_READY 16U
#define NX_MODEM_OPEN 1U
#define NX_MODEM_READ 2U
#define NX_MODEM_WRITE 4U
#define NX_MODEM_PENDING 8U
struct nx_modem_tty { uint32_t mode,signals,outputs,reserved; };
struct nx_modem_peer {
    uint32_t device,operation,signals,count,state,reserved;
    uint8_t data[512];
};
_Static_assert(sizeof(struct nx_modem_tty)==16,"modem tty ABI");
_Static_assert(sizeof(struct nx_modem_peer)==536,"modem peer ABI");
#endif
