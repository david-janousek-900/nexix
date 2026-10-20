#include "loadavg.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct proc table[5]={0};struct nx_kinfo_load r;
    loadavg_snapshot(&r);assert(r.scale==65536 && !r.average[0] && !r.reserved);
    table[1].p_stat=V7_SRUN;
    table[2].p_stat=V7_SSLEEP;
    table[3].p_stat=V7_SRUN;table[3].p_flag=V7_SSYS;
    table[4].p_stat=V7_SSLEEP;table[4].p_interruptible=true;
    for(unsigned i=0;i<60;i++)loadavg_sample(table,5);
    loadavg_snapshot(&r);assert(r.runnable==2 && r.processes==4);
    assert(r.average[0]>81920 && r.average[0]<83886);
    assert(r.average[0]>r.average[1] && r.average[1]>r.average[2]);
    uint32_t before=r.average[0];table[1].p_stat=V7_SSTOP;table[2].p_timed_sleep=true;
    for(unsigned i=0;i<60;i++)loadavg_sample(table,5);
    loadavg_snapshot(&r);assert(!r.runnable && r.average[0]<before/2);
    for(unsigned i=0;i<5;i++){table[i].p_stat=V7_SRUN;table[i].p_flag=0;}
    for(unsigned i=0;i<20000;i++)loadavg_sample(table,5);
    loadavg_snapshot(&r);for(unsigned i=0;i<3;i++)assert(r.average[i]>4*65536-1000 && r.average[i]<=4*65536);
    puts("PASS: 1/5/15 minute exponential load, runnable/I/O waits, idle/system exclusions and convergence");
}
