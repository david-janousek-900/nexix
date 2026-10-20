/* V7 ps selection and columns, retargeted from kernel memory to nx_kinfo.
 * Source behavior: reference/v7/usr/src/cmd/ps.c. No private addresses escape. */
#include <sys/kinfo.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
struct device { unsigned dev; char name[15]; };
static struct device devices[256];
static unsigned ndev;
static void getdev(void)
{
    FILE *f=fopen("/dev","r");struct direct d;struct stat st;char path[32];
    if(!f)return;
    while(ndev<256 && fread(&d,sizeof d,1,f)==1){
        if(!d.d_ino || (strncmp(d.d_name,"tty",3) && strncmp(d.d_name,"console",7)))continue;
        memcpy(devices[ndev].name,d.d_name,14);devices[ndev].name[14]=0;
        strcpy(path,"/dev/");strcat(path,devices[ndev].name);
        if(stat(path,&st) || !S_ISCHR(st.st_mode))continue;
        devices[ndev++].dev=st.st_rdev;
    }
    fclose(f);
}
static const char *terminal(unsigned dev)
{
    unsigned i;for(i=0;i<ndev;i++)if(devices[i].dev==dev)return devices[i].name;
    return "?";
}
static void printtime(int64_t ticks,unsigned hz)
{
    uint64_t seconds=(ticks+(hz/2))/hz;
    printf(" %2lu:%02lu",(unsigned long)(seconds/60),(unsigned long)(seconds%60));
}
static int usage(void){fputs("usage: ps [alvx] [tTTY] [PID]\nps: archived kernel-memory mode (-k/namelist) is unavailable on Nexix\n",stderr);return 2;}
int main(int argc,char **argv)
{
    int all=0,without=0,longform=0,children=0,pid=0,matched=0,i;char *tty=NULL,*p,*end;
    for(i=1;i<argc;i++){
        p=argv[i];if(*p=='-')p++;
        if(!*p)return usage();
        while(*p){
            switch(*p++){
            case 'a':all=1;break;case 'x':without=1;break;
            case 'l':longform=1;break;case 'v':children=1;break;
            case 't':tty=p;if(!*tty){if(++i>=argc)return usage();tty=argv[i];}
                if(!strncmp(tty,"/dev/",5))tty+=5;
                all=1;if(*tty=='?')without=1;p="";break;
            default:
                --p;if(*p<'0'||*p>'9')return usage();
                {long value=strtol(p,&end,10);if(*end || value<=0 || value>32767)return usage();pid=value;p=end;}
            }
        }
    }
    /* Retry size races: a fork can publish another process between snapshots. */
    void *buffer=NULL;ssize_t bytes=-1;
    for(i=0;i<8;i++){
        ssize_t size=nx_kinfo(NX_KINFO_VERSION,NX_KINFO_PROCESS,NULL,0);
        if(size<0)break;
        free(buffer);buffer=malloc(size+16*sizeof(struct nx_kinfo_process));
        if(!buffer){errno=ENOMEM;break;}
        bytes=nx_kinfo(NX_KINFO_VERSION,NX_KINFO_PROCESS,buffer,size+16*sizeof(struct nx_kinfo_process));
        if(bytes>=0 || errno!=ERANGE)break;
    }
    if(bytes<0){perror("ps");free(buffer);return 1;}
    struct nx_kinfo_header *h=buffer;
    if(h->record_size!=sizeof(struct nx_kinfo_process) || h->header_size!=sizeof(*h) || !h->tick_hz){fputs("ps: invalid process snapshot\n",stderr);free(buffer);return 1;}
    struct nx_kinfo_process *r=(void *)(h+1);getdev();
    if(longform)puts(" F S UID   PID  PPID CPU PRI NICE   SZ WAIT TTY      TIME CMD");
    else if(!pid)puts("   PID TTY      TIME CMD");
    unsigned n;for(n=0;n<h->count;n++){
        const struct nx_kinfo_process *q=&r[n];const char *t=terminal(q->tty_dev);
        if(!without && (q->tty_dev==65535 || (!q->pgrp && !q->ruid)))continue;
        if((!all && q->ruid!=getuid()) || (pid && q->pid!=pid))continue;
        if(tty && strcmp(tty,t) && !(strlen(t)>3 && !strcmp(tty,t+3)))continue;
        if(longform)printf("%2o %c%4u",q->flags,q->state<7?"0SWRIZT"[q->state]:'?',q->ruid);
        printf("%6d",q->pid);
        if(longform)printf("%6d%4u%4u%5d%5lu %4s",q->ppid,q->cpu,q->priority,(int)q->nice-20,(unsigned long)((q->size_bytes+1023)/1024),q->waiting?"yes":"");
        printf(" %-5s",t);
        if(q->state==NX_KPROC_ZOMBIE)printf("  <defunct>");
        else {
            printtime(q->user_ticks+q->system_ticks,h->tick_hz);
            if(children && !longform){printtime(q->child_system_ticks,h->tick_hz);printtime(q->child_user_ticks,h->tick_hz);}
            printf(" %.16s",q->command);
        }
        putchar('\n');matched=1;
    }
    free(buffer);if(fflush(stdout)==EOF)return 1;return matched?0:1;
}
