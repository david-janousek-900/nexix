#ifndef NEXIX_CALLOUT_H
#define NEXIX_CALLOUT_H
#include <stdint.h>
#define NX_CALLOUT_CAPACITY 20
/* IRQ-serialized callbacks must not sleep. Zero delay runs on the next tick. */
int callout_schedule(void (*function)(void *),void *argument,uint64_t delay);
unsigned callout_cancel(void (*function)(void *),void *argument);
void callout_tick(uint64_t now);
#endif
