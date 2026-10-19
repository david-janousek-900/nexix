#include "command-fixture.h"
int main(void)
{
 command(CMD("/bin/expr","2","+","3","*","4"),0,"14\n","");
 command(CMD("/bin/expr","(","2","+","3",")","*","4"),0,"20\n","");
 command(CMD("/bin/expr","-5","+","2"),0,"-3\n","");
 command(CMD("/bin/expr","0","|","yes"),0,"yes\n","");
 command(CMD("/bin/expr","yes","&","0"),1,"0\n","");
 command(CMD("/bin/expr","2","<=","3"),0,"1\n","");
 command(CMD("/bin/expr","z",">","a"),0,"1\n","");
 command(CMD("/bin/expr","abc123",":","[a-z]*"),0,"3\n","");
 command(CMD("/bin/expr","abc123",":","[a-z]*\\([0-9]*\\)"),0,"123\n","");
 command(CMD("/bin/expr","xx",":","\\(a\\)"),1,"\n","");
 command(CMD("/bin/expr","substr","abcdef","2","3"),0,"bcd\n","");
 command(CMD("/bin/expr","length","abcdef"),0,"6\n","");
 command(CMD("/bin/expr","index","abcdef","xd"),0,"4\n","");
 command(CMD("/bin/expr","1","/","0"),2,"","division by zero\n");
 command(CMD("/bin/expr","x","+","2"),2,"","non-numeric argument\n");
 command(CMD("/bin/expr","1","+"),2,"","syntax error\n");
 command(CMD("/bin/expr","abc",":","["),2,"","RE error\n");
 command(CMD("/bin/expr","9223372036854775807","+","1"),2,"","arithmetic overflow\n");
 command(CMD("/bin/expr","abc",":","\\("),2,"","RE error\n");
 char pattern[257];memset(pattern,'.',255);pattern[255]='a';pattern[256]=0;
 command(CMD("/bin/expr","abc",":",pattern),2,"","RE error\n");
 char subject[201];memset(subject,'a',200);subject[200]=0;
 command(CMD("/bin/expr",subject,":","\\(a*\\)"),2,"","RE error\n");
 puts("EXPR_OK");return 0;
}
