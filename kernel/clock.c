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
int64_t clock_now(void)
{
    return offset + timer_ticks() / NX_CLK_TCK;
}
bool clock_init(void)
{
    int64_t epoch;
    return rtc_read_epoch(&epoch) && clock_set(epoch);
}
