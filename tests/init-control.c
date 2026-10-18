/* Private root-only signal/state observer for init acceptance. */
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utmp.h>
#include <sys/kinfo.h>
#include <sys/stat.h>
#define CHECK(c) do{if(!(c)){printf("INIT_CONTROL_ERROR %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    CHECK(argc==2 && geteuid()==0);
    if(!strcmp(argv[1],"reload"))return kill(1,SIGINT)?1:0;
    if(!strcmp(argv[1],"single"))return kill(1,SIGHUP)?1:0;
    int no_peer=!strcmp(argv[1],"no-peer");
    struct utmp users[3]={0};int fd=open("/etc/utmp",O_RDONLY);
    ssize_t wanted=no_peer?2*sizeof(*users):sizeof(users);
    CHECK(fd>=0 && read(fd,users,wanted)==wanted && !close(fd));
    if(no_peer){struct stat st;CHECK(!stat("/usr/adm/wtmp",&st) && st.st_size<=48);}
    int recovery=!strcmp(argv[1],"recovery"),two=!strcmp(argv[1],"two");
    CHECK(!strncmp(users[1].ut_name,recovery?"":"root",8));
    CHECK(!strncmp(users[2].ut_name,two?"bob":"",8));
    ssize_t bytes=nx_kinfo(1,NX_KINFO_PROCESS,NULL,0);CHECK(bytes>=48);
    char *buffer=malloc(bytes);CHECK(buffer && nx_kinfo(1,NX_KINFO_PROCESS,buffer,bytes)==bytes);
    struct nx_kinfo_header *h=(void *)buffer;struct nx_kinfo_process *p=(void *)(buffer+sizeof(*h));
    int bob=0,root=0;
    for(unsigned n=0;n<h->count;++n){
        if(p[n].euid==101){CHECK(two && p[n].tty_dev==0x101 && p[n].pgrp==p[n].pid);++bob;}
        if(!p[n].euid && p[n].tty_dev==0x100 && p[n].pgrp==p[n].pid)++root;
    }
    CHECK(bob==(two?1:0) && root==1);free(buffer);puts("INIT_STATE_OK");return 0;
}
