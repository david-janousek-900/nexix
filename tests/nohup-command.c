#include "command-fixture.h"
#include <signal.h>
#include <sys/kinfo.h>
static unsigned niceness(void)
{
 unsigned char data[16384];CHECK(nx_kinfo(NX_KINFO_VERSION,NX_KINFO_PROCESS,data,sizeof(data))>0);struct nx_kinfo_header *h=(void *)data;
 for(unsigned i=0;i<h->count;++i){struct nx_kinfo_process *p=(void *)(data+h->header_size+i*h->record_size);if(p->pid==getpid())return p->nice;}CHECK(0);return 0;
}
int main(int argc,char **argv)
{
 if(argc>1 && !strcmp(argv[1],"probe")){
  CHECK(signal(SIGHUP,SIG_IGN)==SIG_IGN && signal(SIGTERM,SIG_IGN)==SIG_IGN);
  CHECK(!raise(SIGHUP) && !raise(SIGTERM));printf("IGNORED %u\n",niceness());return 0;
 }
 unsigned n=niceness();char expected[64];sprintf(expected,"IGNORED %u\n",n+5>39?39:n+5);
 command(CMD("/bin/sh","/bin/nohup","/bin/check","probe"),0,expected,"");CHECK(niceness()==n);
 command(CMD("/bin/sh","/bin/nohup","/bin/echo","PIPE_OUTPUT"),0,"PIPE_OUTPUT\n","");
 command(CMD("/bin/sh","/bin/nohup","absentcmd"),1,"No such file or directory: absentcmd\n","");
 command(CMD("/bin/sh","/bin/nohup"),1,"usage: nice [ -n ] command\n","");
 command(CMD("/bin/sh","/bin/nohup","/bin/false"),1,"","");
 CHECK(access("nohup.out",F_OK)<0);
 char error[256];CHECK(run_command(CMD("/bin/sh","/bin/nohup","/bin/echo","TTY_SAVED"),0,1,error,sizeof(error))==0 && !*error);
 contents("nohup.out","TTY_SAVED\n");
 CHECK(run_command(CMD("/bin/sh","/bin/nohup","/bin/echo","APPEND"),0,1,error,sizeof(error))==0 && !*error);
 contents("nohup.out","TTY_SAVED\nAPPEND\n");CHECK(!unlink("nohup.out"));
 puts("NOHUP_OK");return 0;
}
