#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/wait.h>
#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)
static volatile sig_atomic_t caught;
static void catch_iot(int n) { if (n == SIGIOT) ++caught; }
static void forbidden(void) { _exit(98); }
static void callback(void) { (void)write(2, "callback\n", 9); }
static int disabled_assertions(void)
{
    int n = 0;
#define NDEBUG
#include <assert.h>
    assert(++n); _assert(++n);
#undef NDEBUG
#include <assert.h>
    assert(++n); _assert(++n);
    return n == 2;
}
int libc_helpers_test(void)
{
    unsigned char from[] = {0xa5,1,2,3,4,5,0x5a};
    unsigned char to[] = {0xa5,9,9,9,9,9,0x5a};
    swab(from+1,to+1,5);
    const unsigned char expected[] = {0xa5,2,1,4,3,9,0x5a};
    CHECK(!memcmp(to,expected,sizeof(to)));
    swab(from+1,from+1,4);
    CHECK(from[1]==2 && from[2]==1 && from[3]==4 && from[4]==3 && from[5]==5);
    swab(NULL,NULL,-1); swab(NULL,NULL,0); swab(NULL,NULL,1);
    long values[] = {0,0x123456,0xffffff,0x123456789abcdefL,-1};
    char packed[17]; memset(packed,0xa5,sizeof(packed));
    ltol3(packed+1,values,5);
    const unsigned char golden[] = {0,0,0,0x12,0x56,0x34,0xff,0xff,0xff,0xab,0xef,0xcd,0xff,0xff,0xff};
    CHECK(!memcmp(packed+1,golden,sizeof(golden)) && (unsigned char)packed[0]==0xa5 && (unsigned char)packed[16]==0xa5);
    long decoded[7] = {-99,0,0,0,0,0,-99};
    l3tol(decoded+1,packed+1,5);
    CHECK(decoded[0]==-99 && decoded[6]==-99);
    CHECK(decoded[1]==0 && decoded[2]==0x123456 && decoded[3]==0xffffff && decoded[4]==0xabcdef && decoded[5]==0xffffff);
    ltol3(NULL,NULL,0); l3tol(NULL,NULL,-1);
    char text[80];
    CHECK(gcvt(12.5,6,text)==text && !strcmp(text,"12.5"));
    CHECK(!strcmp(gcvt(-.125,4,text),"-.125"));
    CHECK(!strcmp(gcvt(.0001,6,text),".0001"));
    CHECK(!strcmp(gcvt(.00001,6,text),"1.e-05"));
    CHECK(!strcmp(gcvt(10000.,6,text),"10000"));
    CHECK(!strcmp(gcvt(100000.,6,text),"1.e+05"));
    CHECK(!strcmp(gcvt(9.999,3,text),"10"));
    CHECK(!strcmp(gcvt(1e100,1,text),"1.e+100"));
    CHECK(!strcmp(gcvt(1e-100,1,text),"1.e-100"));
    CHECK(!strcmp(gcvt(0x1p-1074,2,text),"4.9e-324"));
    CHECK(!strcmp(gcvt(0.,6,text),"0") && !strcmp(gcvt(-0.,6,text),"-0"));
    CHECK(!strcmp(gcvt(INFINITY,6,text),"inf") && !strcmp(gcvt(-INFINITY,6,text),"-inf"));
    CHECK(!strcmp(gcvt(NAN,6,text),"nan"));
    CHECK(!strcmp(gcvt(12.,0,text),"10"));
    CHECK(!strcmp(gcvt(1.,1000000,text),"1"));
    CHECK(disabled_assertions());
    errno = 0; CHECK(tell(-1)==-1 && errno==EBADF);
    int pipefd[2]; CHECK(pipe(pipefd)==0);
    CHECK(tell(pipefd[0])==-1 && errno==ESPIPE);
    CHECK(close(pipefd[0])==0 && close(pipefd[1])==0);
    pid_t child = fork(); CHECK(child>=0);
    if (!child) { CHECK(atexit(forbidden)==0); abort(); _exit(99); }
    int status;
    CHECK(wait(&status)==child && WIFSIGNALED(status) && WTERMSIG(status)==SIGIOT);
    CHECK(signal(SIGIOT,catch_iot)==SIG_DFL);
    CHECK(abort()==0 && caught==1);
    CHECK(signal(SIGIOT,SIG_IGN)==SIG_DFL); /* V7 caught dispositions reset. */
    CHECK(abort()==0 && caught==1);
    CHECK(signal(SIGIOT,SIG_DFL)==SIG_IGN);
    CHECK(pipe(pipefd)==0);
    child=fork(); CHECK(child>=0);
    if (!child) {
        CHECK(close(pipefd[0])==0 && dup2(pipefd[1],2)==2 && close(pipefd[1])==0);
        CHECK(atexit(callback)==0);
        assert(0); _exit(99);
    }
    CHECK(close(pipefd[1])==0);
    char diagnostic[256]; ssize_t used=0, n;
    while ((n=read(pipefd[0],diagnostic+used,sizeof(diagnostic)-1-(size_t)used))>0) used+=n;
    CHECK(n==0 && close(pipefd[0])==0);
    diagnostic[used]=0;
    CHECK(wait(&status)==child && WIFEXITED(status) && WEXITSTATUS(status)==1);
    CHECK(!strncmp(diagnostic,"Assertion failed: file user/libc-helpers-test.c, line ",strlen("Assertion failed: file user/libc-helpers-test.c, line ")));
    CHECK(used>=10 && !strcmp(diagnostic+used-10,"\ncallback\n"));
    CHECK(wait(NULL)==-1 && errno==ECHILD);
    return 0;
}
