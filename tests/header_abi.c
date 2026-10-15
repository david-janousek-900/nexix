/* Independently specified LP64 record contracts, shared kernel/libc boundaries. */
#include <sys/types.h>
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
#include <nexix/tty.h>
#include "v7/types.h"
#define SIZE(t,n) _Static_assert(sizeof(t)==(n), #t " size")
#define FIELD(t,f,n) _Static_assert(offsetof(t,f)==(n), #t "." #f " offset")
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
SIZE(struct nx_signal_context,688); FIELD(struct nx_signal_context,rip,136);
FIELD(struct nx_signal_context,rsp,160); FIELD(struct nx_signal_context,fpu,176);
SIZE(struct nx_fpu_state,512);
_Static_assert(_Alignof(struct nx_fpu_state)==16,"FXSAVE alignment");
_Static_assert(NX_ABI_VERSION==2,"native ABI version");
_Static_assert(BSIZE==512 && NINDIR==128 && DIRSIZ==14,"disk constants");
_Static_assert(HZ==100 && CLK_TCK==100,"native tick rate");
_Static_assert(S_IFMPC==0030000 && S_IFMPB==0070000,"V7 mpx modes");
_Static_assert(SIGINS==4 && SIGTRC==5 && SIGCLK==14,"V7 signal aliases");
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
