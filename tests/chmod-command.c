#include "command-fixture.h"
int main(void)
{
 putfile("/tmp/mode","data");
 command(CMD("/bin/chmod","0740","/tmp/mode"),0,"","");CHECK((metadata("/tmp/mode").st_mode&07777)==0740);
 command(CMD("/bin/chmod","g=u,o+r","/tmp/mode"),0,"","");CHECK((metadata("/tmp/mode").st_mode&07777)==0774);
 command(CMD("/bin/chmod","u+s,g-w,o=","/tmp/mode"),0,"","");CHECK((metadata("/tmp/mode").st_mode&07777)==04750);
 command(CMD("/bin/chmod","bogus","/tmp/mode"),255,"","chmod: invalid mode\n");
 command(CMD("/bin/chmod"),255,"","Usage: chmod [ugoa][+-=][rwxstugo] file ...\n");
 command(CMD("/bin/chmod","600","/missing","/tmp/mode"),1,"","chmod: can't access /missing\n");CHECK((metadata("/tmp/mode").st_mode&07777)==0600);
 pid_t p=fork();CHECK(p>=0);if(!p){CHECK(!setgid(100) && !setuid(100));command(CMD("/bin/chmod","777","/tmp/mode"),1,"","chmod: can't change /tmp/mode\n");_exit(0);}int status;CHECK(wait(&status)==p && !status);
 CHECK(!chown("/tmp/mode",100,100));p=fork();CHECK(p>=0);if(!p){CHECK(!setgid(100) && !setuid(100));command(CMD("/bin/chmod","640","/tmp/mode"),0,"","");_exit(0);}CHECK(wait(&status)==p && !status);CHECK((metadata("/tmp/mode").st_mode&07777)==0640);
 puts("CHMOD_OK");return 0;
}
