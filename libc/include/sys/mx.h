#ifndef NEXIX_LIBC_SYS_MX_H
#define NEXIX_LIBC_SYS_MX_H
#include <nexix/mpx.h>
#define NDEBUGS 30
#define MCCLOSE 29
#define MCOPEN 28
#define ALL 27
#define SCON 26
#define MSREAD 25
#define SDATA 24
#define MCREAD 23
#define MCWRITE 22
#define CNTLSIZ 10
#define NINDEX NX_MX_INDEX
#define DO_BLK 7
#define DO_UBLK 8
#define NGROUPS NX_MX_GROUPS
#define NCHANS NX_MX_CHANNELS
#define NPORTS NX_MX_PORTS
#define NLEVELS NX_MX_LEVELS
#define NMSIZE NX_MX_NAME
#define MPX NX_MX_MPX
#define MPXN NX_MX_MPXN
#define CHAN NX_MX_CHAN
#define JOIN NX_MX_JOIN
#define EXTR NX_MX_EXTR
#define ATTACH NX_MX_ATTACH
#define CONNECT NX_MX_CONNECT
#define DETACH NX_MX_DETACH
#define DISCON NX_MX_DISCON
#define DEBUG NX_MX_DEBUG
#define NPGRP NX_MX_NPGRP
#define CSIG NX_MX_CSIG
#define PACK NX_MX_PACK
#define M_WATCH NX_M_WATCH
#define M_CLOSE NX_M_CLOSE
#define M_EOT NX_M_EOT
#define M_OPEN NX_M_OPEN
#define M_BLK NX_M_BLK
#define M_UBLK NX_M_UBLK
#define M_IOCTL NX_M_IOCTL
#define M_SIG NX_M_SIG
#define MXLSTN NX_MXLSTN
#define MXNBLK NX_MXNBLK
struct rh { uint16_t index,count,ccount; };
struct wh { uint16_t index,count; int16_t ccount; uint16_t reserved; char *data; };
struct mx_args { char *m_name; int32_t m_cmd,m_arg[3]; };
_Static_assert(sizeof(struct rh)==sizeof(struct nx_mx_read),"rh ABI");
_Static_assert(sizeof(struct wh)==sizeof(struct nx_mx_write),"wh ABI");
_Static_assert(sizeof(struct mx_args)==sizeof(struct nx_mx_args),"mx_args ABI");
int mpxcall(int command,const struct mx_args *arguments);
int mpx(const char *name,int mode);
int chan(int group);
int join(int fd,int group);
int connect(int fd,int channel,int side);
/* Native completion of the reference's unimplemented inverse. */
int disconnect(int fd,int channel,int side);
int attach(int index,int group);
int detach(int index,int group);
int extract(int index,int group,int side);
int debug(int variable,int value);
int npgrp(int index,int group,int process_group);
int ckill(int index,int group,int signal);
#endif
