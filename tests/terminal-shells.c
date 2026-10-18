/* Private init for concurrent real Bourne shell acceptance before getty/login. */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/kinfo.h>
struct resources {ssize_t processes,files,inodes,text;uint64_t pages;};
static int snapshot(struct resources *r)
{
    struct {struct nx_kinfo_header header;struct nx_kinfo_memory memory;} info;
    r->processes=nx_kinfo(1,NX_KINFO_PROCESS,NULL,0);
    r->files=nx_kinfo(1,NX_KINFO_FILE,NULL,0);
    r->inodes=nx_kinfo(1,NX_KINFO_INODE,NULL,0);
    r->text=nx_kinfo(1,NX_KINFO_TEXT,NULL,0);
    if(r->processes<48 || r->files<48 || r->inodes<48 || r->text<48 ||
        nx_kinfo(1,NX_KINFO_MEMORY,&info,sizeof(info))!=sizeof(info))return 1;
    r->pages=info.memory.free_pages;return 0;
}
int main(void)
{
    struct resources before,after;int failed=0,results[4]={0};
    if(getpid()!=1 || snapshot(&before))return 1;
    for(unsigned round=0;round<4;++round){
        pid_t children[2];
        for(unsigned line=0;line<2;++line){
            children[line]=fork();if(children[line]<0)return 2;
            if(!children[line]){
                if(setpgrp()!=getpid() || open(line?"/dev/ttyS1":"/dev/console",O_RDWR)!=0 || dup(0)!=1 || dup(0)!=2)_exit(3);
                printf("ROUND_%u_LINE_%u\n",round,line);fflush(stdout);
                execl("/bin/sh","-sh",(char *)0);_exit(4);
            }
        }
        for(unsigned n=0;n<2;++n){
            int status;pid_t p=wait(&status);
            if((p!=children[0] && p!=children[1]) || status){results[round]=status?status:5;failed=1;}
        }
        if(snapshot(&after) || memcmp(&before,&after,sizeof(before))){results[round]=6;failed=1;}
    }
    if(open("/dev/console",O_RDWR)!=0 || dup(0)!=1 || dup(0)!=2)return 7;
    if(failed)printf("RESOURCE_ERRORS %d %d %d %d\n",results[0],results[1],results[2],results[3]);
    printf("SHELL_RECOVERY_%s\n",failed?"FAILED":"PASSED");fflush(stdout);
    for(;;)pause();
}
