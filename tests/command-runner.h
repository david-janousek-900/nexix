/* Run an installed command with selected terminal descriptors and captured
 * stderr. The caller checks exact status/diagnostics and resulting state. */
#include <sys/wait.h>
static int run_command(char *const argv[],int input,int output,char *error,size_t capacity)
{
    int pair[2],status;size_t used=0;char byte;
    if(pipe(pair))return -1;
    pid_t pid=fork();
    if(pid<0){close(pair[0]);close(pair[1]);return -1;}
    if(!pid){
        if(dup2(input,0)<0 || dup2(output,1)<0 || dup2(pair[1],2)<0)_exit(126);
        for(int fd=3;fd<20;++fd)close(fd);
        execv(argv[0],argv);_exit(127);
    }
    close(pair[1]);ssize_t n;
    while((n=read(pair[0],&byte,1))>0){if(used+1<capacity)error[used++]=byte;}
    close(pair[0]);error[used]=0;
    if(wait(&status)!=pid || n<0 || !WIFEXITED(status))return -1;
    return WEXITSTATUS(status);
}
