/* Native adaptation of V7 libc/gen/{ctime,timezone}.c. Retains the
 * historical US DST rules when TZ omits explicit transitions, including
 * 1974/1975 exceptions. TZ adds fixed offsets and Mmonth.week.weekday rules.
 * Reference sources remain unchanged under reference/v7. */
#include <time.h>
#include <sys/timeb.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
static const unsigned lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
static struct tm result;
static char text[26];
struct rule { int month, week, weekday, seconds; };
static struct {
    char standard[16], daylight[16];
    int west, dst, explicit_rules;
    struct rule start, end;
} zone;
int dysize(int y)
{
    int64_t year = (int64_t)y + 1900;
    return 365 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
static int number(const char **p)
{
    int n = 0, count = 0;
    while (**p >= '0' && **p <= '9') {
        if (++count > 3) return -1;
        n = n * 10 + *(*p)++ - '0';
    }
    return count ? n : -1;
}
static int name(const char **p, char *out)
{
    int n = 0;
    while ((**p >= 'A' && **p <= 'Z') || (**p >= 'a' && **p <= 'z')) {
        if (n == 15) return 0;
        out[n++] = *(*p)++;
    }
    out[n] = 0;
    return n >= 3;
}
static int offset(const char **p, int *out)
{
    int sign = 1;
    if (**p == '-' || **p == '+') { if (*(*p)++ == '-') sign = -1; }
    int h = number(p), m = 0;
    if (**p == ':') { ++*p; m = number(p); }
    if (h < 0 || h > 24 || m < 0 || m > 59 || (h == 24 && m)) return 0;
    *out = sign * (h * 60 + m);
    return 1;
}
static int rule(const char **p, struct rule *r)
{
    if (*(*p)++ != 'M') return 0;
    r->month = number(p);
    if (*(*p)++ != '.') return 0;
    r->week = number(p);
    if (*(*p)++ != '.') return 0;
    r->weekday = number(p); r->seconds = 7200;
    if (**p == '/') {
        ++*p;
        int minutes;
        if (!offset(p, &minutes) || minutes < 0) return 0;
        r->seconds = minutes * 60;
    }
    return r->month >= 1 && r->month <= 12 && r->week >= 1 && r->week <= 5 && r->weekday >= 0 && r->weekday <= 6;
}
void tzset(void)
{
    memset(&zone, 0, sizeof(zone)); strcpy(zone.standard, "GMT");
    const char *p = getenv("TZ");
    if (!p || !*p) return;
    /* Parse into a temporary object; malformed TZ falls back to GMT. */
    char standard[16], daylight[16]; int west;
    struct rule start, end;
    if (!name(&p, standard) || !offset(&p, &west)) return;
    if (*p) {
        if (!name(&p, daylight)) return;
        if (*p) {
            if (*p++ != ',' || !rule(&p, &start) || *p++ != ',' || !rule(&p, &end) || *p) return;
            zone.start = start; zone.end = end; zone.explicit_rules = 1;
        }
        strcpy(zone.daylight, daylight); zone.dst = 1;
    }
    strcpy(zone.standard, standard); zone.west = west;
}
struct tm *gmtime(const time_t *value)
{
    if (!value || *value < -62135596800LL || *value > 253402300799LL) { errno = EINVAL; return NULL; }
    int64_t days = *value / 86400, seconds = *value % 86400;
    if (seconds < 0) { seconds += 86400; --days; }
    memset(&result, 0, sizeof(result));
    result.tm_sec = seconds % 60; result.tm_min = seconds / 60 % 60; result.tm_hour = seconds / 3600;
    result.tm_wday = (days + 4) % 7;
    if (result.tm_wday < 0) result.tm_wday += 7;
    int year = 70;
    while (days < 0) { --year; days += dysize(year); }
    while (days >= dysize(year)) { days -= dysize(year); ++year; }
    result.tm_year = year; result.tm_yday = days;
    int month = 0;
    while (days >= (int)(lengths[month] + (month == 1 && dysize(year) == 366))) {
        days -= lengths[month] + (month == 1 && dysize(year) == 366); ++month;
    }
    result.tm_mon = month; result.tm_mday = days + 1;
    return &result;
}
static int sunday(const struct tm *t, int day)
{
    if (day >= 58) day += dysize(t->tm_year) - 365;
    return day - (day - t->tm_yday + t->tm_wday + 700) % 7;
}
static int transition(const struct tm *t, const struct rule *r)
{
    int first = 0;
    for (int m = 1; m < r->month; ++m) first += lengths[m-1] + (m == 2 && dysize(t->tm_year) == 366);
    int weekday = (t->tm_wday - t->tm_yday + first + 371) % 7;
    int day = (r->weekday - weekday + 7) % 7 + (r->week - 1) * 7;
    int size = lengths[r->month-1] + (r->month == 2 && dysize(t->tm_year) == 366);
    if (day >= size) day -= 7;
    return (first + day) * 86400 + r->seconds;
}
struct tm *localtime(const time_t *value)
{
    tzset();
    if (!value || *value < -62135596800LL || *value > 253402300799LL) { errno = EINVAL; return NULL; }
    time_t adjusted = *value - zone.west * 60;
    struct tm *t = gmtime(&adjusted);
    if (!t || !zone.dst) return t;
    int begin, end;
    if (zone.explicit_rules) {
        begin = transition(t, &zone.start);
        /* End rule is expressed in daylight time, one hour ahead of standard. */
        end = transition(t, &zone.end) - 3600;
    } else {
        int first = t->tm_year == 74 ? 5 : t->tm_year == 75 ? 58 : 119;
        int last = t->tm_year == 74 ? 333 : 303;
        begin = sunday(t, first) * 86400 + 7200;
        end = sunday(t, last) * 86400 + 3600;
    }
    int now = t->tm_yday * 86400 + t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
    int dst = begin < end ? now >= begin && now < end : now >= begin || now < end;
    if (dst) { adjusted += 3600; t = gmtime(&adjusted); if (t) t->tm_isdst = 1; }
    return t;
}
char *asctime(const struct tm *t)
{
    static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    static const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    if (!t || t->tm_year < -1899 || t->tm_year > 8099 || t->tm_mon < 0 || t->tm_mon > 11 ||
        t->tm_wday < 0 || t->tm_wday > 6 || t->tm_mday < 1 || t->tm_mday > 31 ||
        t->tm_hour < 0 || t->tm_hour > 23 || t->tm_min < 0 || t->tm_min > 59 || t->tm_sec < 0 || t->tm_sec > 60) {
        errno = EINVAL; return NULL;
    }
    snprintf(text, sizeof(text), "%s %s %2d %02d:%02d:%02d %04d\n", days[t->tm_wday], months[t->tm_mon],
        t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, t->tm_year + 1900);
    return text;
}
char *ctime(const time_t *value) { return asctime(localtime(value)); }
char *timezone(int west, int dst)
{
    tzset();
    if (west == zone.west && (!dst || zone.dst)) return dst ? zone.daylight : zone.standard;
    static const struct { int west; char *standard, *daylight; } names[] = {
        {240,"AST","ADT"},{300,"EST","EDT"},{360,"CST","CDT"},{420,"MST","MDT"},{480,"PST","PDT"},{0,"GMT",NULL}
    };
    for (unsigned i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
        if (west == names[i].west && (!dst || names[i].daylight)) return dst ? names[i].daylight : names[i].standard;
    static char buffer[32];
    int64_t magnitude = west < 0 ? -(int64_t)west : west;
    snprintf(buffer, sizeof(buffer), "GMT%s%ld:%02ld", west < 0 ? "+" : "-", (long)(magnitude/60), (long)(magnitude%60));
    return buffer;
}
int ftime(struct timeb *out)
{
    if (!out) { errno = EFAULT; return -1; }
    time_t now = time(NULL);
    if (now == -1) return -1;
    tzset();
    *out = (struct timeb){now, 0, zone.west, zone.dst};
    return 0; /* Current clock ABI exposes whole seconds only. */
}
