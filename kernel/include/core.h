#ifndef NEXIX_CORE_H
#define NEXIX_CORE_H
#include "process.h"
#include <nexix/core.h>
#include <nexix/signal.h>
bool core_signal(unsigned);
bool core_dump(unsigned,const struct interrupt_frame *);
#endif
