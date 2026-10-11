#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
int mktemp_test(void)
{
    char name[]="/tmp/tXXXXXX",expected[32];struct stat st;
    CHECK(snprintf(expected,sizeof(expected),"/tmp/t%06d",getpid()%1000000)==12);
    CHECK(mktemp(name)==name&&!strcmp(name,expected));
    CHECK(stat(name,&st)==-1&&errno==ENOENT); /* Naming must not create/reserve. */
    int fd=creat(name,0600);CHECK(fd>=0);
    CHECK(write(fd,"keep",4)==4&&close(fd)==0);
    char second[]="/tmp/tXXXXXX";
    CHECK(mktemp(second)==second&&second[6]=='a'&&!strcmp(second+7,expected+7));
    fd=creat(second,0600);CHECK(fd>=0&&close(fd)==0);
    char third[]="/tmp/tXXXXXX";
    CHECK(mktemp(third)==third&&third[6]=='b');
    /* Occupy all V7 letter candidates; z remains unused. */
    for(char c='b';c<'z';++c){
        second[6]=c;fd=creat(second,0600);CHECK(fd>=0&&close(fd)==0);
    }
    char exhausted[]="/tmp/tXXXXXX";
    CHECK(!strcmp(mktemp(exhausted),"/")&&exhausted[6]=='y');
    second[6]='z';CHECK(stat(second,&st)==-1&&errno==ENOENT);
    fd=open(name,O_RDONLY);CHECK(fd>=0);
    char contents[4];CHECK(read(fd,contents,4)==4&&!memcmp(contents,"keep",4)&&close(fd)==0);
    CHECK(unlink(name)==0);
    for(char c='a';c<'z';++c){second[6]=c;CHECK(unlink(second)==0);}
    char missing[]="/missing/tXXXXXX";
    CHECK(mktemp(missing)==missing&&access(missing,F_OK)==-1&&errno==ENOENT);
    CHECK(stat("/tmp",&st)==0); /* Namespace remains usable after cleanup. */
    return 0;
}
