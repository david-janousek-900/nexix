#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
int execl(const char *path,const char *first,...)
{
    char *argv[65];size_t n=0;const char *arg=first;
    va_list ap;va_start(ap,first);
    while(arg){if(n==64){va_end(ap);errno=E2BIG;return -1;}argv[n++]=(char *)arg;arg=va_arg(ap,const char *);}
    va_end(ap);argv[n]=NULL;return execv(path,argv);
}
/* V7 execvp.c's variadic PATH-search entry point, with native va_list. */
int execlp(const char *path,const char *first,...)
{
    char *argv[65];size_t n=0;const char *arg=first;
    va_list ap;va_start(ap,first);
    while(arg){if(n==64){va_end(ap);errno=E2BIG;return -1;}argv[n++]=(char *)arg;arg=va_arg(ap,const char *);}
    va_end(ap);argv[n]=NULL;return execvp(path,argv);
}
int execle(const char *path, const char *first, ...)
{
    char *argv[65];
    size_t n = 0;
    const char *arg = first;
    va_list ap;
    va_start(ap, first);
    while (arg) {
        if (n == 64) { va_end(ap); errno = E2BIG; return -1; }
        argv[n++] = (char *)arg;
        arg = va_arg(ap, const char *);
    }
    char **environment = va_arg(ap, char **);
    va_end(ap);
    argv[n] = NULL;
    return execve(path, argv, environment);
}
int execvp(const char *name,char *const argv[])
{
    if(strchr(name,'/'))return execv(name,argv);
    const char *path=getenv("PATH");if(!path)path="/bin:/usr/bin";
    int denied=0;
    do{char candidate[256];size_t n=0;while(*path&&*path!=':'){if(n<sizeof(candidate)-1)candidate[n++]=*path;++path;}
        if(n){if(n==sizeof(candidate)-1){errno=E2BIG;return -1;}candidate[n++]='/';}
        size_t length=strlen(name);if(length>=sizeof(candidate)-n){errno=E2BIG;return -1;}
        memcpy(candidate+n,name,length+1);execv(candidate,argv);
        if(errno==EACCES)denied=1;else if(errno!=ENOENT)return -1;
    }while(*path++);
    errno=denied?EACCES:ENOENT;return -1;
}
