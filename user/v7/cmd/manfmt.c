/* Run a manual formatter and an optional pager, preserving both exit statuses. */
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int status(int value){return WIFEXITED(value)?WEXITSTATUS(value):128+WTERMSIG(value);}
int main(int argc,char **argv)
{
    int pipefd[2],s,result=0,formatter_status=0,pager_status=0;pid_t formatter,pager,got;char *command,*copy,*args[32];unsigned n=0;
    if(argc<2){fputs("usage: manfmt formatter [arguments]\n",stderr);return 2;}
    if(!isatty(1)){execvp(argv[1],argv+1);perror("man: formatter");return 127;}
    command=getenv("MANPAGER");if(!command || !*command)command=getenv("PAGER");if(!command || !*command)command="/bin/more";
    copy=malloc(strlen(command)+1);if(!copy){perror("man");return 1;}strcpy(copy,command);
    char *p=copy;
    while(*p){while(*p==' '||*p=='\t')p++;if(!*p)break;
        if(n>=31){fputs("man: too many pager arguments\n",stderr);free(copy);return 2;}
        args[n++]=p;while(*p && *p!=' ' && *p!='\t')p++;if(*p)*p++=0;
    }
    args[n]=NULL;if(!n){fputs("man: empty pager command\n",stderr);free(copy);return 2;}
    if(pipe(pipefd)){perror("man: pipe");free(copy);return 1;}
    formatter=fork();
    if(formatter==0){close(pipefd[0]);if(dup2(pipefd[1],1)<0)_exit(126);close(pipefd[1]);execvp(argv[1],argv+1);perror("man: formatter");_exit(127);}
    if(formatter<0){perror("man: fork");close(pipefd[0]);close(pipefd[1]);free(copy);return 1;}
    pager=fork();
    if(pager==0){close(pipefd[1]);if(dup2(pipefd[0],0)<0)_exit(126);close(pipefd[0]);execvp(args[0],args);perror("man: pager");_exit(127);}
    close(pipefd[0]);close(pipefd[1]);
    if(pager<0){perror("man: fork");result=1;}
    int remaining=pager<0?1:2;
    while(remaining){got=wait(&s);if(got<0){if(errno==EINTR)continue;result=1;break;}
        if(got==formatter || got==pager){remaining--;if(got==formatter)formatter_status=status(s);else pager_status=status(s);}
    }
    free(copy);return result?result:pager_status?pager_status:formatter_status;
}
