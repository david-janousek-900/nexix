#include "command-fixture.h"
int main(void)
{
 umask(022);
 command(CMD("/bin/mkdir"),1,"","mkdir: arg count\n");
 command(CMD("/bin/mkdir","/tmp/newdir"),0,"","");struct stat st=metadata("/tmp/newdir");CHECK(S_ISDIR(st.st_mode) && (st.st_mode&0777)==0755 && st.st_nlink==2);
 CHECK(metadata("/tmp/newdir/.").st_ino==st.st_ino && metadata("/tmp/newdir/..").st_ino==metadata("/tmp").st_ino);
 command(CMD("/bin/mkdir","/tmp/newdir","/tmp/second"),1,"","mkdir: cannot make directory /tmp/newdir\n");CHECK(S_ISDIR(metadata("/tmp/second").st_mode));
 command(CMD("/bin/mkdir","/missing/sub"),1,"","mkdir: cannot access /missing/.\n");
 pid_t p=fork();CHECK(p>=0);if(!p){CHECK(!setgid(100)&&!setuid(100));command(CMD("/bin/mkdir","/tmp/userdir"),0,"","");command(CMD("/bin/mkdir","/etc/denied"),1,"","mkdir: cannot access /etc/.\n");_exit(0);}int status;CHECK(wait(&status)==p && !status);
 st=metadata("/tmp/userdir");CHECK(st.st_uid==100 && st.st_gid==100 && S_ISDIR(st.st_mode));
 puts("MKDIR_OK");return 0;
}
