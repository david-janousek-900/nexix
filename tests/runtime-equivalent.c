#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#define CHECK(x) do { if (!(x)) { printf("runtime failure line %d\n",__LINE__); return 1; } } while (0)
__attribute__((noinline)) static long recurse(long n,long value)
{
    volatile long saved=value;
    return n ? recurse(n-1,value+3)+saved : saved;
}
__attribute__((noinline)) static long sum(int count,...)
{
    va_list args;va_start(args,count);long result=0;
    while(count--)result+=va_arg(args,long);
    va_end(args);return result;
}
int main(void)
{
    volatile long a=123456789,b=12345,negative=-123456789;
    CHECK(a*b==1524074060205L && a/b==10000 && a%b==6789);
    CHECK(negative/b==-10000 && negative%b==-6789);
    long x=a;x*=b;CHECK(x==1524074060205L);x/=b;CHECK(x==a);
    x=negative;x%=b;CHECK(x==-6789);
    CHECK(recurse(8,1)==117);
    CHECK(sum(9,1L,2L,3L,4L,5L,6L,7L,8L,9L)==45);
    struct timeb now;CHECK(ftime(&now)==0 && now.time>0 && now.millitm<1000);
    int fd=creat("/tmp/runtime-data",0600);CHECK(fd>=0);
    CHECK(write(fd,"native",6)==6 && close(fd)==0);
    struct stat st;CHECK(stat("/tmp/runtime-data",&st)==0 && st.st_size==6);
    CHECK(access("/tmp/runtime-data",0)==0);
    CHECK(chown("/tmp/runtime-data",0,0)==0);
    CHECK(unlink("/tmp/runtime-data")==0);
    errno=0;CHECK(access("/tmp/runtime-data",0)==-1 && errno==ENOENT);
    puts("RUNTIME_EQUIVALENT_OK");return 0;
}
