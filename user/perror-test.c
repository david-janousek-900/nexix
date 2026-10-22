#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
int perror_test(void)
{
    CHECK(sys_nerr==58&&!strcmp(sys_errlist[ECONNREFUSED],"Connection refused")&&!strcmp(sys_errlist[EPERM],"Not owner")&&!strcmp(sys_errlist[ERANGE],"Result too large"));
    const char *expected[]={
        "open: No such file or directory\nUnknown error\nUnknown error\nsyscall: Function not implemented\n",
        "Not owner\npending",
        ""
    };
    for(int mode=0;mode<3;++mode){
        int pair[2];CHECK(pipe(pair)==0);
        pid_t child=fork();CHECK(child>=0);
        if(!child){
            if(close(pair[0])||dup2(pair[1],2)!=2||close(pair[1]))_exit(1);
            if(mode==0){
                stderr=NULL; /* perror must use descriptor 2, not this pointer. */
                errno=ENOENT;perror("open");if(errno!=ENOENT)_exit(2);
                errno=-1;perror(NULL);if(errno!=-1)_exit(3);
                errno=sys_nerr;perror("");if(errno!=sys_nerr)_exit(4);
                errno=ENOSYS;perror("syscall");if(errno!=ENOSYS)_exit(5);
            }else if(mode==1){
                char buffer[BUFSIZ];setbuf(stderr,buffer);
                if(fputs("pending",stderr))_exit(6);
                errno=EPERM;perror("");
                if(errno!=EPERM||fflush(stderr))_exit(7);
            }else{
                if(close(2))_exit(8);
                errno=EACCES;perror("closed");
                if(errno!=EACCES||ferror(stderr))_exit(9);
            }
            _exit(0);
        }
        CHECK(close(pair[1])==0);
        char text[256];size_t used=0;ssize_t n;
        while((n=read(pair[0],text+used,sizeof(text)-1-used))>0)used+=(size_t)n;
        CHECK(n==0&&close(pair[0])==0);text[used]=0;
        int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
        CHECK(!strcmp(text,expected[mode]));
    }
    int pair[2];CHECK(pipe(pair)==0&&close(pair[0])==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        if(dup2(pair[1],2)!=2||close(pair[1])||signal(SIGPIPE,SIG_IGN)==SIG_ERR)_exit(1);
        errno=EIO;perror("broken");_exit(errno==EIO&&!ferror(stderr)?0:2);
    }
    CHECK(close(pair[1])==0);
    int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    return 0;
}
