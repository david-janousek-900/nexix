#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <nexix/mpx.h>
#include <nexix/libc.h>
#include <errno.h>
#include <fcntl.h>
static volatile long datum=33;
extern int trace_patch_target(void);
int main(int argc,char **argv)
{
    if(argc!=4)return 90;
    int mode=atoi(argv[1]),ready=atoi(argv[2]),go=atoi(argv[3]);
    if(mode==8){
        struct nx_mx_args a={.arg={0,ready,0}};
        return fcntl(go,F_GETFD)==-1 && errno==EBADF &&
            nx_syscall6(NX_SYS_MPX,NX_MX_CHAN,(long)&a,0,0,0,0)==0xfff0?0:85;
    }
    if(mode==7){
        volatile uint64_t *a=(void *)NX_PHYS_ADDRESS(0),*b=(void *)NX_PHYS_ADDRESS(1);
        if(a[0]!=0x5566 || a[512]!=0x7788 || b[0]!=0x7788)return 87;
        a[0]=0x9999;b[0]=0xaaaa;char c;
        return write(ready,"P",1)==1 && read(go,&c,1)==1?0:86;
    }
    if(mode==6){char c;
        return write(ready,"L",1)==1 && read(go,&c,1)==1?0:88;
    }
    if(mode==5){long n=0x912;__asm__ volatile("int $0x81":"+a"(n)::"memory","cc");return n?89:0;}
    if(mode==3)return trace_patch_target()==41&&datum==33?0:91;
    int hold[2];pid_t helper=-1;
    if(mode==1){
        if(pipe(hold))return 92;
        helper=fork();if(helper<0)return 93;
        if(!helper){close(hold[1]);char b;
            _exit(read(hold[0],&b,1)==1&&trace_patch_target()==41&&datum==33?0:94);}
        close(hold[0]);
    }
    uintptr_t addresses[2]={(uintptr_t)trace_patch_target,(uintptr_t)&datum};
    if(ptrace(0,0,0,0)||write(ready,addresses,sizeof(addresses))!=sizeof(addresses))return 95;
    raise(SIGINT);
    if(trace_patch_target()!=(mode==0||mode==4?42:41)||datum!=77)return 96;
    if(mode==4){
        pid_t copy=fork();if(copy<0)return 97;
        if(!copy)_exit(trace_patch_target()==42&&datum==77?0:98);
        int status;if(wait(&status)!=copy||status)return 99;
    }
    if(helper>0){
        if(write(hold[1],"G",1)!=1||close(hold[1]))return 100;
        int status;if(wait(&status)!=helper||status)return 101;
    }
    (void)go;return 0;
}
