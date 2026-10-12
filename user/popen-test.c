#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
static void handler(int n){(void)n;}
int popen_test(void)
{
    char text[64];FILE *f;
    CHECK(popen(NULL,"r")==NULL&&errno==EINVAL);
    CHECK(popen("exit 0",NULL)==NULL&&errno==EINVAL);
    CHECK(popen("exit 0","r+")==NULL&&errno==EINVAL);
    CHECK(pclose(NULL)==-1&&errno==EINVAL);
    CHECK(pclose(stdout)==-1&&errno==EINVAL&&fileno(stdout)==1);
    f=popen("/bin/echo 'two words'; exit 7","r");CHECK(f);
    CHECK(putc('x',f)==EOF&&errno==EBADF);
    CHECK(fgets(text,sizeof(text),f)&&!strcmp(text,"two words\n"));
    CHECK(getc(f)==EOF&&feof(f));
    CHECK(signal(SIGINT,handler)==SIG_DFL&&signal(SIGQUIT,handler)==SIG_DFL&&signal(SIGHUP,handler)==SIG_DFL);
    CHECK(pclose(f)==(7<<8));
    CHECK(signal(SIGINT,SIG_DFL)==handler&&signal(SIGQUIT,SIG_DFL)==handler&&signal(SIGHUP,SIG_DFL)==handler);
    CHECK(pclose(f)==-1&&errno==EINVAL);
    f=popen("/bin/cat > /tmp/task49","w");CHECK(f);
    CHECK(fputs("buffered output\n",f)==0&&pclose(f)==0);
    f=fopen("/tmp/task49","r");CHECK(f);
    CHECK(fgets(text,sizeof(text),f)&&!strcmp(text,"buffered output\n")&&fclose(f)==0);
    CHECK(unlink("/tmp/task49")==0);
    /* Closing second first can reap the first child's status on the way. */
    FILE *first=popen("exit 3","r");CHECK(first);
    FILE *second=popen("exit 9","r");CHECK(second);
    CHECK(pclose(second)==(9<<8)&&pclose(first)==(3<<8));
    /* The later child must not retain the earlier output pipe's writer. */
    first=popen("/bin/cat > /tmp/task49","w");CHECK(first);
    second=popen("/bin/cat","w");CHECK(second);
    CHECK(fputs("EOF\n",first)==0&&pclose(first)==0&&pclose(second)==0);
    CHECK(unlink("/tmp/task49")==0);
    f=popen("exec /bin/v7-test system-signal-exit","r");CHECK(f&&pclose(f)==SIGTERM);
    /* An external wait can consume a popen child, as in V7. pclose must
     * still flush/close its pipe and restore signals on the ECHILD path. */
    f=popen("exit 0","w");CHECK(f);
    int reaped_status;CHECK(wait(&reaped_status)>0&&reaped_status==0);
    CHECK(signal(SIGPIPE,SIG_IGN)==SIG_DFL);
    CHECK(fputs("queued",f)==0&&pclose(f)==-1&&errno==ECHILD);
    CHECK(signal(SIGPIPE,SIG_DFL)==SIG_IGN);
    for(int i=0;i<12;++i){f=popen("exit 0","r");CHECK(f&&pclose(f)==0);}
    /* Fill the descriptor table: rejection must preserve the existing handles. */
    int fds[20],count=0;
    while(count<20){int fd=open("/hello",O_RDONLY);if(fd<0)break;fds[count++]=fd;}
    CHECK(popen("exit 0","r")==NULL&&errno==EMFILE);
    for(int i=0;i<count;++i)CHECK(close(fds[i])==0);
    /* Closed stdio descriptors expose pipe endpoints equal to the dup target. */
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        if(close(0)||close(1))_exit(1);
        FILE *input=popen("/bin/echo collision","r");
        if(!input||!fgets(text,sizeof(text),input)||strcmp(text,"collision\n")||pclose(input))_exit(2);
        FILE *output=popen("/bin/cat > /tmp/task49-low","w");
        if(!output||fputs("low\n",output)||pclose(output))_exit(3);
        _exit(0);
    }
    int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    f=fopen("/tmp/task49-low","r");CHECK(f);
    CHECK(fgets(text,sizeof(text),f)&&!strcmp(text,"low\n")&&fclose(f)==0&&unlink("/tmp/task49-low")==0);
    CHECK(wait(&status)==-1&&errno==ECHILD);
    CHECK(printf("NEXIX_POPEN_OK\n")>0);
    return 0;
}
