#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>
#define CHECK(c) do { if (!(c)) return __LINE__; } while (0)
static char output[4096];
static int shell(const char *script,const char *expected,int code)
{
    int fds[2]; CHECK(pipe(fds)==0);
    pid_t child=fork(); CHECK(child>=0);
    if(!child){close(fds[0]);dup2(fds[1],1);close(fds[1]);
        char *args[]={"sh","-c",(char *)script,NULL};execv("/bin/sh",args);_exit(99);}
    close(fds[1]);size_t done=0;ssize_t n;
    while((n=read(fds[0],output+done,sizeof(output)-done-1))>0)done+=n;
    CHECK(n==0 && close(fds[0])==0);output[done]=0;
    int status; CHECK(wait(&status)==child && WIFEXITED(status) && WEXITSTATUS(status)==code);
    if(strcmp(output,expected)){fprintf(stderr,"V7 output mismatch:\n%s\n",output);return __LINE__;}
    return 0;
}
int main(void)
{
    CHECK(open("/dev/console",O_RDWR)==0 && dup(0)==1 && dup(0)==2);
    char text[64]; CHECK(snprintf(text,sizeof(text),"%s %04d %ld %.3s","ok",7,1234567890L,"abcdef")==22);
    CHECK(!strcmp(text,"ok 0007 1234567890 abc"));
    CHECK(snprintf(text,4,"hello")==5 && !strcmp(text,"hel"));
    time_t epoch=0;CHECK(!strcmp(ctime(&epoch),"Thu Jan  1 00:00:00 1970\n"));
    unsigned char *memory=calloc(32,1);CHECK(memory);for(unsigned i=0;i<32;++i)CHECK(memory[i]==0);
    memory[0]=42;memory=realloc(memory,512);CHECK(memory && memory[0]==42);free(memory);
    FILE *file=fopen("/tmp/stdio","w+");CHECK(file);
    CHECK(fputs("line\n",file)>=0 && ftell(file)==5 && fseek(file,0,SEEK_SET)==0);
    CHECK(fgets(text,sizeof(text),file) && !strcmp(text,"line\n"));
    CHECK(getc(file)==EOF && feof(file));rewind(file);CHECK(getc(file)=='l' && !feof(file));
    CHECK(fclose(file)==0 && unlink("/tmp/stdio")==0);
    int error=shell("echo hello; echo 'two words'; V=world; echo \"$V\"; exit 7",
                    "hello\ntwo words\nworld\n",7);if(error)return error;
    error=shell("echo data > /tmp/v7-one; echo more >> /tmp/v7-one; cat < /tmp/v7-one; "
                "cp /tmp/v7-one /tmp/v7-two; ln /tmp/v7-two /tmp/v7-link; "
                "echo /tmp/v7-*; echo pipe | cat; "
                "for I in a b; do echo $I; done; "
                "if /bin/sh -c 'exit 0'; then echo yes; else echo no; fi; "
                "echo `echo sub`; cd /tmp; pwd; "
                "rm /tmp/v7-one /tmp/v7-two /tmp/v7-link; exit 0",
                "data\nmore\n/tmp/v7-link /tmp/v7-one /tmp/v7-two\npipe\na\nb\nyes\nsub\n/tmp\n",0);
    if(error)return error;
    error=shell("ln /hello /hello 2>&1", "ln: File exists\n",1);if(error)return error;
    error=shell("ls /bin; echo short-name", "exec-test\nlibc-test\nshort-name\n",0);if(error)return error;
    CHECK(printf("NEXIX_V7_USER_PROGRAM_OK\n")>0);
    return 0;
}
