#ifndef NEXIX_MODEM_H
#define NEXIX_MODEM_H
#include "process.h"
#include <nexix/modem.h>
bool modem_matches(uint16_t);
int modem_open(uint16_t,struct user *,void **);
void modem_close(void *);
int64_t modem_io(void *,struct address_space *,uintptr_t,size_t,bool);
int modem_ioctl(void *,struct address_space *,uint64_t,uintptr_t);
void modem_timer(uint64_t);
void tty_modem_status(unsigned);
int tty_modem_peer(unsigned);
#endif
