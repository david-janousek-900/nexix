#include "tick_clock.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct tick_clock clock={.last=100,.period=1000};
    assert(!tick_clock_advance(&clock,600));
    assert(tick_clock_advance(&clock,4100)==4 && !clock.remainder);
    assert(!tick_clock_advance(&clock,4100));
    for(unsigned i=1;i<=100;++i)
        assert(tick_clock_advance(&clock,4100+i*10)==(i==100));
    clock.last=UINT64_MAX-499;clock.remainder=0;
    assert(tick_clock_advance(&clock,500)==1);
    clock.last=0;clock.period=UINT64_MAX;clock.remainder=UINT64_MAX-2;
    assert(tick_clock_advance(&clock,3)==1 && clock.remainder==1);
    puts("PASS: elapsed timer periods, delayed polling, fractional cycles and wrap");
}
