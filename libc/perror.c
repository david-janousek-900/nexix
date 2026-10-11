/* V7 gen/perror.c: diagnostics bypass FILE buffering and use descriptor 2. */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
static void diagnostic_write(const char *text,size_t length)
{
    while(length){
        ssize_t n=write(2,text,length);
        if(n<0){if(errno==EINTR)continue;return;}
        if(!n)return;
        text+=(size_t)n;length-=(size_t)n;
    }
}
void perror(const char *prefix)
{
    int saved=errno;
    const char *message=saved>=0 && saved<sys_nerr && saved<=ENOSYS
        ? sys_errlist[saved] : "Unknown error";
    if(prefix && *prefix){
        diagnostic_write(prefix,strlen(prefix));
        diagnostic_write(": ",2);
    }
    diagnostic_write(message,strlen(message));
    diagnostic_write("\n",1);
    errno=saved;
}
