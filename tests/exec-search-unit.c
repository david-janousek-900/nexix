#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
int nx_test_execvp(const char *,char *const[]);
static const char *path;
static int errors[8],calls,sleeps;
static char names[8][256];
static int inspect_script;
char *nx_test_getenv(const char *key){assert(!strcmp(key,"PATH"));return (char *)path;}
unsigned nx_test_sleep(unsigned n){assert(n==(unsigned)sleeps+2);++sleeps;return 0;}
int nx_test_execv(const char *name,char *const argv[])
{
    assert(calls<8);strcpy(names[calls],name);
    if(inspect_script && calls==1){
        assert(!strcmp(argv[0],"sh") && !strcmp(argv[1],names[0]));
        assert(!strcmp(argv[2],"argument") && !argv[3]);
    }
    errno=errors[calls]?errors[calls]:ENOENT;++calls;return -1;
}
static void reset(const char *p){path=p;calls=sleeps=inspect_script=0;memset(errors,0,sizeof(errors));}
int main(void)
{
    char *argv[]={"original","argument",0};
    reset(0);assert(nx_test_execvp("missing",argv)==-1 && calls==3);
    assert(!strcmp(names[0],"missing") && !strcmp(names[1],"/bin/missing") && !strcmp(names[2],"/usr/bin/missing"));
    reset("/bad:/bin");errors[0]=ENOTDIR;
    assert(nx_test_execvp("cmd",argv)==-1 && calls==2);
    reset("/bad:/bin");errors[0]=EACCES;
    assert(nx_test_execvp("cmd",argv)==-1 && errno==EACCES && calls==2);
    reset("/bin");errors[0]=ENOEXEC;inspect_script=1;
    assert(nx_test_execvp("script",argv)==-1 && calls==2 && !strcmp(names[1],"/bin/sh"));
    reset("/bin");errors[0]=ENOEXEC;inspect_script=1;
    assert(nx_test_execvp("/tmp/script",argv)==-1 && calls==2 && !strcmp(names[0],"/tmp/script"));
    reset("/bin");for(int i=0;i<5;++i)errors[i]=ETXTBSY;
    assert(nx_test_execvp("busy",argv)==-1 && calls==5 && sleeps==4 && errno==ETXTBSY);
    reset("/bin");errors[0]=ENOMEM;
    assert(nx_test_execvp("cmd",argv)==-1 && calls==1 && errno==ENOMEM);
    char huge[300];memset(huge,'x',299);huge[299]=0;reset(huge);
    assert(nx_test_execvp("cmd",argv)==-1 && errno==E2BIG && calls==0);
    char *many[257];for(int i=0;i<256;++i)many[i]="x";many[256]=0;
    reset("/bin");errors[0]=ENOEXEC;
    assert(nx_test_execvp("script",many)==-1 && errno==E2BIG && calls==1);
    puts("PASS: V7 PATH defaults/errors, script fallback, ETXTBSY retry and native bounds");
}
