#include <stdio.h>
#include <nexix/stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
extern long v7_stdio_gdate(FILE *);
extern FILE *v7_stdio_refer_output(int);
extern FILE *v7_stdio_refer_input(int);
extern void v7_stdio_quiz_output(void);
extern void v7_stdio_cpp_output(FILE *);
static int remote_report(char *out,size_t capacity,const char *fmt,...)
{
    va_list ap;va_start(ap,fmt);
    struct nx_printf_remote remote;
    nx_printf_remote_init(&remote,fmt,ap);
    int result=snprintf(out,capacity,"prefix:%r ignored %d",&remote,999);
    char again[256];
    int second=snprintf(again,sizeof(again),"prefix:%r ignored",&remote);
    if(result>=0){
        char shifted[256];
        int third=snprintf(shifted,sizeof(shifted),"id=%d prefix:%r lost",9,&remote);
        if(third!=result+5||strncmp(shifted,"id=9 ",5)||strcmp(shifted+5,again))result=-97;
    }
    nx_printf_remote_end(&remote);va_end(ap);
    if(second!=result)return -99; /* A packet is reusable, including errors. */
    if(result>=0&&capacity>=sizeof(again)&&strcmp(out,again))return -98;
    return result;
}
static int nested_report(char *out,size_t capacity,const char *fmt,...)
{
    va_list ap;va_start(ap,fmt);
    struct nx_printf_remote inner;
    nx_printf_remote_init(&inner,fmt,ap);
    int n=remote_report(out,capacity,"outer:%r lost",&inner);
    nx_printf_remote_end(&inner);va_end(ap);return n;
}
static int cycle_report(char *out,size_t capacity,struct nx_printf_remote *loop,const char *fmt,...)
{
    va_list ap;va_start(ap,fmt);
    nx_printf_remote_init(loop,fmt,ap);
    int n=snprintf(out,capacity,"loop:%r",loop);
    nx_printf_remote_end(loop);va_end(ap);return n;
}
static int remote_file(FILE *f,const char *fmt,...)
{
    va_list ap;va_start(ap,fmt);
    struct nx_printf_remote remote;
    nx_printf_remote_init(&remote,fmt,ap);
    int n=fprintf(f,"file:%r lost",&remote);
    nx_printf_remote_end(&remote);va_end(ap);return n;
}
int stdio_compat_test(void)
{
    char output[256];
    const char *expected="prefix:7 1.25 native 4294967296 8 9 10 11 2.5";
    CHECK(remote_report(output,sizeof(output),"%d %.2f %s %ld %d %d %d %d %.1f",
        7,1.25,"native",4294967296L,8,9,10,11,2.5)==(int)strlen(expected));
    CHECK(!strcmp(output,expected));
    CHECK(remote_report(output,5,"%d",123)==10&&!strcmp(output,"pref"));
    CHECK(remote_report(NULL,0,"%d",123)==10);
    CHECK(remote_report(output,sizeof(output),"%*.*f %X",8,2,1.25,0x123456789aUL)==26);
    CHECK(!strcmp(output,"prefix:    1.25 123456789a"));
    CHECK(nested_report(output,sizeof(output),"%d/%s",42,"nested")==22);
    CHECK(!strcmp(output,"prefix:outer:42/nested"));
    CHECK(remote_report(output,sizeof(output),"bad:%q",1)==-1&&errno==EINVAL);
    CHECK(!strcmp(output,"prefix:bad:"));
    CHECK(snprintf(output,sizeof(output),"safe:%r",(struct nx_printf_remote *)NULL)==-1&&errno==EINVAL);
    CHECK(!strcmp(output,"safe:"));
    struct nx_printf_remote loop;
    CHECK(cycle_report(output,sizeof(output),&loop,"%r",&loop)==-1&&errno==EINVAL);
    CHECK(!strcmp(output,"loop:")&&loop.format==NULL);
    CHECK(snprintf(output,sizeof(output),"ended:%r",&loop)==-1&&errno==EINVAL&&!strcmp(output,"ended:"));
    const char *path="/tmp/stdio-compat";
    FILE *f=fopen(path,"w+");CHECK(f);
    CHECK(remote_file(f,"%d/%.1f/%s",17,2.5,"native")==18);
    CHECK(fseek(f,0,SEEK_SET)==0&&fread(output,1,18,f)==18&&!memcmp(output,"file:17/2.5/native",18));
    struct stat st;CHECK(fstat(fileno(f),&st)==0&&v7_stdio_gdate(f)==st.st_mtime);
    setbuf(f,NULL);
    int fd=fileno(f);CHECK(close(fd)==0);
    CHECK(remote_file(f,"%d",7)==-1&&ferror(f)&&errno==EBADF);
    CHECK(fclose(f)==EOF&&errno==EBADF);
    int pair[2];CHECK(pipe(pair)==0);
    FILE *writer=v7_stdio_refer_output(pair[1]);CHECK(writer);
    CHECK(fputs("refer",writer)==0&&fclose(writer)==0);
    FILE *reader=v7_stdio_refer_input(pair[0]);CHECK(reader);
    CHECK(fread(output,1,5,reader)==5&&!memcmp(output,"refer",5)&&getc(reader)==EOF);
    CHECK(fclose(reader)==0&&fcntl(pair[0],F_GETFL)==-1&&errno==EBADF);
    CHECK(fcntl(pair[1],F_GETFL)==-1&&errno==EBADF);
    f=fopen(path,"w");CHECK(f);
    v7_stdio_cpp_output(f);
    CHECK(fputs("cpp",f)==0&&stat(path,&st)==0&&st.st_size==0);
    CHECK(fclose(f)==0&&stat(path,&st)==0&&st.st_size==3);
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        if(!freopen(path,"w",stdout))_exit(1);
        v7_stdio_quiz_output();
        if(putchar('q')!='q'||stat(path,&st)||st.st_size!=1)_exit(2);
        _exit(0); /* Unbuffered output must survive bypassing exit flushing. */
    }
    int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    f=fopen(path,"r");CHECK(f&&getc(f)=='q'&&getc(f)==EOF&&fclose(f)==0);
    CHECK(unlink(path)==0);
    return 0;
}
