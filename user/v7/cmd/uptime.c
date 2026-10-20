/* Native uptime: monotonic boot ticks, utmp sessions and sampled kernel load. */
#include <sys/kinfo.h>
#include <stdio.h>
#include <time.h>
#include <utmp.h>
#include <stdint.h>
int main(int argc,char **argv)
{
    struct {struct nx_kinfo_header header;struct nx_kinfo_load load;} snapshot;
    struct utmp record;FILE *f;unsigned users=0;time_t now;struct tm *tm;uint64_t seconds,days,hours,minutes;
    (void)argv;
    if(argc!=1){fputs("usage: uptime\n",stderr);return 2;}
    if(nx_kinfo(NX_KINFO_VERSION,NX_KINFO_LOAD,&snapshot,sizeof snapshot)<0){perror("uptime");return 1;}
    if(snapshot.header.count!=1 || snapshot.header.record_size!=sizeof(snapshot.load) || !snapshot.header.tick_hz || !snapshot.load.scale){fputs("uptime: invalid kernel snapshot\n",stderr);return 1;}
    seconds=snapshot.header.ticks/snapshot.header.tick_hz;days=seconds/86400;hours=(seconds/3600)%24;minutes=(seconds/60)%60;
    f=fopen("/etc/utmp","r");if(!f){perror("uptime: /etc/utmp");return 1;}
    while(fread(&record,sizeof record,1,f)==1)if(record.ut_name[0])users++;
    if(ferror(f)){fclose(f);fputs("uptime: accounting read error\n",stderr);return 1;}fclose(f);
    now=time(NULL);tm=localtime(&now);if(!tm){fputs("uptime: cannot read system time\n",stderr);return 1;}
    printf(" %02d:%02d:%02d up ",tm->tm_hour,tm->tm_min,tm->tm_sec);
    if(days)printf("%lu day%s, ",(unsigned long)days,days==1?"":"s");
    printf("%lu:%02lu, %u user%s, load average: ",(unsigned long)hours,(unsigned long)minutes,users,users==1?"":"s");
    for(unsigned i=0;i<3;i++){
        uint64_t hundredths=((uint64_t)snapshot.load.average[i]*100+snapshot.load.scale/2)/snapshot.load.scale;
        printf("%s%lu.%02lu",i?", ":"",(unsigned long)(hundredths/100),(unsigned long)(hundredths%100));
    }
    putchar('\n');return fflush(stdout)==EOF;
}
