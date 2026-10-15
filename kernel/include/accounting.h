#ifndef NEXIX_ACCOUNTING_H
#define NEXIX_ACCOUNTING_H
#include "v7/core.h"
#include <nexix/acct.h>
uint16_t accounting_compress(uint64_t);
void accounting_record(const struct user *,int64_t now,struct nx_acct *);
bool proc_superuser(struct user *); /* V7 suser: sets ASU only on success. */
int accounting_control(uintptr_t path);
void accounting_exit(const struct user *);
#endif
