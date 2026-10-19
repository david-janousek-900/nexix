#include "command-fixture.h"
#include <utime.h>
int main(void)
{
 command(CMD("/bin/mv"),1,"","usage: mv f1 f2; or mv d1 d2; or mv f1 ... fn d1\n");
 putfile("/tmp/src","contents");struct stat before=metadata("/tmp/src");
 command(CMD("/bin/mv","/tmp/src","/tmp/dst"),0,"","");CHECK(access("/tmp/src",F_OK)<0 && metadata("/tmp/dst").st_ino==before.st_ino);contents("/tmp/dst","contents");
 command(CMD("/bin/mv","/tmp/dst","/tmp/dst"),1,"","mv: /tmp/dst and /tmp/dst are identical\n");
 command(CMD("/bin/mkdir","/tmp/dir","/tmp/other","/tmp/mnt"),0,"","");
 command(CMD("/bin/mv","/tmp/dir","/tmp/other/dir"),0,"","");CHECK(metadata("/tmp/other/dir/..").st_ino==metadata("/tmp/other").st_ino);
 command(CMD("/bin/mv","/tmp/other","/tmp/other/dir/inner"),1,"","mv: cannot move a directory into itself\n");
 putfile("/tmp/a","a");putfile("/tmp/b","b");command(CMD("/bin/mv","/tmp/a","/tmp/b","/tmp/other"),0,"","");contents("/tmp/other/a","a");contents("/tmp/other/b","b");
 CHECK(!mount("/dev/hp0","/tmp/mnt",0));putfile("/tmp/cross","cross-device");struct utimbuf times={1000,2000};CHECK(!utime("/tmp/cross",&times));
 command(CMD("/bin/mv","/tmp/cross","/tmp/mnt/cross"),0,"","");CHECK(access("/tmp/cross",F_OK)<0);struct stat st=metadata("/tmp/mnt/cross");CHECK(st.st_mtime==2000 && st.st_atime==1000);contents("/tmp/mnt/cross","cross-device");
 command(CMD("/bin/mv","/tmp/other","/tmp/mnt/other"),1,"","mv: cannot move directories across devices\n");CHECK(!umount("/dev/hp0"));
 pid_t p=fork();CHECK(p>=0);if(!p){CHECK(!setgid(100)&&!setuid(100));command(CMD("/bin/mkdir","/tmp/udir"),0,"","");command(CMD("/bin/mv","/tmp/udir","/tmp/unew"),0,"","");_exit(0);}int status;CHECK(wait(&status)==p && !status);CHECK(metadata("/tmp/unew").st_uid==100);
 puts("MV_OK");return 0;
}
