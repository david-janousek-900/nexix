#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sgtty.h>
#include "command-runner.h"
#define CHECK(c) do{if(!(c)){printf("STTY_ERROR %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    struct sgttyb a,b,after;char error[512];
    int fd=open("/dev/ttyS1",O_RDWR);CHECK(fd>=0 && !gtty(0,&a) && !gtty(fd,&b));
    char *query[]={"/bin/stty",NULL};
    CHECK(run_command(query,0,fd,error,sizeof(error))==0 && !strncmp(error,"speed 115200 baud\n",18));
    char *change[]={"/bin/stty","9600","erase","^H","kill","^U","-echo",NULL};
    CHECK(run_command(change,0,fd,error,sizeof(error))==0 && !*error);
    CHECK(!gtty(fd,&after) && after.sg_ispeed==B9600 && after.sg_ospeed==B9600 && after.sg_erase==8 && after.sg_kill==21 && !(after.sg_flags&ECHO));
    CHECK(!gtty(0,&after) && !memcmp(&a,&after,sizeof(a)));
    CHECK(run_command(query,0,fd,error,sizeof(error))==0 && !strcmp(error,"speed 9600 baud\nerase = '^H'; kill = '^U'\n-nl \n"));
    char *raw[]={"/bin/stty","raw",NULL};
    CHECK(run_command(raw,0,fd,error,sizeof(error))==0 && !gtty(fd,&after) && (after.sg_flags&RAW));
    char *bad[]={"/bin/stty","erase",NULL};
    CHECK(run_command(bad,0,fd,error,sizeof(error))==1 && !strcmp(error,"stty: missing erase character\n"));
    char *unknown[]={"/bin/stty","nonsense",NULL};
    CHECK(run_command(unknown,0,fd,error,sizeof(error))==1 && !strcmp(error,"unknown mode: nonsense\n"));
    CHECK(!stty(fd,&b) && !close(fd));
    fd=creat("/tmp/notty",0600);CHECK(fd>=0);
    CHECK(run_command(query,0,fd,error,sizeof(error))==1 && !strcmp(error,"stty: not a tty\n"));
    CHECK(!close(fd) && !unlink("/tmp/notty"));puts("STTY_OK");return 0;
}
