#include "command-fixture.h"
#include <signal.h>
int main(void)
{
 command(CMD("/bin/kill"),2,"usage: kill [ -signo ] pid ...\n","");
 command(CMD("/bin/kill","-9"),2,"usage: kill [ -signo ] pid ...\n","");
 command(CMD("/bin/kill","12bad"),2,"usage: kill [ -signo ] pid ...\n","");
 command(CMD("/bin/kill","2147483647"),1,"2147483647: No such process\n","");
 int ready[2],done[2];CHECK(!pipe(ready)&&!pipe(done));pid_t supervisor=fork();CHECK(supervisor>=0);
 if(!supervisor){close(ready[0]);close(done[1]);pid_t victim=fork();CHECK(victim>=0);if(!victim){signal(SIGTERM,SIG_DFL);for(;;)pause();}CHECK(write(ready[1],&victim,sizeof(victim))==sizeof(victim));char c;CHECK(read(done[0],&c,1)==1);int status;CHECK(wait(&status)==victim && WIFSIGNALED(status) && WTERMSIG(status)==SIGTERM);_exit(0);}
 close(ready[1]);close(done[0]);pid_t victim;CHECK(read(ready[0],&victim,sizeof(victim))==sizeof(victim));char pid[16],error[80];sprintf(pid,"%u",victim);
 command(CMD("/bin/kill","-0",pid),0,"","");
 sprintf(error,"%u: Invalid argument\n",victim);command(CMD("/bin/kill","-999",pid),1,error,"");
 pid_t user=fork();CHECK(user>=0);if(!user){CHECK(!setgid(100)&&!setuid(100));sprintf(error,"%u: Not owner\n",victim);command(CMD("/bin/kill","-0",pid),1,error,"");_exit(0);}int status;CHECK(wait(&status)==user && !status);
 command(CMD("/bin/kill",pid),0,"","");CHECK(write(done[1],"x",1)==1);CHECK(wait(&status)==supervisor && !status);
 puts("KILL_OK");return 0;
}
