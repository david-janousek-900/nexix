#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <nexix/hostname.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){printf("HOSTNAME_API_ERROR %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    char name[NX_UTS_FIELD_SIZE],small[4]={'A','B','C','D'};
    struct utsname value;
    struct stat config;
    CHECK(argc==2 && gethostname(name,sizeof name)==0 && !strcmp(name,argv[1]));
    CHECK(uname(&value)==0 && !strcmp(value.nodename,name));
    CHECK(gethostname(small,2)==-1 && errno==ERANGE && !memcmp(small,"ABCD",4));
    CHECK(gethostname(NULL,sizeof name)==-1 && errno==EFAULT);
    CHECK(stat("/etc/hostname",&config)==0 && config.st_uid==0 && (config.st_mode&0777)==0644);
    if(geteuid()) {
        CHECK(sethostname("denied.example",14)==-1 && errno==EPERM);
    } else {
        CHECK(sethostname(NULL,4)==-1 && errno==EFAULT);
        CHECK(sethostname("bad name",8)==-1 && errno==EINVAL);
        CHECK(sethostname(name,0)==-1 && errno==EINVAL);
        CHECK(sethostname(name,65)==-1 && errno==EINVAL);
        CHECK(sethostname(name,strlen(name))==0);
    }
    CHECK(gethostname(name,sizeof name)==0 && !strcmp(name,argv[1]));
    puts("HOSTNAME_API_OK");return 0;
}
