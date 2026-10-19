/* Resident-memory adaptation of V7 clock.c and slp.c setpri/swtch. */
#include "scheduler.h"
void scheduler_priority(struct proc *p)
{
    int priority=p->p_cpu/16+V7_PUSER+(int)p->p_nice-V7_NZERO;
    p->p_pri=priority>127?127:priority<0?0:priority;
}
void scheduler_tick(struct proc *table,size_t count,struct proc *running,bool second)
{
    if(running && running->p_cpu<255)++running->p_cpu;
    if(!second)return;
    for(size_t i=1;i<count;++i){
        struct proc *p=&table[i];
        if(!p->p_stat || p->p_stat>=V7_SZOMB)continue;
        if(p->p_time<127)++p->p_time;
        int usage=p->p_cpu*8/10+(int)p->p_nice-V7_NZERO;
        p->p_cpu=usage<0?0:usage>255?255:usage;
        /* Sleeping kernel priorities survive decay until the syscall returns. */
        if(p->p_pri>=V7_PUSER)scheduler_priority(p);
    }
}
struct proc *scheduler_pick(struct proc *table,size_t count,unsigned cursor)
{
    struct proc *selected=NULL;
    for(size_t n=0;n<count-1;++n){
        struct proc *p=&table[1+(cursor+n)%(count-1)];
        if(p->p_stat==V7_SRUN && (!selected || p->p_pri<selected->p_pri))selected=p;
    }
    return selected;
}
