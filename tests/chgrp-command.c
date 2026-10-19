#include "command-fixture.h"
int main(void)
{
 putfile("/tmp/owned","data");CHECK(!chown("/tmp/owned",101,0));
 command(CMD("/bin/chgrp","staff","/tmp/owned"),0,"","");struct stat st=metadata("/tmp/owned");CHECK(st.st_gid==100 && st.st_uid==101);
 command(CMD("/bin/chgrp","1","/tmp/owned"),0,"","");CHECK(metadata("/tmp/owned").st_gid==1);
 command(CMD("/bin/chgrp"),4,"usage: chgrp gid file ...\n","");
 command(CMD("/bin/chgrp","missing","/tmp/owned"),4,"unknown group: missing\n","");
 command(CMD("/bin/chgrp","65536","/tmp/owned"),4,"invalid group id\n","");
 command(CMD("/bin/chgrp","0","/missing","/tmp/owned"),1,"","/missing: No such file or directory\n");CHECK(metadata("/tmp/owned").st_gid==0);
 pid_t p=fork();CHECK(p>=0);if(!p){CHECK(!setgid(100) && !setuid(100));command(CMD("/bin/chgrp","1","/tmp/owned"),1,"","/tmp/owned: Not owner\n");_exit(0);}int status;CHECK(wait(&status)==p && !status);
 puts("CHGRP_OK");return 0;
}
