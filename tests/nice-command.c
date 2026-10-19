#include "command-fixture.h"
#include <sys/kinfo.h>
static unsigned niceness(void)
{
 unsigned char buffer[16384];ssize_t size=nx_kinfo(NX_KINFO_VERSION,NX_KINFO_PROCESS,buffer,sizeof(buffer));CHECK(size>0);struct nx_kinfo_header *h=(void *)buffer;unsigned i;for(i=0;i<h->count;++i){struct nx_kinfo_process *p=(void *)(buffer+h->header_size+i*h->record_size);if(p->pid==getpid())return p->nice;}CHECK(0);return 0;
}
int main(int argc,char **argv)
{
 (void)argv;
 if(argc>1){printf("%u\n",niceness());return 0;}
 unsigned n=niceness();char expected[32];
 sprintf(expected,"%u\n",n+10>39?39:n+10);command(CMD("/bin/nice","/bin/check","probe"),0,expected,"");
 sprintf(expected,"%u\n",n+5>39?39:n+5);command(CMD("/bin/nice","-5","/bin/check","probe"),0,expected,"");
 sprintf(expected,"%u\n",n<5?0:n-5);command(CMD("/bin/nice","--5","/bin/check","probe"),0,expected,"");CHECK(niceness()==n);
 command(CMD("/bin/nice"),1,"","usage: nice [ -n ] command\n");command(CMD("/bin/nice","-bad","echo"),1,"","usage: nice [ -n ] command\n");
 command(CMD("/bin/nice","absentcmd"),1,"","No such file or directory: absentcmd\n");command(CMD("/bin/nice","echo","PATH_OK"),0,"PATH_OK\n","");
 putfile("/tmp/nicescript","echo SCRIPT_OK\n");CHECK(!chmod("/tmp/nicescript",0755));command(CMD("/bin/nice","/tmp/nicescript"),0,"SCRIPT_OK\n","");
 puts("NICE_OK");return 0;
}
