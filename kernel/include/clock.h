#ifndef NEXIX_CLOCK_H
#define NEXIX_CLOCK_H
#include <stdbool.h>
#include <stdint.h>
#include <nexix/timeb.h>
/* Boot RTC is UTC. No implicit local-time conversion or RTC writes. */
bool rtc_read_epoch(int64_t *epoch);
bool clock_init(void);
int64_t clock_now(void);
/* Caller serializes against clock_set with interrupts disabled. */
struct nx_timeb clock_snapshot(void);
bool clock_set(int64_t epoch);
#endif
