#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
static int leap(int y){return !(y%4)&&(y%100||!(y%400));}
char *ctime(const time_t *value)
{
    static char result[26];
    const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    const unsigned lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if(*value<0||*value>253402300799LL){errno=EINVAL;return NULL;}
    int64_t total=*value/86400,remaining=total;int y=1970,m=0;
    while(remaining>=365+leap(y)){remaining-=365+leap(y);++y;}
    while(remaining>=lengths[m]+(m==1&&leap(y))){remaining-=lengths[m]+(m==1&&leap(y));++m;}
    int seconds=*value%86400;
    snprintf(result,sizeof(result),"%s %s %2d %02d:%02d:%02d %04d\n",days[(total+4)%7],months[m],(int)remaining+1,seconds/3600,seconds/60%60,seconds%60,y);
    return result;
}
