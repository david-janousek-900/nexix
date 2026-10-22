#include <nexix/dectape.h>
#include <nexix/modem.h>
#include <nexix/tape.h>
/* Independently specified LP64 record contracts, shared kernel/libc boundaries. */
#include <sys/types.h>
#include <mon.h>
#include <a.out.h>
#include <sys/kinfo.h>
#include <sys/phys.h>
#include <sys/pk.h>
#include <sys/acct.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/param.h>
#include <sys/times.h>
#include <sys/timeb.h>
#include <sgtty.h>
#include <signal.h>
#include <utmp.h>
#include <pwd.h>
#include <grp.h>
#include <setjmp.h>
#include <nexix/file.h>
#include <nexix/memdev.h>
#include <nexix/disk.h>
#include <nexix/tty.h>
#include "v7/types.h"
_Static_assert(NX_TIOCHPCL==0x7402 && NX_TIOCEXCL==0x740d && NX_TIOCNXCL==0x740e, "terminal ioctl numbers");
_Static_assert(NX_TTY_CONTROLLER_DEV(NX_TTY_KL,0)==0x1a00, "controller device number");
#define SIZE(t,n) _Static_assert(sizeof(t)==(n), #t " size")
SIZE(struct nx_modem_tty,16); SIZE(struct nx_modem_peer,536);
#define FIELD(t,f,n) _Static_assert(offsetof(t,f)==(n), #t "." #f " offset")
FIELD(struct nx_modem_peer,data,24); FIELD(struct nx_modem_peer,count,12);
_Static_assert(NX_MDM_GET==0x6d01 && NX_MDM_SET==0x6d02 && NX_MDM_PEER==0x6d03,"modem ioctl numbers");
_Static_assert(NX_PHYS_WINDOWS==8 && NX_PHYS_PAGES==2 && NX_PHYS_PAGE_SIZE==4096, "phys window units");
_Static_assert(NX_PHYS_BASE==0x600000000000UL && NX_PHYS_ADDRESS(7)==0x60000000e000UL, "phys window addresses");
SIZE(struct nx_kinfo_header,48); FIELD(struct nx_kinfo_header,total_bytes,24); FIELD(struct nx_kinfo_header,ticks,32);
SIZE(struct nx_kinfo_process,144); FIELD(struct nx_kinfo_process,size_bytes,40); FIELD(struct nx_kinfo_process,user_ticks,72); FIELD(struct nx_kinfo_process,command,128);
SIZE(struct nx_kinfo_file,40); FIELD(struct nx_kinfo_file,offset,24);
SIZE(struct nx_kinfo_inode,40); FIELD(struct nx_kinfo_inode,size,24);
SIZE(struct nx_kinfo_text,32); FIELD(struct nx_kinfo_text,size,16);
SIZE(struct nx_kinfo_mount,48); FIELD(struct nx_kinfo_mount,fs_blocks,16);
SIZE(struct nx_kinfo_memory,32); FIELD(struct nx_kinfo_memory,page_size,24);
SIZE(struct nx_kinfo_io,96); FIELD(struct nx_kinfo_io,cache_lookups,64);
SIZE(struct nlist,24); FIELD(struct nlist,n_name,0); FIELD(struct nlist,n_type,8); FIELD(struct nlist,n_value,16);
SIZE(Elf64_Ehdr,64); FIELD(Elf64_Ehdr,e_shoff,40); FIELD(Elf64_Ehdr,e_shnum,60);
SIZE(Elf64_Shdr,64); FIELD(Elf64_Shdr,sh_link,40); FIELD(Elf64_Shdr,sh_entsize,56);
SIZE(Elf64_Sym,24); FIELD(Elf64_Sym,st_shndx,6); FIELD(Elf64_Sym,st_value,8);
SIZE(struct nx_mon_header,64); SIZE(struct nx_mon_call,16);
FIELD(struct nx_mon_header,version,8); FIELD(struct nx_mon_header,machine,12);
FIELD(struct nx_mon_header,header_size,14); FIELD(struct nx_mon_header,low_pc,16);
FIELD(struct nx_mon_header,high_pc,24); FIELD(struct nx_mon_header,tick_hz,32);
FIELD(struct nx_mon_header,scale,36); FIELD(struct nx_mon_header,call_capacity,40);
FIELD(struct nx_mon_header,call_used,44); FIELD(struct nx_mon_header,histogram_bytes,48);
FIELD(struct nx_mon_header,call_record_size,56); FIELD(struct nx_mon_header,counter_size,60);
FIELD(struct nx_mon_header,reserved,62); FIELD(struct nx_mon_call,count,8);
SIZE(struct acct,40); SIZE(struct nx_acct,40); SIZE(comp_t,2);
FIELD(struct acct,ac_comm,0); FIELD(struct acct,ac_utime,10);
FIELD(struct acct,ac_stime,12); FIELD(struct acct,ac_etime,14);
FIELD(struct acct,ac_btime,16); FIELD(struct acct,ac_uid,24);
FIELD(struct acct,ac_gid,26); FIELD(struct acct,ac_mem,28);
FIELD(struct acct,ac_io,30); FIELD(struct acct,ac_tty,32);
FIELD(struct acct,ac_flag,34); FIELD(struct acct,ac_reserved,35);
SIZE(dev_t,2); SIZE(ino_t,2); SIZE(daddr_t,4); SIZE(off_t,8);
SIZE(time_t,8); SIZE(pid_t,4); SIZE(uid_t,2); SIZE(gid_t,2);
SIZE(caddr_t,8); SIZE(struct stat,48); SIZE(struct nx_stat,48);
FIELD(struct stat,st_dev,0); FIELD(struct stat,st_ino,2);
FIELD(struct stat,st_mode,4); FIELD(struct stat,st_nlink,6);
FIELD(struct stat,st_uid,8); FIELD(struct stat,st_gid,10);
FIELD(struct stat,st_rdev,12); FIELD(struct stat,reserved,14);
FIELD(struct stat,st_size,16); FIELD(struct stat,st_atime,24);
FIELD(struct stat,st_mtime,32); FIELD(struct stat,st_ctime,40);
FIELD(struct nx_stat,size,16); FIELD(struct nx_stat,atime,24);
FIELD(struct nx_stat,mtime,32); FIELD(struct nx_stat,ctime,40);
SIZE(struct direct,16); FIELD(struct direct,d_name,2);
SIZE(struct tms,32); FIELD(struct tms,tms_utime,0);
FIELD(struct tms,tms_stime,8); FIELD(struct tms,tms_cutime,16);
FIELD(struct tms,tms_cstime,24);
SIZE(struct timeb,16); FIELD(struct timeb,millitm,8);
FIELD(struct timeb,timezone,10); FIELD(struct timeb,dstflag,12);
SIZE(struct sgttyb,8); FIELD(struct sgttyb,sg_flags,4);
SIZE(struct tchars,6); SIZE(struct nx_sgttyb,8); SIZE(struct nx_tchars,6);
SIZE(struct utmp,24); FIELD(struct utmp,ut_time,16);
SIZE(struct passwd,64); FIELD(struct passwd,pw_uid,16);
FIELD(struct passwd,pw_comment,32); FIELD(struct passwd,pw_shell,56);
SIZE(struct group,32); FIELD(struct group,gr_gid,16); FIELD(struct group,gr_mem,24);
SIZE(jmp_buf,72);
_Static_assert(NX_PT_WRITE_I==4 && NX_PT_WRITE_D==5 && NX_PT_STEP==9,"V7 debugger requests");
_Static_assert(NX_SYS_PROFIL==44 && sizeof(unsigned short)==2,"profil ABI");
SIZE(struct nx_ptrace_user,688); FIELD(struct nx_ptrace_user,r12,24);
FIELD(struct nx_ptrace_user,rax,112); FIELD(struct nx_ptrace_user,rip,136);
FIELD(struct nx_ptrace_user,rflags,152); FIELD(struct nx_ptrace_user,rsp,160);
FIELD(struct nx_ptrace_user,fpu,176);
SIZE(struct nx_signal_context,688); FIELD(struct nx_signal_context,rip,136);
FIELD(struct nx_signal_context,rsp,160); FIELD(struct nx_signal_context,fpu,176);
SIZE(struct nx_fpu_state,512);
_Static_assert(_Alignof(struct nx_fpu_state)==16,"FXSAVE alignment");
_Static_assert(NX_ABI_VERSION==2,"native ABI version");
_Static_assert(BSIZE==512 && NINDIR==128 && DIRSIZ==14,"disk constants");
_Static_assert(HZ==100 && CLK_TCK==100,"native tick rate");
_Static_assert(S_IFMPC==0030000 && S_IFMPB==0070000,"V7 mpx modes");
_Static_assert(SIGINS==4 && SIGTRC==5 && SIGCLK==14,"V7 signal aliases");
SIZE(struct piocb,12);FIELD(struct piocb,t,0);FIELD(struct piocb,psize,4);
FIELD(struct piocb,mode,6);FIELD(struct piocb,state,8);FIELD(struct piocb,window,10);
_Static_assert(DIOCGETP==0x6408 && NX_PK_WINDOW==2 && NX_PK_MAX_SIZE==512,"packet contract");
_Static_assert(NX_MEM_DEV==0x600 && NX_KMEM_DEV==0x601 && NX_MEMORY_NULL_DEV==0x602 && NX_NULL_DEV==0x300,"memory devices");
_Static_assert(NX_DISK_DEV(NX_DISK_HP,0,0)==0x700 && NX_DISK_DEV(NX_DISK_RP,2,1)==0x1002,"native controller devices");
int main(void)
{
    for (unsigned a=0; a<256; a++)
        for (unsigned b=0; b<256; b++) {
            dev_t d=makedev(a,b);
            if (major(d)!=(int)a || minor(d)!=(int)b || d!=v7_makedev(a,b)) return 1;
        }
    if (NODEV!=65535 || itod(1)!=2 || itoo(1)!=0 || itod(65535)!=8193 || itoo(65535)!=6) return 2;
    return 0;
}

