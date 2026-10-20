#include <sys/kinfo.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#define CHECK(x) do{if(!(x)){printf("uptime check failed: %d\n",__LINE__);return 1;}}while(0)
struct snapshot {struct nx_kinfo_header h;struct nx_kinfo_load l;};
static int readload(struct snapshot *s){return nx_kinfo(1,NX_KINFO_LOAD,s,sizeof(*s));}
int main(void)
{
    struct snapshot before,after;struct tms t;int status;pid_t worker;
    CHECK(readload(&before)==sizeof(before));
    CHECK(before.h.count==1 && before.h.record_size==32 && before.l.scale==65536 && !before.l.reserved);
    CHECK(nx_kinfo(1,NX_KINFO_LOAD,NULL,0)==sizeof(before));
    CHECK(nx_kinfo(1,NX_KINFO_LOAD,&after,sizeof(after)-1)==-1 && errno==ERANGE);
    CHECK(nx_kinfo(1,NX_KINFO_LOAD,(void *)1,sizeof(after))==-1 && errno==EFAULT);
    worker=fork();CHECK(worker>=0);
    if(!worker){volatile unsigned long x=0;for(;;)x++;}
    clock_t until=times(&t)+3*CLK_TCK;
    while(times(&t)<until){} /* Keep two real tasks runnable across samples. */
    CHECK(readload(&after)==sizeof(after));
    CHECK(after.h.ticks>before.h.ticks && after.l.average[0]>before.l.average[0]);
    CHECK(after.l.average[0]>after.l.average[1] && after.l.average[1]>after.l.average[2]);
    CHECK(kill(worker,SIGTERM)==0 && wait(&status)==worker);
    /* Wall clock changes must never change elapsed boot ticks or load samples. */
    time_t wall=time(NULL),changed=wall+3600;CHECK(stime(&changed)==0);
    CHECK(readload(&before)==sizeof(before) && before.h.ticks>=after.h.ticks);
    CHECK(stime(&wall)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){if(setuid(123) || readload(&before)!=sizeof(before))_exit(1);_exit(0);}
    CHECK(wait(&status)==child && status==0);
    puts("UPTIME_KERNEL_OK");return 0;
}
