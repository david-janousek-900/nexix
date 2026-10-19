/* Native counterpart of V7 dmesg: read a checked kernel log snapshot rather
 * than dereference /dev/mem using PDP-11 namelist addresses. */
#include <nexix/libc.h>
#include <nexix/syscall.h>
#include <nexix/klog.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static char current[NX_KLOG_CAPACITY],previous[NX_KLOG_CAPACITY+1];
static int write_all(int fd,const char *p,size_t n)
{
    while(n){ssize_t done=write(fd,p,n);if(done<0 && errno==EINTR)continue;
        if(done<=0)return -1;p+=done;n-=done;}
    return 0;
}
int main(int argc,char **argv)
{
    int incremental=argc==2 && !strcmp(argv[1],"-");
    if(argc!=1 && !incremental){fputs("usage: dmesg [-]\n",stderr);return 2;}
    long length=nx_syscall6(NX_SYS_KLOG,(long)current,sizeof(current),0,0,0,0);
    if(length<0){errno=-length;perror("dmesg");return 1;}
    size_t old=0,skip=0;
    if(incremental){
        int fd=open("/usr/adm/msgbuf",O_RDONLY);
        if(fd<0 && errno!=ENOENT){perror("dmesg: /usr/adm/msgbuf");return 1;}
        if(fd>=0){
            while(old<sizeof(previous)){
                ssize_t n=read(fd,previous+old,sizeof(previous)-old);
                if(n<0 && errno==EINTR)continue;
                if(n<0){close(fd);perror("dmesg: read snapshot");return 1;}
                if(!n)break;old+=n;
            }
            close(fd);
            if(old>sizeof(current)){fputs("dmesg: invalid saved snapshot\n",stderr);return 1;}
        }
        /* The ring may have dropped its oldest lines since the previous call. */
        size_t overlap=old<(size_t)length?old:(size_t)length;
        for(;overlap;--overlap){
            size_t start=old-overlap;
            if(start && previous[start-1]!='\n')continue;
            if(!memcmp(previous+start,current,overlap)){skip=overlap;break;}
        }
        if(old && !skip && write_all(1,"...\n",4)){perror("dmesg");return 1;}
    }
    if(write_all(1,current+skip,(size_t)length-skip)){perror("dmesg");return 1;}
    if(incremental){
        int fd=open("/usr/adm/msgbuf",O_WRONLY|O_CREAT|O_TRUNC,0600);
        if(fd<0){perror("dmesg: /usr/adm/msgbuf");return 1;}
        int error=write_all(fd,current,length);
        if(close(fd)<0 || error){perror("dmesg: save snapshot");return 1;}
    }
    return 0;
}
