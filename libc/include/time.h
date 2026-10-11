#ifndef NEXIX_LIBC_TIME_H
#define NEXIX_LIBC_TIME_H
#include <sys/types.h>
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
};
char *ctime(const time_t *);
char *asctime(const struct tm *);
struct tm *gmtime(const time_t *);
struct tm *localtime(const time_t *);
void tzset(void);
char *timezone(int minutes_west, int dst);
int dysize(int year_since_1900);
time_t time(time_t *);
int stime(const time_t *);
#endif
