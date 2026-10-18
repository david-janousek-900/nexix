#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
extern char **environ;
int main(void)
{
    const char *script="echo EXECVP_SCRIPT_$1\nexit 7\n";
    int fd=creat("/tmp/native-script",0755);
    if(fd<0 || write(fd,script,strlen(script))!=(int)strlen(script) || close(fd))return 1;
    for(int explicit=0;explicit<2;++explicit){
        int child=fork(),status;if(child<0)return 2;
        if(!child){
            char *environment[]={"PATH=/tmp:/bin",0};environ=environment;
            execlp(explicit?"/tmp/native-script":"native-script","ignored","argument",(char *)0);
            _exit(90);
        }
        if(wait(&status)!=child || !WIFEXITED(status) || WEXITSTATUS(status)!=7)return 3;
    }
    if(unlink("/tmp/native-script"))return 4;
    puts("EXEC_CONSUMER_OK");return 0;
}
