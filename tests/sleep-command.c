#include "command-fixture.h"
#include <sys/times.h>
int main(void)
{
 command(CMD("/bin/sleep"),0,"arg count\n","");command(CMD("/bin/sleep","bad"),0,"bad character\n","");command(CMD("/bin/sleep","4294967296"),1,"argument too large\n","");
 struct tms usage;clock_t start=times(&usage);command(CMD("/bin/sleep","0"),0,"","");CHECK(times(&usage)-start<CLK_TCK);
 start=times(&usage);command(CMD("/bin/sleep","1"),0,"","");clock_t elapsed=times(&usage)-start;CHECK(elapsed>=CLK_TCK && elapsed<5*CLK_TCK);
 command(CMD("/bin/sleep","0","ignored"),0,"","");puts("SLEEP_OK");return 0;
}
