#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mon.h>
static void cleanup(void){puts("DEVKIT_EXIT");}
__attribute__((noinline)) static double mixed(int first,...)
{
    va_list args,copy;va_start(args,first);va_copy(copy,args);
    double value=va_arg(copy,double)+va_arg(copy,long)+first;
    va_end(copy);va_end(args);return value;
}
int main(int argc,char **argv,char **environment)
{
    if(argc==2 && !strcmp(argv[1],"profile")){
        struct nx_mon_header header;struct stat metadata;
        int fd=open("mon.out",O_RDONLY);
        if(fd<0 || fstat(fd,&metadata) || read(fd,&header,sizeof(header))!=sizeof(header) || close(fd) ||
           memcmp(header.magic,NX_MON_MAGIC,8) || header.version!=NX_MON_VERSION ||
           header.machine!=NX_MON_MACHINE || !header.call_used || header.call_used>header.call_capacity ||
           metadata.st_size!=(long)(sizeof(header)+header.call_capacity*16+header.histogram_bytes))return 6;
        puts("DEVKIT_PROFILE_OK");return 0;
    }
    if(argc!=2 || strcmp(argv[1],"native") || !environment || !getenv("DEVKIT_TEST") ||
       strcmp(getenv("DEVKIT_TEST"),"ok") || sizeof(long)!=8 || UINTPTR_MAX!=ULONG_MAX)return 1;
    if(mixed(2,1.5,3L)!=6.5 || fabs(gamma(.5)-.572364942924700087)>1e-12)return 2;
    FILE *stream=fopen("/tmp/devkit-data","w+");if(!stream)return 3;
    char bytes[32];
    if(fprintf(stream,"%ld %.2f\n",123456789L,1.25)<0 || fseek(stream,0,SEEK_SET) ||
       !fgets(bytes,sizeof(bytes),stream) || strcmp(bytes,"123456789 1.25\n") || fclose(stream))return 4;
    if(unlink("/tmp/devkit-data") || atexit(cleanup))return 5;
    puts("DEVKIT_MAIN");return 0;
}
