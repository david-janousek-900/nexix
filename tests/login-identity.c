#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
int main(int argc,char **argv)
{
    struct stat a,b,s;const char *home=getenv("HOME"),*path=getenv("PATH");
    if(argc!=1 || strcmp(argv[0],"-identity") || getuid()!=100 || geteuid()!=100 ||
        getgid()!=100 || getegid()!=100 || !home || strcmp(home,"/home/alice") ||
        !path || strcmp(path,":/bin:/usr/bin") || getenv("TZ") ||
        stat(".",&a) || stat(home,&b) || a.st_dev!=b.st_dev || a.st_ino!=b.st_ino ||
        stat("/dev/console",&s) || s.st_uid!=100 || s.st_gid!=100)return 1;
    int fd=creat("login-owned",0666);
    if(fd<0 || fstat(fd,&s) || (s.st_mode&0777)!=0664 || s.st_uid!=100 || s.st_gid!=100 || close(fd) || unlink("login-owned"))return 2;
    puts("LOGIN_IDENTITY_OK");return 0;
}
