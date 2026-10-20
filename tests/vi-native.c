#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
extern sighandler_t nx_vi_signal(int,sighandler_t);
extern char *nx_vi_strerror(int);
extern int nx_vi_same_directory(const char *);
static char output[128];static unsigned used;
int nx_vi_putchar(int c){if(used<sizeof(output)-1)output[used++]=(char)c;return c;}
extern int nx_vi_printf(const char *,...);
static volatile int calls;
static void caught(int n){if(n==SIGINT)++calls;}
int main(void)
{
 if(nx_vi_signal(SIGINT,caught)==SIG_ERR)return 1;
 if(kill(getpid(),SIGINT) || kill(getpid(),SIGINT) || calls!=2)return 2;
 if(nx_vi_signal(SIGINT,SIG_IGN)!=caught || nx_vi_signal(SIGINT,caught)!=SIG_IGN)return 3;
 if(kill(getpid(),SIGINT) || calls!=3)return 4;
 if(nx_vi_signal(SIGKILL,caught)!=SIG_ERR || errno!=EINVAL)return 5;
 if(chdir("/tmp") || !nx_vi_same_directory("/tmp") || nx_vi_same_directory("/") || nx_vi_same_directory("/absent"))return 6;
 if(strcmp(nx_vi_strerror(ENOENT),sys_errlist[ENOENT]) || strcmp(nx_vi_strerror(-1),"Unknown error"))return 7;
 nx_vi_printf("%ld %lx",9223372036854775807L,0x123456789abcdef0UL);
 if(strcmp(output,"9223372036854775807 0x123456789abcdef0"))return 8;
 used=0;memset(output,0,sizeof(output));
 nx_vi_printf("%ld %lu",(-9223372036854775807L-1L),18446744073709551615UL);
 if(strcmp(output,"-9223372036854775808 18446744073709551615")){puts(output);return 9;}
 puts("VI_NATIVE_OK");return 0;
}
