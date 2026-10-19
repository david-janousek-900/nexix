#include "scheduler.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct proc table[4]={0};
    for(int i=1;i<4;i++){table[i].p_stat=V7_SRUN;table[i].p_nice=20;scheduler_priority(&table[i]);}
    assert(scheduler_pick(table,4,0)==&table[1]);
    assert(scheduler_pick(table,4,1)==&table[2]);
    assert(scheduler_pick(table,4,2)==&table[3]);
    /* A CPU-bound process accumulates usage; an idle peer gets the next turn. */
    for(int i=0;i<100;i++)scheduler_tick(table,4,&table[1],i==99);
    assert(table[1].p_cpu==80 && table[1].p_pri==55);
    assert(scheduler_pick(table,4,0)==&table[2]);
    for(int i=0;i<1000;i++)scheduler_tick(table,4,&table[1],false);
    assert(table[1].p_cpu==255);
    table[2].p_stat=V7_SSLEEP;table[2].p_pri=28;table[2].p_cpu=100;
    scheduler_tick(table,4,NULL,true);
    assert(table[2].p_cpu==80 && table[2].p_pri==28);
    scheduler_priority(&table[2]);assert(table[2].p_pri==55);
    table[3].p_nice=0;table[3].p_cpu=0;scheduler_priority(&table[3]);
    assert(table[3].p_pri==30 && scheduler_pick(table,4,1)==&table[3]);
    table[3].p_nice=39;table[3].p_cpu=255;scheduler_priority(&table[3]);
    assert(table[3].p_pri==84);
    for(int i=0;i<300;i++)scheduler_tick(table,4,NULL,true);
    assert(table[1].p_time==127 && table[2].p_time==127 && table[3].p_cpu<=95);
    table[1].p_stat=V7_SZOMB;table[1].p_cpu=123;
    scheduler_tick(table,4,NULL,true);assert(table[1].p_cpu==123);
    puts("PASS: V7 CPU saturation/decay, nice priorities, wakeup priorities and rotating selection");
}
