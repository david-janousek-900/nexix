/* Native extension: exponentially damped 1/5/15-minute scheduler load. */
#include "loadavg.h"
static struct nx_kinfo_load values={.scale=65536};
void loadavg_sample(const struct proc *table,size_t count)
{
    unsigned active=0,total=0;
    static const unsigned decay[3]={64453,65318,65463}; /* round(65536*exp(-1/tau)) */
    for(size_t i=1;i<count;i++){
        const struct proc *p=&table[i];
        if(!p->p_stat || p->p_stat==V7_SIDL || p->p_stat==V7_SZOMB)continue;
        ++total;
        if(p->p_flag&V7_SSYS)continue;
        if(p->p_stat==V7_SRUN || (p->p_stat==V7_SSLEEP && !p->p_interruptible && !p->p_timed_sleep))++active;
    }
    values.runnable=active;values.processes=total;
    for(unsigned i=0;i<3;i++)values.average[i]=((uint64_t)values.average[i]*decay[i]+(uint64_t)active*65536*(65536-decay[i])+32768)/65536;
}
void loadavg_snapshot(struct nx_kinfo_load *out){*out=values;}
