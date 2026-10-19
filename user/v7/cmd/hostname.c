/* Native persistent hostname management; no DNS/network lookup is needed. */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "../hostname-file.h"
static int usage(void)
{fputs("usage: hostname [-f|-s|-d] | hostname name | hostname -F file\n",stderr);return 2;}
static int persist(const char *name)
{
    char line[NX_UTS_FIELD_SIZE+1];
    size_t length=strlen(name),done=0;
    int fd=open("/etc/hostname",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0)return -1;
    memcpy(line,name,length);line[length++]='\n';
    while(done<length) {
        ssize_t n=write(fd,line+done,length-done);
        if(n<0 && errno==EINTR)continue;
        if(n<=0){int error=n<0?errno:EIO;close(fd);errno=error;return -1;}
        done+=n;
    }
    return close(fd);
}
int main(int argc,char **argv)
{
    char name[NX_UTS_FIELD_SIZE],*dot;
    const char *value=NULL;
    int load=argc==3 && !strcmp(argv[1],"-F");
    if(load) {
        if(geteuid()!=0){errno=EPERM;perror("hostname");return 1;}
        if(read_hostname_file(argv[2],name)<0){perror("hostname");return 1;}
        value=name;
    } else if(argc==2 && argv[1][0]!='-') value=argv[1];
    else if(argc>2 || (argc==2 && strcmp(argv[1],"-f") && strcmp(argv[1],"-s") && strcmp(argv[1],"-d")))return usage();
    if(value) {
        if(geteuid()!=0){errno=EPERM;perror("hostname");return 1;}
        if(!nx_hostname_valid(value,strlen(value))){fputs("hostname: invalid hostname\n",stderr);return 1;}
        /* Publish kernel state only after persistent writes have succeeded.
         * -F is the explicit runtime-only loader for an existing file. */
        if(!load && persist(value)<0){perror("hostname: /etc/hostname");return 1;}
        if(sethostname(value,strlen(value))<0){perror("hostname");return 1;}
        return 0;
    }
    if(gethostname(name,sizeof name)<0){perror("hostname");return 1;}
    dot=strchr(name,'.');
    if(argc==2 && !strcmp(argv[1],"-s") && dot)*dot=0;
    value=argc==2 && !strcmp(argv[1],"-d") ? (dot?dot+1:"") : name;
    puts(value);return fflush(stdout)==EOF || ferror(stdout) ? 1 : 0;
}
