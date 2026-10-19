#include "command-fixture.h"
int main(void)
{
 putfile("/tmp/owned","data");CHECK(!chown("/tmp/owned",0,1));
 command(CMD("/bin/chown","alice","/tmp/owned"),0,"","");struct stat st=metadata("/tmp/owned");CHECK(st.st_uid==100 && st.st_gid==1);
 command(CMD("/bin/chown","101","/tmp/owned"),0,"","");CHECK(metadata("/tmp/owned").st_uid==101);
 command(CMD("/bin/chown"),4,"usage: chown uid file ...\n","");
 command(CMD("/bin/chown","missing","/tmp/owned"),4,"unknown user id: missing\n","");
 command(CMD("/bin/chown","65536","/tmp/owned"),4,"invalid user id\n","");
 command(CMD("/bin/chown","0","/missing","/tmp/owned"),1,"","/missing: No such file or directory\n");CHECK(metadata("/tmp/owned").st_uid==0);
 pid_t p=fork();CHECK(p>=0);if(!p){CHECK(!setgid(100) && !setuid(100));command(CMD("/bin/chown","100","/tmp/owned"),1,"","/tmp/owned: Not owner\n");_exit(0);}int status;CHECK(wait(&status)==p && !status);
 puts("CHOWN_OK");return 0;
}
