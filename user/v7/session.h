#ifndef NEXIX_V7_SESSION_H
#define NEXIX_V7_SESSION_H
/* Native record-I/O adapters for V7 login.c and init.c:rmut. */
int v7_accounting_reset(void);
int v7_login_record(const char *name,const char *terminal,int slot);
int v7_logout_record(const char *line);
#endif
