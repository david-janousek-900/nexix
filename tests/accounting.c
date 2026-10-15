#include "accounting.h"
#include <nexix/errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct proc process;
static struct user user;
static struct inode inode;
static struct nx_acct written;
static int opened,released,appended,open_error,release_error,write_error;
struct proc *proc_current(void){return &process;}
int64_t clock_now(void){return 1709210096;}
int files_account_open(uintptr_t path,struct inode **out)
{assert(path);++opened;if(open_error)return open_error;*out=&inode;++inode.i_count;return 0;}
int files_inode_put(struct inode *ip){assert(ip==&inode&&ip->i_count);--ip->i_count;++released;return release_error;}
int files_account_append(struct inode *ip,const void *data,size_t size)
{assert(ip==&inode&&ip->i_count==1&&size==40);memcpy(&written,data,size);++appended;return write_error;}
int main(void)
{
    process.p_user=&user;
    assert(accounting_compress(0)==0&&accounting_compress(8191)==8191);
    assert(accounting_compress(8192)==0x2400&&accounting_compress(8195)==0x2400&&accounting_compress(8196)==0x2401);
    assert(accounting_compress(65535)==0x4400&&accounting_compress(8191ULL<<21)==0xffff);
    assert(accounting_compress(UINT64_MAX)==0xffff);
    user=(struct user){.u_start=1709210079,.u_utime=8196,.u_stime=65535,.u_ruid=123,.u_rgid=456,.u_uid=789,.u_gid=321,.u_ttyd=0x1234,.u_acflag=NX_AFORK|NX_ASU};
    memcpy(user.u_comm,"abcdefghijklmn",14);
    struct nx_acct record;accounting_record(&user,1709210096,&record);
    static const unsigned char expected[40]={
        'a','b','c','d','e','f','g','h','i','j',1,0x24,0,0x44,17,0,
        0xdf,0x79,0xe0,0x65,0,0,0,0,123,0,0xc8,1,0,0,0,0,0x34,0x12,3,0,0,0,0,0};
    assert(!memcmp(&record,expected,40));
    accounting_record(&user,0,&record);assert(!record.etime);
    user.u_utime=-1;user.u_stime=-1;accounting_record(&user,INT64_MAX,&record);
    assert(!record.utime&&!record.stime&&record.etime==0xffff);
    assert(!proc_superuser(&user)&&user.u_acflag==3);
    assert(accounting_control(1)==-NX_EPERM&&!opened);
    user.u_uid=0;user.u_acflag=0;assert(proc_superuser(&user)&&user.u_acflag==NX_ASU);
    assert(accounting_control(0)==0);
    open_error=-NX_ENOENT;assert(accounting_control(1)==-NX_ENOENT&&!inode.i_count);open_error=0;
    assert(accounting_control(1)==0&&inode.i_count==1);
    assert(accounting_control(2)==-NX_EBUSY&&opened==2);
    accounting_exit(&user);assert(appended==1&&written.uid==123&&written.gid==456);
    write_error=-NX_ENOSPC;accounting_exit(&user);assert(appended==2&&inode.i_count==1);
    release_error=-NX_EIO;assert(accounting_control(0)==-NX_EIO&&!inode.i_count);release_error=0;
    accounting_exit(&user);assert(appended==2&&accounting_control(0)==0&&released==1);
    assert(accounting_control(1)==0&&accounting_control(0)==0&&released==2);
    puts("PASS: accounting golden record, compression/overflow, real IDs, flags, clock rollback, lifecycle and control/write failures");
    return 0;
}
