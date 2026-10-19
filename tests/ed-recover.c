#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do{if(!(c)){printf("ED_RECOVERY_ERROR %d errno %d\n",__LINE__,errno);return 1;}}while(0)
static int transfer(int fd,void *buffer,size_t n,int output)
{
    char *p=buffer;
    while(n){ssize_t done=output?write(fd,p,n):read(fd,p,n);if(done<0 && errno==EINTR)continue;
        if(done<=0)return -1;
        p+=done;n-=done;
    }
    return 0;
}
int main(void)
{
    int input[2],output[2],status,fd;char bytes[8];
    CHECK(!pipe(input) && !pipe(output));
    CHECK(signal(SIGHUP,SIG_DFL)!=SIG_ERR);
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        if(dup2(input[0],0)!=0 || dup2(output[1],1)!=1)_exit(100);
        close(input[0]);close(input[1]);close(output[0]);close(output[1]);
        execl("/bin/ed","ed","-",(char *)0);_exit(101);
    }
    close(input[0]);close(output[1]);
    char commands[]="a\nrecover\n.\n1p\n";
    CHECK(!transfer(input[1],commands,sizeof(commands)-1,1));
    CHECK(!transfer(output[0],bytes,sizeof(bytes),0) && !memcmp(bytes,"recover\n",8));
    CHECK(!kill(child,SIGHUP));CHECK(wait(&status)==child && status==0);
    close(input[1]);close(output[0]);
    fd=open("ed.hup",O_RDONLY);CHECK(fd>=0);
    CHECK(!transfer(fd,bytes,sizeof(bytes),0) && !memcmp(bytes,"recover\n",8));
    CHECK(read(fd,bytes,1)==0 && !close(fd) && !unlink("ed.hup"));
    puts("ED_RECOVERY_OK");return 0;
}
