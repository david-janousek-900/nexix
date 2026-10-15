#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
static volatile long datum=33;
extern int trace_patch_target(void);
int main(int argc,char **argv)
{
    if(argc!=4)return 90;
    int mode=atoi(argv[1]),ready=atoi(argv[2]),go=atoi(argv[3]);
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
