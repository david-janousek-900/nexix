/* Run real installed commands; inspect exact output, status and guest state. */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include "command-runner.h"
#define CHECK(c) do { if(!(c)){printf("COMMAND_ERROR %s:%d\n",__FILE__,__LINE__);exit(1);} } while(0)
static void putfile(const char *path,const char *text)
{int fd=creat(path,0644);CHECK(fd>=0);CHECK(write(fd,text,strlen(text))==(ssize_t)strlen(text));CHECK(!close(fd));}
static void contents(const char *path,const char *text)
{char data[4096];int fd=open(path,O_RDONLY);CHECK(fd>=0);ssize_t n=read(fd,data,sizeof(data));CHECK(n==(ssize_t)strlen(text) && !memcmp(data,text,n));CHECK(!close(fd));}
static struct stat metadata(const char *path)
{struct stat st;CHECK(!stat(path,&st));return st;}
static void command(char *const argv[],int expected,const char *output,const char *diagnostic)
{
 char path[32],error[1024];sprintf(path,"/tmp/cmd%u",(unsigned)getpid());int in=open("/dev/null",O_RDONLY),out=creat(path,0600);CHECK(in>=0 && out>=0);
 int status=run_command(argv,in,out,error,sizeof(error));
 if(status!=expected || strcmp(error,diagnostic))printf("COMMAND_RESULT %s status=%d expected=%d error=%s\n",argv[0],status,expected,error);
 CHECK(status==expected && !strcmp(error,diagnostic));CHECK(!close(in) && !close(out));contents(path,output);CHECK(!unlink(path));
}
#define CMD(...) (char *const[]){__VA_ARGS__,NULL}
