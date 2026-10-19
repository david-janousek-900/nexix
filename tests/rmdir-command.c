#include "command-fixture.h"
int main(void)
{
 command(CMD("/bin/rmdir"),1,"","rmdir: arg count\n");
 command(CMD("/bin/mkdir","/tmp/remove"),0,"","");unsigned links=metadata("/tmp").st_nlink;
 command(CMD("/bin/rmdir","/tmp/remove"),0,"","");CHECK(access("/tmp/remove",F_OK)<0 && metadata("/tmp").st_nlink==links-1);
 command(CMD("/bin/rmdir","/missing"),1,"","rmdir: /missing non-existent\n");putfile("/tmp/file","x");command(CMD("/bin/rmdir","/tmp/file"),1,"","rmdir: /tmp/file not a directory\n");
 command(CMD("/bin/mkdir","/tmp/nonempty"),0,"","");putfile("/tmp/nonempty/f","x");command(CMD("/bin/rmdir","/tmp/nonempty"),1,"","rmdir: /tmp/nonempty not empty\n");contents("/tmp/nonempty/f","x");
 command(CMD("/bin/mkdir","/tmp/cwd"),0,"","");CHECK(!chdir("/tmp/cwd"));command(CMD("/bin/rmdir","."),1,"","rmdir: cannot remove current directory\n");CHECK(!chdir("/"));
 pid_t p=fork();CHECK(p>=0);if(!p){CHECK(!setgid(100)&&!setuid(100));command(CMD("/bin/mkdir","/tmp/urem"),0,"","");command(CMD("/bin/rmdir","/tmp/urem"),0,"","");command(CMD("/bin/rmdir","/tmp/cwd"),0,"","");_exit(0);}int status;CHECK(wait(&status)==p && !status);
 puts("RMDIR_OK");return 0;
}
