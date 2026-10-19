#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>
#include <errno.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>
#include <nexix/timeb.h>
int errno;
static char *setting;
long nx_syscall6(long n,long p,long a,long b,long c,long d,long e)
{
    assert(n==NX_SYS_FTIME && !a && !b && !c && !d && !e);
    if(!p)return -EFAULT;
    *(struct nx_timeb *)p=(struct nx_timeb){.time=1709210096,.millitm=570};
    return 0;
}
char *getenv(const char *name) { assert(!strcmp(name, "TZ")); return setting; }
time_t time(time_t *out) { if(out) *out=1709210096; return 1709210096; }
static void check(time_t epoch, char *tz, const char *expected, int dst)
{
    setting = tz;
    struct tm *t = localtime(&epoch);
    assert(t && t->tm_isdst == dst);
    if(strcmp(asctime(t), expected)) {
        printf("got %s expected %s", asctime(t), expected); assert(0);
    }
    assert(!strcmp(ctime(&epoch), expected));
}
int main(void)
{
    check(0,NULL,"Thu Jan  1 00:00:00 1970\n",0);
    check(0,"EST5","Wed Dec 31 19:00:00 1969\n",0);
    check(-1,"GMT0","Wed Dec 31 23:59:59 1969\n",0);
    check(1709210096,"CET-1","Thu Feb 29 13:34:56 2024\n",0);
    check(1709210096,"IST-5:30","Thu Feb 29 18:04:56 2024\n",0);
    char *prague="CET-1CEST,M3.5.0,M10.5.0/3";
    check(1711846799,prague,"Sun Mar 31 01:59:59 2024\n",0);
    check(1711846800,prague,"Sun Mar 31 03:00:00 2024\n",1);
    check(1729990799,prague,"Sun Oct 27 02:59:59 2024\n",1);
    check(1729990800,prague,"Sun Oct 27 02:00:00 2024\n",0);
    check(1704067200,"AEST-10AEDT,M10.1.0,M4.1.0/3","Mon Jan  1 11:00:00 2024\n",1);
    check(126687600,"EST5EDT","Sun Jan  6 03:00:00 1974\n",1);
    check(162370800,"EST5EDT","Sun Feb 23 03:00:00 1975\n",1);
    check(0,"Europe/Prague","Thu Jan  1 00:00:00 1970\n",0);
    char *bad[]={"CET-","CET-1CEST,","CET-1CEST,M3.","CET-1CEST,M13.1.0,M10.5.0","CET-1CEST,M3.5.0,M10.5.0/junk","GMT25","GMT1:90"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) check(0,bad[i],"Thu Jan  1 00:00:00 1970\n",0);
    time_t future=4107542400; setting="GMT0";
    struct tm *t=gmtime(&future); assert(t && t->tm_year==200 && t->tm_mon==2 && t->tm_mday==1);
    assert(dysize(200)==365 && dysize(100)==366);
    assert(!strcmp(timezone(300,1),"EDT"));
    setting=prague; assert(!strcmp(timezone(-60,1),"CEST"));
    struct timeb b; assert(ftime(&b)==0 && b.time==1709210096 && b.timezone==-60 && b.dstflag==1 && b.millitm==570);
    assert(ftime(NULL)==-1 && errno==EFAULT);
    time_t huge=INT64_MAX; assert(!gmtime(&huge) && errno==EINVAL);
    assert(!asctime(NULL) && errno==EINVAL);
    puts("PASS: UTC calendar, negative epochs, offsets, TZ changes, DST transitions, historical US exceptions, malformed TZ and ftime");
}