_Static_assert(sizeof(struct nx_tape_operation)==8 && offsetof(struct nx_tape_status,position)==24,"tape native ABI");

_Static_assert(NX_TC_DEV(7)==0x1607 && NX_TC_BYTES==295936,"DECtape native ABI");

_Static_assert(NX_HP_INTERLEAVED_MINOR(2,1)==0111,"HP interleaved minor ABI");

/* Task 424: socket ABI values (4.4BSD numbering) and fixed-width records. */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <nexix/socket.h>
_Static_assert(NX_SYS_SOCKET==77 && NX_SYS_SENDTO==83 && NX_SYS_GETSOCKOPT==88 && NX_SYS_LAST==88, "socket syscall numbers");
_Static_assert(ENOSYS==35 && ENOTSOCK==36 && EAFNOSUPPORT==43 && ECONNRESET==49 && ECONNREFUSED==54 &&
               EINPROGRESS==57 && EWOULDBLOCK==EAGAIN, "networking errno numbers");
_Static_assert(AF_INET==2 && SOCK_STREAM==1 && SOCK_DGRAM==2 && SOCK_RAW==3 && IPPROTO_TCP==6 &&
               IPPROTO_UDP==17 && IPPROTO_ICMP==1 && SOL_SOCKET==0xffff, "socket family/type/protocol values");
