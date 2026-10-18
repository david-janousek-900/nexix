#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"launch")){
        int fd=creat("/tmp/newgrp-fd",0600);
        if(fd<0 || dup2(fd,19)!=19 || close(fd))return 2;
        execl("/bin/newgrp","newgrp","staff",(char *)0);return 3;
    }
    if(argc==2 && !strcmp(argv[1],"closed") && (fcntl(19,F_GETFD)!=-1 || errno!=EBADF))return 4;
    struct stat a,b;const char *home=getenv("HOME");
    if(stat(".",&a) || stat("/",&b))return 1;
    printf("UID=%u EUID=%u GID=%u EGID=%u HOME=%s CWD_ROOT=%d\n",
        (unsigned)getuid(),(unsigned)geteuid(),(unsigned)getgid(),(unsigned)getegid(),
        home?home:"unset",a.st_dev==b.st_dev && a.st_ino==b.st_ino);return 0;
}
