#ifndef NEXIX_TICK_CLOCK_H
#define NEXIX_TICK_CLOCK_H
#include <stdint.h>
/* Fractional cycles survive polling; unsigned subtraction handles TSC wrap. */
struct tick_clock { uint64_t last, period, remainder; };
static inline uint64_t tick_clock_advance(struct tick_clock *clock,uint64_t now)
{
    uint64_t elapsed=now-clock->last;clock->last=now;
    uint64_t ticks=elapsed/clock->period,part=elapsed%clock->period;
    if(part>=clock->period-clock->remainder){++ticks;clock->remainder-=clock->period-part;}
    else clock->remainder+=part;
    return ticks;
}
#endif
