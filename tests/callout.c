#include "callout.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
static uint64_t ticks;
static unsigned depth,used,order[64];
uintptr_t proc_irq_save(void){unsigned old=depth;depth++;return old;}
void proc_irq_restore(uintptr_t f){assert(depth==f+1);depth=f;}
uint64_t timer_ticks(void){assert(depth);return ticks;}
static void record(void *p){assert(depth);order[used++]=(uintptr_t)p;}
static void again(void *p){record(p);assert(callout_schedule(record,(void *)8,0)==0);}
static void cancel(void *p){record(p);assert(callout_cancel(record,(void *)9)==1);}
int main(void)
{
    assert(callout_schedule(NULL,0,1)==-NX_EINVAL);
    assert(callout_schedule(record,0,UINT64_MAX)==-NX_EINVAL);
    assert(callout_schedule(record,(void *)3,3)==0);
    assert(callout_schedule(record,(void *)1,1)==0);
    assert(callout_schedule(record,(void *)2,1)==0);
    callout_tick(ticks);assert(!used);
    callout_tick(++ticks);assert(used==2 && order[0]==1 && order[1]==2);
    callout_tick(++ticks);assert(used==2);
    callout_tick(++ticks);assert(used==3 && order[2]==3);
    for(unsigned i=0;i<NX_CALLOUT_CAPACITY;i++)assert(!callout_schedule(record,(void *)4,10));
    assert(callout_schedule(record,0,1)==-NX_ENOSPC);
    assert(callout_cancel(record,(void *)4)==NX_CALLOUT_CAPACITY);
    assert(!callout_cancel(record,(void *)4));
    assert(!callout_schedule(again,(void *)7,0));callout_tick(++ticks);
    assert(order[used-1]==7);callout_tick(++ticks);assert(order[used-1]==8);
    assert(!callout_schedule(cancel,(void *)6,1));assert(!callout_schedule(record,(void *)9,1));
    callout_tick(++ticks);assert(order[used-1]==6);
    ticks=UINT64_MAX-1;
    assert(!callout_schedule(record,(void *)10,1));assert(!callout_schedule(record,(void *)11,3));
    callout_tick(++ticks);assert(order[used-1]==10);
    callout_tick(++ticks);assert(order[used-1]==10);
    callout_tick(++ticks);assert(order[used-1]==11 && !depth);
    puts("PASS: callout deadlines, stable ordering, cancellation, capacity, callback rearming and tick wrap");
}
