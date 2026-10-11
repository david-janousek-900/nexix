#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
int errno;
static pid_t pid;
static int collisions, probes, failure;
static char last[128];
pid_t getpid(void) { return pid; }
int access(const char *name,int mode)
{
    if (mode!=F_OK) return 0;
    strcpy(last,name);
    ++probes;
    if (probes<=collisions) return 0;
    errno=failure;return -1;
}
static void reset(pid_t value,int occupied,int error)
{ pid=value;collisions=occupied;failure=error;probes=0; }
int main(void)
{
    char name[128];
    reset(123,0,ENOENT);strcpy(name,"/tmp/tXXXXXX");
    CHECK(mktemp(name)==name&&!strcmp(name,"/tmp/t000123")&&probes==1);
    reset(2147483647,0,ENOENT);strcpy(name,"/tmp/tXXXXX");
    CHECK(mktemp(name)==name&&!strcmp(name,"/tmp/t83647"));
    reset(42,1,ENOENT);strcpy(name,"/tmp/tXXXXXX");
    CHECK(mktemp(name)==name&&!strcmp(name,"/tmp/ta00042")&&probes==2);
    reset(42,25,ENOENT);strcpy(name,"/tmp/tXXXXXX");
    CHECK(mktemp(name)==name&&!strcmp(name,"/tmp/ty00042")&&probes==26);
    reset(42,26,ENOENT);strcpy(name,"/tmp/tXXXXXX");
    CHECK(!strcmp(mktemp(name),"/")&&!strcmp(name,"/tmp/ty00042")&&probes==26);
    reset(42,0,EACCES);strcpy(name,"/denied/tXXXXXX");
    CHECK(mktemp(name)==name&&!strcmp(name,"/denied/t000042")&&errno==EACCES);
    reset(42,0,ENOENT);strcpy(name,"xXXXXXXXXXXXX");
    CHECK(mktemp(name)==name&&!strcmp(name,"x000000000042"));
    reset(42,0,ENOENT);strcpy(name,"/tmp/tX");
    CHECK(mktemp(name)==name&&!strcmp(name,"/tmp/t2"));
    reset(42,0,ENOENT);strcpy(name,"XXXXXX");
    CHECK(mktemp(name)==name&&!strcmp(name,"000042"));
    reset(42,0,ENOENT);
    strcpy(name,"/tmp/no-suffix");
    CHECK(mktemp(name)==name&&!strcmp(name,"/tmp/no-suffix"));
    reset(42,1,ENOENT);errno=0;
    CHECK(!strcmp(mktemp(name),"/")&&errno==EINVAL&&!strcmp(name,"/tmp/no-suffix")&&probes==1);
    reset(42,0,ENOENT);name[0]=0;errno=0;
    CHECK(!strcmp(mktemp(name),"/")&&errno==EINVAL&&name[0]==0);
    CHECK(!strcmp(mktemp(NULL),"/")&&errno==EINVAL&&probes==0);
    puts("PASS: V7 mktemp PID widths, a-y collision sequence, exhaustion, access errors and bounded malformed templates");
    return 0;
}
