#include "clock.h"
#include "interrupts.h"
#include <nexix/syscall.h>
static int64_t offset;
bool clock_set(int64_t epoch)
{
    int64_t elapsed = timer_ticks() / NX_CLK_TCK;
    if (epoch < 0 || epoch > INT64_MAX - elapsed) return false;
    offset = epoch - elapsed;
    return true;
}
/* Saturate wall time when an accepted setting reaches the signed limit. */
static int64_t at_ticks(uint64_t ticks)
{
    int64_t elapsed = ticks / NX_CLK_TCK;
    if (offset > 0 && elapsed > INT64_MAX - offset) return INT64_MAX;
    return offset + elapsed;
}
int64_t clock_now(void)
{
    return at_ticks(timer_ticks());
}
bool clock_init(void)
{
    int64_t epoch;
    return rtc_read_epoch(&epoch) && clock_set(epoch);
}

struct nx_timeb clock_snapshot(void)
{
    uint64_t ticks=timer_ticks();
    return (struct nx_timeb){.time=at_ticks(ticks),
        .millitm=(ticks%NX_CLK_TCK)*(1000/NX_CLK_TCK)};
}
