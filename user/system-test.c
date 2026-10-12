#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
static volatile sig_atomic_t received;
static void handler(int n){received=n;}
int system_probe(const char *mode)
{
    if(!strcmp(mode,"system-signal-exit")){raise(SIGTERM);return 90;}
    if(!strcmp(mode,"system-default"))return signal(SIGINT,SIG_DFL)==SIG_DFL?0:91;
    if(!strcmp(mode,"system-ignored"))return signal(SIGINT,SIG_DFL)==SIG_IGN?0:92;
    return 93;
}
int system_test(void)
{
    CHECK(system(NULL)==-1&&errno==EINVAL);
    CHECK(system("")==0&&system("exit 7")==(7<<8)&&system("exit 255")==(255<<8));
    CHECK(system("exec /bin/v7-test system-signal-exit")==SIGTERM);
    CHECK(signal(SIGINT,handler)==SIG_DFL&&signal(SIGQUIT,handler)==SIG_DFL);
    CHECK(system("exec /bin/v7-test system-default")==0&&received==0);
    CHECK(signal(SIGINT,SIG_DFL)==handler&&signal(SIGQUIT,SIG_DFL)==handler);
    CHECK(signal(SIGINT,SIG_IGN)==SIG_DFL);
    CHECK(system("exec /bin/v7-test system-ignored")==0);
    CHECK(signal(SIGINT,SIG_DFL)==SIG_IGN);
    char **environment=environ;
    char *custom[]={"TASK48=inherited","PATH=/invalid",NULL};environ=custom;
    CHECK(chdir("/tmp")==0);
    CHECK(system("/bin/echo \"$TASK48\" > task48; exit 3")==(3<<8));
    environ=environment;
    char text[32];int fd=open("task48",O_RDONLY);CHECK(fd>=0);
    CHECK(read(fd,text,sizeof(text))==10&&!memcmp(text,"inherited\n",10));
    CHECK(close(fd)==0&&unlink("task48")==0&&chdir("/")==0);
    for(int i=0;i<12;++i)CHECK(system("exit 0")==0);
    int status;CHECK(wait(&status)==-1&&errno==ECHILD);
    CHECK(printf("NEXIX_SYSTEM_OK\n")>0);
    return 0;
}
