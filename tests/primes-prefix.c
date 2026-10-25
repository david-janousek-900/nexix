/* Private controller: read N generated lines, close the pipe, reap SIGPIPE. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
int main(int argc,char **argv)
{
    int p[2],pid,lines,c,status=0;
    FILE *input;
    if(argc!=3 || (lines=atoi(argv[2]))<=0 || pipe(p)<0)return 1;
    pid=fork();
    if(pid<0)return 1;
    if(!pid){
        close(p[0]);if(dup2(p[1],1)<0)_exit(1);close(p[1]);
        if(!strcmp(argv[1],"--stdin"))execl("/bin/primes","primes",(char *)0);
        else execl("/bin/primes","primes",argv[1],(char *)0);
        _exit(1);
    }
    close(p[1]);input=fdopen(p[0],"r");
    if(!input){close(p[0]);kill(pid,SIGTERM);wait(&status);return 1;}
    while(lines>0 && (c=getc(input))!=EOF){if(putchar(c)==EOF)break;if(c=='\n')--lines;}
    fclose(input);
    if(wait(&status)!=pid)return 1;
    return lines || ((status&127)!=SIGPIPE && status!=(1<<8)) ? 1 : 0;
}
