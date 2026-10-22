#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
int errno;
static char captured[256];
static size_t used;
static int interrupt_once,fail,zero;
ssize_t write(int fd,const void *bytes,size_t count)
{
    if(fd!=2){errno=EBADF;return -1;}
    if(interrupt_once){interrupt_once=0;errno=EINTR;return -1;}
    if(fail){errno=fail;return -1;}
    if(zero)return 0;
    if(count>2)count=2; /* Force partial writes within every message. */
    if(count>sizeof(captured)-used-1){errno=EIO;return -1;}
    memcpy(captured+used,bytes,count);used+=count;captured[used]=0;
    return (ssize_t)count;
}
static void reset(void){used=0;captured[0]=0;fail=zero=interrupt_once=0;}
int main(void)
{
    CHECK(sys_nerr==58);
    reset();interrupt_once=1;errno=ENOENT;perror("open");
    CHECK(!strcmp(captured,"open: No such file or directory\n")&&errno==ENOENT);
    reset();errno=0;perror("");CHECK(!strcmp(captured,"Error 0\n")&&errno==0);
    reset();errno=ENOSYS;perror(NULL);CHECK(!strcmp(captured,"Function not implemented\n")&&errno==ENOSYS);
    int unknown[]={-1,58,INT32_MAX};
    for(unsigned i=0;i<sizeof(unknown)/sizeof(*unknown);++i){
        reset();errno=unknown[i];perror("bad");
        CHECK(!strcmp(captured,"bad: Unknown error\n")&&errno==unknown[i]);
    }
    reset();fail=EPIPE;errno=EACCES;perror("failed");CHECK(used==0&&errno==EACCES);
    reset();zero=1;errno=EIO;perror("zero");CHECK(used==0&&errno==EIO);
    reset();int count=sys_nerr;sys_nerr=1000;errno=999;perror("");
    CHECK(!strcmp(captured,"Unknown error\n")&&errno==999);sys_nerr=count;
    puts("PASS: V7 perror text, prefixes, unknown values, errno preservation, short/interrupted/failed/zero writes");
    return 0;
}
