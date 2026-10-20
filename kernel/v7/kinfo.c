/* Native counterpart of V7 diagnostic reads of kernel tables via /dev/{k,}mem. */
#include "kinfo.h"
#include "files.h"
#include "text.h"
#include "namespace.h"
#include "memory.h"
#include "accounting.h"
#include "uaccess.h"
#include <nexix/syscall.h>
#include <nexix/errno.h>
union record {
    struct nx_kinfo_process process;
    struct nx_kinfo_file file;
    struct nx_kinfo_inode inode;
    struct nx_kinfo_text text;
    struct nx_kinfo_mount mount;
    struct nx_kinfo_memory memory;
    struct nx_kinfo_io io;
};
static bool process_info(unsigned i,struct nx_kinfo_process *r,uint64_t now)
{
    if(i>=V7_NPROC)return false;
    const struct proc *p=&proc_table()[i];
    if(p->p_stat==V7_UNUSED || p->p_stat==V7_SIDL)return false;
    const struct user *u=p->p_user;
    *r=(struct nx_kinfo_process){.slot=i+1,.pid=p->p_pid,.ppid=p->p_ppid,
        .pgrp=p->p_pgrp,.state=p->p_stat,.flags=p->p_flag,.priority=p->p_pri,
        .nice=p->p_nice,.cpu=p->p_cpu,.age=p->p_time,.pending=p->p_sig,
        .status=p->p_stat==V7_SZOMB?p->p_xstat:0,.waiting=p->p_wchan!=NULL,
        .size_bytes=p->p_size,.text_id=text_info_id(p->p_textp),
        .tty_dev=V7_NODEV,.alarm_ticks=p->p_clktim>(int64_t)now?(uint64_t)p->p_clktim-now:0};
    if(u){
        r->ruid=u->u_ruid;r->euid=u->u_uid;r->rgid=u->u_rgid;r->egid=u->u_gid;
        r->tty_dev=u->u_ttyd;r->text_bytes=u->u_tsize;r->data_bytes=u->u_dsize;
        r->stack_bytes=u->u_ssize;r->user_ticks=u->u_utime;r->system_ticks=u->u_stime;
        r->child_user_ticks=u->u_cutime;r->child_system_ticks=u->u_cstime;
        r->start_time=u->u_start;
        for(unsigned j=0;j<V7_DIRSIZ;++j)r->command[j]=u->u_comm[j];
    }
    return true;
}
static bool get(unsigned kind,unsigned i,union record *r,uint64_t now)
{
    switch(kind){
    case NX_KINFO_PROCESS:return process_info(i,&r->process,now);
    case NX_KINFO_FILE:return files_info(i,&r->file);
    case NX_KINFO_INODE:return inodes_info(i,&r->inode);
    case NX_KINFO_TEXT:return text_info(i,&r->text);
    case NX_KINFO_MOUNT:return mounts_info(i,&r->mount);
    case NX_KINFO_MEMORY:
        r->memory=(struct nx_kinfo_memory){.total_pages=page_total_count(),
            .free_pages=page_free_count(),.page_size=PAGE_SIZE,.flags=NX_KINFO_RESIDENT};
        r->memory.allocated_pages=r->memory.total_pages-r->memory.free_pages;return i==0;
    case NX_KINFO_IO:
        r->io=(struct nx_kinfo_io){0};block_statistics(&r->io);cache_statistics(&r->io);
        tty_statistics(&r->io);cpu_statistics(&r->io);return i==0;
    default:return false;
    }
}
int64_t kinfo_request(uint64_t version,uint64_t kind,uintptr_t buffer,size_t bytes)
{
    if(version!=NX_KINFO_VERSION || kind<NX_KINFO_PROCESS || kind>NX_KINFO_IO)return -NX_EINVAL;
    uintptr_t irq=proc_irq_save();
    struct proc *caller=proc_current();
    if(!caller || !caller->p_user || (kind!=NX_KINFO_PROCESS && !proc_superuser(caller->p_user))){
        proc_irq_restore(irq);return -NX_EPERM;
    }
    static const unsigned capacities[]={0,V7_NPROC,V7_NFILE,V7_NINODE,V7_NTEXT,NAMESPACE_MOUNTS,1,1};
    static const unsigned sizes[]={0,sizeof(struct nx_kinfo_process),sizeof(struct nx_kinfo_file),
        sizeof(struct nx_kinfo_inode),sizeof(struct nx_kinfo_text),sizeof(struct nx_kinfo_mount),
        sizeof(struct nx_kinfo_memory),sizeof(struct nx_kinfo_io)};
    uint64_t now=timer_ticks();unsigned count=0;union record r;
    for(unsigned i=0;i<capacities[kind];++i)if(get(kind,i,&r,now))++count;
    size_t total=sizeof(struct nx_kinfo_header)+(size_t)count*sizes[kind];
    int64_t result=total;
    if(!buffer && !bytes)goto done;
    if(!buffer){result=-NX_EFAULT;goto done;}
    if(bytes<total){result=-NX_ERANGE;goto done;}
    if(!user_access_ok(&caller->p_vm,buffer,total,true)){result=-NX_EFAULT;goto done;}
    struct nx_kinfo_header h={.version=NX_KINFO_VERSION,.kind=kind,.record_size=sizes[kind],
        .count=count,.header_size=sizeof(h),.flags=NX_KINFO_RESIDENT,.total_bytes=total,
        .ticks=now,.tick_hz=NX_CLK_TCK};
    /* No allocation, I/O or scheduling between count, validation and publication. */
    uintptr_t out=buffer+sizeof(h);
    for(unsigned i=0;i<capacities[kind];++i)if(get(kind,i,&r,now)){
        if(copyout(&caller->p_vm,out,&r,sizes[kind]))panic("snapshot mapping changed");
        out+=sizes[kind];
    }
    if(copyout(&caller->p_vm,buffer,&h,sizeof(h)))panic("snapshot header mapping changed");
done:
    proc_irq_restore(irq);return result;
}