_Static_assert(SO_REUSEADDR==4 && SO_SNDBUF==0x1001 && SO_RCVBUF==0x1002 && SO_SNDTIMEO==0x1005 &&
               SO_RCVTIMEO==0x1006 && SO_ERROR==0x1007 && SO_TYPE==0x1008 && TCP_NODELAY==1, "socket option values");
_Static_assert(MSG_PEEK==2 && MSG_WAITALL==0x40 && MSG_DONTWAIT==0x80 && SHUT_RD==0 && SHUT_WR==1 && SHUT_RDWR==2,
               "socket flag values");
#include <fcntl.h>
_Static_assert(O_NONBLOCK==4 && S_IFSOCK==0140000, "socket descriptor flag and file type");
_Static_assert(NX_SIOCGIFINFO==0x6901 && NX_SIOCLOOKUP==0x6905 && NX_IFNAMSIZ==8 && NX_IFF_RUNNING==0x40,
               "configuration ioctl numbers");
_Static_assert(offsetof(struct nx_ifinfo,flags)==12 && offsetof(struct nx_ifinfo,lladdr)==20 &&
               offsetof(struct nx_ifinfo,addr)==28 && offsetof(struct nx_ifinfo,ipackets)==40 &&
               offsetof(struct nx_ifconfig,change)==8 && offsetof(struct nx_route_lookup,ifname)==4 &&
               offsetof(struct nx_route_lookup,next_hop)==12, "configuration record offsets");
_Static_assert(INADDR_LOOPBACK==0x7f000001u && INADDR_ANY==0 && INADDR_NONE==0xffffffffu && INET_ADDRSTRLEN==16,
               "IPv4 address constants");
SIZE(socklen_t,4); SIZE(sa_family_t,2); SIZE(in_port_t,2); SIZE(in_addr_t,4); SIZE(struct in_addr,4);
SIZE(struct sockaddr,16); SIZE(struct sockaddr_in,16); SIZE(struct sockaddr_storage,128); SIZE(struct timeval,16);
_Static_assert(offsetof(struct sockaddr_in,sin_family)==0 && offsetof(struct sockaddr_in,sin_port)==2 &&
               offsetof(struct sockaddr_in,sin_addr)==4 && offsetof(struct sockaddr_in,sin_zero)==8, "sockaddr_in offsets");
_Static_assert(offsetof(struct sockaddr,sa_data)==2 && offsetof(struct timeval,tv_usec)==8 &&
               offsetof(struct nx_timeval,usec)==8, "sockaddr/timeval offsets");
_Static_assert(_Alignof(struct sockaddr_storage)>=_Alignof(struct sockaddr_in), "storage holds any address");
