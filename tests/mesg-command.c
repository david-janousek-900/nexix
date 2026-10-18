#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
int main(void)
{
    int fd=open("/dev/ttyS1",O_RDWR);struct stat before,after;
    if(fd<0 || stat("/dev/ttyS1",&before))return 1;
    pid_t pid=fork();if(pid<0)return 2;
    if(!pid){
        if(dup2(fd,2)<0 || setgid(100) || setuid(100))_exit(3);
        execl("/bin/mesg","mesg","y",(char *)0);_exit(4);
    }
    int status;
    if(wait(&status)!=pid || !WIFEXITED(status) || WEXITSTATUS(status)!=255 ||
        stat("/dev/ttyS1",&after) || before.st_mode!=after.st_mode || before.st_uid!=after.st_uid || close(fd))return 5;
    puts("MESG_PERMISSIONS_OK");return 0;
}
