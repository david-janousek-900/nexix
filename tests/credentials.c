#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
int main(void)
{
    struct stat a,b;const char *home=getenv("HOME");
    if(stat(".",&a) || stat("/",&b))return 1;
    printf("UID=%u EUID=%u GID=%u EGID=%u HOME=%s CWD_ROOT=%d\n",
        (unsigned)getuid(),(unsigned)geteuid(),(unsigned)getgid(),(unsigned)getegid(),
        home?home:"unset",a.st_dev==b.st_dev && a.st_ino==b.st_ino);return 0;
}
