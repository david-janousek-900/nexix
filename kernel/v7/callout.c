/* V7 timeout queue adapted to monotonic 64-bit absolute tick deadlines. */
#include "callout.h"
#include "process.h"
#include <nexix/errno.h>
struct callout {uint64_t deadline;void (*function)(void *);void *argument;};
static struct callout pending[NX_CALLOUT_CAPACITY];
static unsigned count;
static bool dispatching;
int callout_schedule(void (*function)(void *),void *argument,uint64_t delay)
{
    if(!function || delay>INT64_MAX)return -NX_EINVAL;
    uintptr_t flags=proc_irq_save();
    if(count==NX_CALLOUT_CAPACITY){proc_irq_restore(flags);return -NX_ENOSPC;}
    uint64_t deadline=timer_ticks()+(delay?delay:1);
    unsigned at=count;
    while(at && (int64_t)(pending[at-1].deadline-deadline)>0){pending[at]=pending[at-1];--at;}
    pending[at]=(struct callout){deadline,function,argument};++count;
    proc_irq_restore(flags);return 0;
}
unsigned callout_cancel(void (*function)(void *),void *argument)
{
    uintptr_t flags=proc_irq_save();
    unsigned kept=0,removed=0;
    for(unsigned i=0;i<count;++i){
        if(pending[i].function==function && pending[i].argument==argument)++removed;
        else pending[kept++]=pending[i];
    }
    count=kept;proc_irq_restore(flags);return removed;
}
void callout_tick(uint64_t now)
{
    uintptr_t flags=proc_irq_save();
    if(dispatching){proc_irq_restore(flags);return;}
    dispatching=true;
    /* Bound IRQ work even if a callback schedules another callback. */
    for(unsigned budget=0;budget<NX_CALLOUT_CAPACITY && count && (int64_t)(now-pending[0].deadline)>=0;++budget){
        struct callout ready=pending[0];
        --count;for(unsigned i=0;i<count;++i)pending[i]=pending[i+1];
        ready.function(ready.argument);
    }
    dispatching=false;proc_irq_restore(flags);
}
