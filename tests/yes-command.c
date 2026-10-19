#include "command-fixture.h"
#include <signal.h>
static void stream(char *const argv[],const char *expected)
{
 int pair[2];CHECK(!pipe(pair));pid_t child=fork();CHECK(child>=0);
 if(!child){signal(SIGPIPE,SIG_DFL);CHECK(dup2(pair[1],1)==1);close(pair[0]);close(pair[1]);execv(argv[0],argv);_exit(127);}
 close(pair[1]);size_t size=strlen(expected);char buffer[1024];size_t used=0;
 while(used<size){ssize_t n=read(pair[0],buffer+used,size-used);CHECK(n>0);used+=n;}
 CHECK(!memcmp(buffer,expected,size));CHECK(!close(pair[0]));int status;CHECK(wait(&status)==child && WIFSIGNALED(status) && WTERMSIG(status)==SIGPIPE);
}
int main(void)
{
 stream(CMD("/bin/yes"),"y\ny\ny\n");stream(CMD("/bin/yes","word"),"word\nword\nword\n");
 stream(CMD("/bin/yes","first","ignored"),"first\nfirst\n");stream(CMD("/bin/yes",""),"\n\n\n");
 puts("YES_OK");return 0;
}
