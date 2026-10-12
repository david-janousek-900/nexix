/* Native adapter for V7 stdio/system.c; wait-any semantics are intentional. */
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

int system(const char *command)
{
    if(!command){errno=EINVAL;return -1;}
    pid_t child=fork();
    if(child<0)return -1;
    if(!child){
        execl("/bin/sh","sh","-c",command,(char *)NULL);
        _exit(127);
    }
    sighandler_t interrupt=signal(SIGINT,SIG_IGN);
    sighandler_t quit=signal(SIGQUIT,SIG_IGN);
    int status=-1;pid_t collected;
    while((collected=wait(&status))!=child&&collected!=-1){}
    int error=errno;
    if(collected==-1)status=-1;
    (void)signal(SIGINT,interrupt);
    (void)signal(SIGQUIT,quit);
    if(collected==-1)errno=error;
    return status;
}
