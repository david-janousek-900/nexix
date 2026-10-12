#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
static long refs(void)
{long n=0x905;__asm__ volatile("int $0x81":"+a"(n)::"memory","cc");return n;}
int main(int argc,char **argv)
{
    CHECK(argc==4);int ready=atoi(argv[2]),go=atoi(argv[3]);char byte;
    CHECK(refs()==(!strcmp(argv[1],"shared")?2:1));
    if(!strcmp(argv[1],"fork")){
        pid_t child=fork();CHECK(child>=0);
        if(!child)_exit(refs()==2&&write(ready,"F",1)==1&&read(go,&byte,1)==1?0:1);
        int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0&&refs()==1);
        CHECK(write(ready,"P",1)==1&&read(go,&byte,1)==1);return 0;
    }
    CHECK(write(ready,"R",1)==1&&read(go,&byte,1)==1);
    if(!strcmp(argv[1],"same")){
        argv[1]="hold";execv("/tmp/text",argv);return 91;
    }
    if(!strcmp(argv[1],"replace")){
        CHECK(execv("/tmp/missing",argv)==-1&&errno==ENOENT&&refs()==1);
        CHECK(write(ready,"B",1)==1&&read(go,&byte,1)==1);
        argv[1]="hold";execv("/tmp/text2",argv);return 90;
    }
    return 0;
}
