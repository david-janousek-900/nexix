#include "signals.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
static struct proc processes[V7_NPROC];
static struct user users[V7_NPROC];
struct proc *proc_current(void){return &processes[1];}
const struct proc *proc_table(void){return processes;}
int main(void)
{
    for(unsigned i=1;i<4;++i){
        processes[i]=(struct proc){.p_stat=V7_SRUN,.p_pid=i,.p_pgrp=i,.p_uid=i==2?200:100,.p_user=&users[i]};
        users[i].u_uid=processes[i].p_uid;
    }
    assert(signal_group_user(2,NX_SIGTERM)==-NX_EPERM && !processes[2].p_sig);
    assert(!signal_group_user(3,NX_SIGTERM) && processes[3].p_sig);
    processes[3].p_sig=0;
    /* Group 1 must not accidentally become kill(-1)'s broadcast selector. */
    assert(!signal_group_user(1,NX_SIGTERM) && processes[1].p_sig && !processes[3].p_sig);
    users[1].u_uid=0;
    assert(!signal_group_user(2,NX_SIGTERM) && processes[2].p_sig);
    processes[2].p_sig=0;users[1].u_uid=100;processes[3].p_pgrp=2;
    assert(!signal_group_user(2,NX_SIGINT) && processes[3].p_sig && !processes[2].p_sig);
    assert(signal_group_user(99,NX_SIGTERM)==-NX_ESRCH);
    assert(signal_group_user(0,NX_SIGTERM)==-NX_EINVAL);
    assert(signal_group_user(2,V7_NSIG)==-NX_EINVAL);
    assert(!signal_kill(-1,0));
    /* V7 traces SIGKILL: a trace stop waits for the tracer even for SIGKILL. */
    processes[3]=(struct proc){.p_stat=V7_SSTOP,.p_flag=V7_STRC,.p_pid=3,.p_pgrp=3,.p_user=&users[3]};
    signal_post(&processes[3],NX_SIGTERM);assert(processes[3].p_stat==V7_SSTOP);
    signal_post(&processes[3],NX_SIGKILL);assert(processes[3].p_stat==V7_SSTOP && (processes[3].p_sig&(1U<<(NX_SIGKILL-1))));
    puts("PASS: user group signals enforce UID permissions, root access, mixed groups, group 1 isolation and traced SIGKILL stops");
}
