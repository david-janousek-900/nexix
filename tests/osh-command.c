#include "command-fixture.h"
int main(void)
{
 command(CMD("/bin/osh","-c","echo OSH_OK"),0,"OSH_OK\n","");
 command(CMD("/bin/osh","-c","echo 'two words' | cat"),0,"two words\n","");
 command(CMD("/bin/osh","-c","echo first > /tmp/oshout; echo second >> /tmp/oshout; cat < /tmp/oshout"),0,"first\nsecond\n","");
 command(CMD("/bin/osh","-c","( echo SUB )"),0,"SUB\n","");
 command(CMD("/bin/osh","-c","chdir /tmp; pwd"),0,"/tmp\n","");
 putfile("/tmp/oshscript","echo $1\nshift\necho $1\n");command(CMD("/bin/osh","/tmp/oshscript","one","two"),0,"one\ntwo\n","");
 putfile("/tmp/oshtext","echo FALLBACK\n");CHECK(!chmod("/tmp/oshtext",0755));command(CMD("/bin/osh","-c","/tmp/oshtext"),0,"FALLBACK\n","");
 putfile("/tmp/g-a","");putfile("/tmp/g-b","");command(CMD("/bin/osh","-c","echo /tmp/g-[ab]"),0,"/tmp/g-a /tmp/g-b\n","");
 command(CMD("/bin/osh","-c","echo '/tmp/g-*'"),0,"/tmp/g-*\n","");
 command(CMD("/bin/osh","-c","echo /tmp/nonmatch*"),255,"","No match\n");
 command(CMD("/bin/osh","-c","missingcmd"),255,"","missingcmd: not found\n");
 command(CMD("/bin/osh","-c","echo >"),255,"","syntax error\n");
 command(CMD("/bin/osh","-c","cat < /absent"),255,"","/absent: cannot open\n");
 command(CMD("/bin/osh"),0,"","# ");
 command(CMD("/bin/osh","-c","/bin/sh -c 'exit 3'; echo CONTINUE"),3,"CONTINUE\n","");
 command(CMD("/bin/osh","-e","-c","/bin/sh -c 'exit 3'; echo UNREACHED"),3,"","\n");
 command(CMD("/bin/mkdir","/tmp/globdir"),0,"","");putfile("/tmp/globdir/item","");
 command(CMD("/bin/osh","-c","echo /tmp/glob*/it?m"),0,"/tmp/globdir/item\n","");
 pid_t p=fork();CHECK(p>=0);if(!p){CHECK(!setgid(100)&&!setuid(256));command(CMD("/bin/osh"),0,"","% ");_exit(0);}int status;CHECK(wait(&status)==p && !status);
 puts("OSH_OK");return 0;
}
