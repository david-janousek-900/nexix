#include "command-fixture.h"
int main(void)
{
 command(CMD("/bin/test"),1,"","");command(CMD("/bin/test",""),1,"","");command(CMD("/bin/test","a"),0,"","");
 command(CMD("/bin/test","-n",""),1,"","");command(CMD("/bin/test","-z",""),0,"","");
 command(CMD("/bin/test","abc","=","abc"),0,"","");command(CMD("/bin/test","abc","!=","abc"),1,"","");
 command(CMD("/bin/test","-2","-lt","1"),0,"","");command(CMD("/bin/test","-l","abc","-eq","3"),0,"","");
 command(CMD("/bin/test","1","-eq","1","-a","2","-gt","1"),0,"","");
 command(CMD("/bin/test","1","=","0","-o","2","=","2"),0,"","");
 command(CMD("/bin/test","!","(","a","=","b",")"),0,"","");
 putfile("/tmp/tfile","text");putfile("/tmp/tempty","");
 command(CMD("/bin/test","-f","/tmp/tfile"),0,"","");command(CMD("/bin/test","-s","/tmp/tfile"),0,"","");command(CMD("/bin/test","-s","/tmp/tempty"),1,"","");
 command(CMD("/bin/test","-d","/tmp"),0,"","");command(CMD("/bin/test","-f","/missing"),1,"","");command(CMD("/bin/test","-f","/dev/null"),0,"","");
 command(CMD("/bin/test","-t"),1,"","");command(CMD("/bin/test","-t","0"),1,"","");
 command(CMD("/bin/[","a","=","a","]"),0,"","");command(CMD("/bin/[","a"),255,"","test: ] missing\n");
 command(CMD("/bin/test","-n"),255,"","test: argument expected\n");command(CMD("/bin/test","-n","a","extra"),255,"","test: too many arguments\n");
 CHECK(!chmod("/tmp/tfile",0600));pid_t p=fork();CHECK(p>=0);if(!p){CHECK(!setgid(100)&&!setuid(100));command(CMD("/bin/test","-r","/tmp/tfile"),1,"","");command(CMD("/bin/test","-w","/tmp/tfile"),1,"","");_exit(0);}int status;CHECK(wait(&status)==p && !status);
 puts("TEST_OK");return 0;
}
