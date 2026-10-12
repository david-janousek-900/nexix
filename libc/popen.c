/* Native adaptation of V7 stdio/popen.c. */
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

struct command_stream { FILE *stream; int fd; pid_t child; int collected,status; };
static struct command_stream commands[20];
FILE *popen(const char *command,const char *mode)
{
    if(!command||!mode||(strcmp(mode,"r")&&strcmp(mode,"w"))){errno=EINVAL;return NULL;}
    unsigned slot;
    for(slot=0;slot<20&&commands[slot].stream;++slot){}
    if(slot==20){errno=EMFILE;return NULL;}
    int pipefd[2];if(pipe(pipefd)<0)return NULL;
    int reading=*mode=='r',parent=pipefd[reading?0:1],child=pipefd[reading?1:0];
    /* Allocate the FILE before forking: failure cannot strand a child. */
    FILE *stream=fdopen(parent,mode);
    if(!stream){int error=errno;(void)close(parent);(void)close(child);errno=error;return NULL;}
    pid_t pid=fork();
    if(pid<0){int error=errno;(void)fclose(stream);(void)close(child);errno=error;return NULL;}
    if(!pid){
        (void)close(parent);
        /* Other command streams must not keep a writer alive and suppress EOF. */
        for(unsigned i=0;i<20;++i)if(commands[i].stream)(void)close(commands[i].fd);
        int target=reading?STDOUT_FILENO:STDIN_FILENO;
        if(dup2(child,target)<0)_exit(1);
        if(child!=target)(void)close(child);
        execl("/bin/sh","sh","-c",command,(char *)NULL);
        _exit(1);
    }
    (void)close(child);
    commands[slot]=(struct command_stream){.stream=stream,.fd=parent,.child=pid};
    return stream;
}
int pclose(FILE *stream)
{
    unsigned slot;
    for(slot=0;slot<20;++slot)if(commands[slot].stream==stream&&stream)break;
    if(slot==20){errno=EINVAL;return -1;}
    struct command_stream command=commands[slot];
    commands[slot]=(struct command_stream){0};
    int closing=fclose(stream),close_error=errno;
    sighandler_t interrupt=signal(SIGINT,SIG_IGN),quit=signal(SIGQUIT,SIG_IGN),hangup=signal(SIGHUP,SIG_IGN);
    int status=command.status,error=0;
    if(!command.collected){
        pid_t collected;
        while((collected=wait(&status))!=command.child){
            if(collected<0){error=errno;status=-1;break;}
            for(unsigned i=0;i<20;++i)if(commands[i].stream&&commands[i].child==collected){
                commands[i].collected=1;commands[i].status=status;break;
            }
        }
    }
    (void)signal(SIGINT,interrupt);(void)signal(SIGQUIT,quit);(void)signal(SIGHUP,hangup);
    if(error)errno=error;else if(closing==EOF)errno=close_error;
    /* V7 returns child status even if fclose's output flush failed. */
    return status;
}
