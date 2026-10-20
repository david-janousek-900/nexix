#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include "../libc/include/termcap.h"
static char bytes[4096];static unsigned count;static int fail;
static int capture(int c){if(fail)return -1;assert(count<sizeof(bytes));bytes[count++]=(char)c;return c;}
int main(void)
{
 assert(!strcmp(tgoto("\033[%i%d;%dH",79,24),"\033[25;80H"));
 assert(!strcmp(tgoto("%r%3,%2",7,4),"007,04"));
 assert(!strcmp(tgoto("%n%d;%d",0,0),"96;96"));
 assert(!strcmp(tgoto("%B%d",0,12),"18"));
 assert(!strcmp(tgoto("%D%d",0,18),"14"));
 assert(!strcmp(tgoto("%>AZ%d",0,80),"170"));
 assert(!strcmp(tgoto("%%",0,0),"%"));
 UP="U";BC="B";assert(!strcmp(tgoto("%.%.",0,0),"\1\1UB"));
 assert(!strcmp(tgoto("%+ ",0,1),"!"));
 assert(!strcmp(tgoto("%>",0,0),"OOPS") && errno==EINVAL);
 char huge[300];memset(huge,'x',299);huge[299]=0;
 assert(!strcmp(tgoto(huge,0,0),"OOPS") && errno==E2BIG);
 assert(!strcmp(tgoto("%i%d",0,INT_MAX),"OOPS") && errno==ERANGE);
 ospeed=13;PC='.';assert(tputs("10.0*ABC",2,capture)==1 && count==23 && !memcmp(bytes,"ABC",3));
 for(unsigned i=3;i<count;++i)assert(bytes[i]=='.');
 count=0;ospeed=0;assert(tputs("\033[2J",1,capture)==1 && count==4);
 fail=1;assert(tputs("x",1,capture)==-1);fail=0;
 count=0;ospeed=1;assert(tputs("99999999999999999*X",INT_MAX,capture)==-1 && count==0);
 assert(tputs(0,1,capture)==1);puts("PASS: cursor expansions/corrections, bounded errors, padding and callback failure");
}
