#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
static FILE *exit_stream;
static void exit_output(void){(void)fputs("handler",exit_stream);}
static int buffering_test(void)
{
    const char *path="/tmp/buffering";
    char supplied[BUFSIZ],data[BUFSIZ*3+17],out[sizeof(data)];struct stat st;
    for(unsigned i=0;i<sizeof(data);++i)data[i]=(char)(i*13+7);
    FILE *f=fopen(path,"w+");CHECK(f);
    setbuf(f,supplied);
    CHECK(fwrite(data,1,17,f)==17&&ftell(f)==17);
    CHECK(fstat(fileno(f),&st)==0&&st.st_size==0);
    CHECK(!memcmp(supplied,data,17)); /* Caller buffer is actually used. */
    CHECK(fflush(f)==0&&fstat(fileno(f),&st)==0&&st.st_size==17);
    CHECK(fwrite(data+17,1,sizeof(data)-17,f)==sizeof(data)-17);
    CHECK(ftell(f)==sizeof(data)&&fseek(f,0,SEEK_SET)==0);
    CHECK(fread(out,1,1,f)==1&&out[0]==data[0]&&ftell(f)==1);
    CHECK(lseek(fileno(f),0,SEEK_CUR)==BUFSIZ); /* Read ahead. */
    CHECK(fflush(f)==0&&getc(f)==(unsigned char)data[1]); /* V7 input flush is a no-op. */
    CHECK(fseek(f,10,SEEK_CUR)==0&&ftell(f)==12&&getc(f)==(unsigned char)data[12]);
    CHECK(ungetc('q',f)==0&&ftell(f)==12);
    CHECK(fseek(f,0,SEEK_CUR)==0&&getc(f)==(unsigned char)data[12]);
    CHECK(fseek(f,0,SEEK_SET)==0&&fread(out,1,sizeof(out),f)==sizeof(out));
    CHECK(!memcmp(out,data,sizeof(data))&&getc(f)==EOF&&feof(f));
    rewind(f);CHECK(!feof(f)&&getc(f)==(unsigned char)data[0]);
    CHECK(fputc('Z',f)=='Z'&&ftell(f)==2); /* Update stream writes at logical position. */
    CHECK(fseek(f,1,SEEK_SET)==0&&getc(f)=='Z');
    CHECK(fclose(f)==0);
    f=fopen(path,"w");CHECK(f);setbuf(f,NULL);
    CHECK(fputs("now",f)==0&&fstat(fileno(f),&st)==0&&st.st_size==3&&fclose(f)==0);
    f=fopen(path,"a");CHECK(f&&fseek(f,0,SEEK_SET)==0);
    CHECK(fputs("!",f)==0&&ftell(f)==4&&lseek(fileno(f),0,SEEK_CUR)==0&&fflush(NULL)==0&&fclose(f)==0);
    f=fopen(path,"r");CHECK(f&&ungetc('Q',f)==0&&getc(f)=='Q'&&fread(out,1,4,f)==4&&!memcmp(out,"now!",4)&&fclose(f)==0);
    /* Close flushes; failed flush reports errors and still closes the descriptor. */
    f=fopen(path,"w");CHECK(f&&fputs("close",f)==0&&fclose(f)==0);
    CHECK(stat(path,&st)==0&&st.st_size==5);
    f=fopen(path,"a");CHECK(f&&fputs("bad",f)==0);
    int fd=fileno(f);CHECK(close(fd)==0&&fflush(f)==EOF&&ferror(f)&&errno==EBADF);
    CHECK(fclose(f)==EOF&&errno==EBADF);
    /* exit flushes after callbacks, _exit and successful exec discard buffers. */
    for(int mode=0;mode<3;++mode){
        pid_t child=fork();CHECK(child>=0);
        if(!child){
            exit_stream=fopen(path,"w");if(!exit_stream)_exit(1);
            if(fputs("child",exit_stream)<0)_exit(2);
            if(mode==0){if(atexit(exit_output))_exit(3);exit(0);}
            if(mode==1)_exit(0);
            char *args[]={"/bin/files-test","stdio-exec",NULL};
            execv(args[0],args);_exit(4);
        }
        int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
        CHECK(stat(path,&st)==0&&st.st_size==(mode==0?12:0));
        if(mode==0){f=fopen(path,"r");CHECK(f&&fread(out,1,12,f)==12&&!memcmp(out,"childhandler",12)&&fclose(f)==0);}
    }
    /* Pre-fork flush avoids duplicate output; inherited pending buffers are
     * intentionally not flushed by fork itself, as in V7. */
    f=fopen(path,"w");CHECK(f&&fputs("parent",f)==0&&fflush(f)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){if(fputs("child",f)<0)_exit(1);exit(0);}
    int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    CHECK(fclose(f)==0&&stat(path,&st)==0&&st.st_size==11);
    f=fopen(path,"w");CHECK(f&&fputs("copy",f)==0);
    child=fork();CHECK(child>=0);
    if(!child)exit(0);
    CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    CHECK(fclose(f)==0&&stat(path,&st)==0&&st.st_size==8);
    f=fopen(path,"r");CHECK(f&&fread(out,1,8,f)==8&&!memcmp(out,"copycopy",8)&&fclose(f)==0);
    child=fork();CHECK(child>=0);
    if(!child){
        int diagnostic=open(path,O_WRONLY|O_TRUNC);if(diagnostic<0)_exit(1);
        if(dup2(diagnostic,2)!=2)_exit(2);
        if(fputs("diagnostic",stderr)<0)_exit(3);
        if(fstat(2,&st)<0||st.st_size!=10)_exit(4);
        _exit(0); /* Initial stderr is immediate, even redirected to a file. */
    }
    CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    CHECK(stat(path,&st)==0&&st.st_size==10);
    int pair[2];CHECK(pipe(pair)==0);
    child=fork();CHECK(child>=0);
    if(!child){
        close(pair[0]);FILE *writer=fdopen(pair[1],"w");if(!writer)_exit(1);
        if(fwrite(data,1,sizeof(data),writer)!=sizeof(data))_exit(2);
        exit(fclose(writer)==0?0:3);
    }
    CHECK(close(pair[1])==0);f=fdopen(pair[0],"r");CHECK(f);
    CHECK(getc(f)==(unsigned char)data[0]);
    CHECK(fseek(f,0,SEEK_CUR)==-1&&errno==ESPIPE); /* Failed seek retains unread data. */
    CHECK(fread(out+1,1,sizeof(out)-1,f)==sizeof(out)-1);out[0]=data[0];
    CHECK(!memcmp(out,data,sizeof(data))&&getc(f)==EOF&&fclose(f)==0);
    CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    sighandler_t saved=signal(SIGPIPE,SIG_IGN);
    CHECK(saved!=SIG_ERR&&pipe(pair)==0&&close(pair[0])==0);
    f=fdopen(pair[1],"w");CHECK(f&&fputs("broken",f)==0);
    CHECK(fflush(f)==EOF&&errno==EPIPE&&ferror(f)&&fclose(f)==EOF);
    CHECK(signal(SIGPIPE,saved)!=SIG_ERR);
    CHECK(unlink(path)==0);return 0;
}
int stdio_test(void)
{
    const char *path="/tmp/stdio";char bytes[16]={0};
    FILE *f=fopen(path,"w+");CHECK(f!=NULL);
    CHECK(fwrite("abc",1,3,f)==3&&ftell(f)==3);
    CHECK(fseek(f,0,SEEK_SET)==0&&getc(f)=='a'&&ftell(f)==1);
    CHECK(ungetc(0xff,f)==0&&ftell(f)==0);
    CHECK(ungetc('x',f)==EOF); /* One-byte guarantee, no overwrite. */
    CHECK(fread(bytes,1,2,f)==2&&(unsigned char)bytes[0]==255&&bytes[1]=='b');
    CHECK(ungetc('X',f)==0&&fseek(f,0,SEEK_CUR)==0&&getc(f)=='b');
    CHECK(getc(f)=='c'&&getc(f)==EOF&&feof(f));
    CHECK(ungetc('z',f)==0&&!feof(f)&&getc(f)=='z');
    CHECK(getc(f)==EOF&&feof(f)&&ungetc(EOF,f)==EOF);
    clearerr(f);CHECK(!feof(f)&&!ferror(f));
    rewind(f);CHECK(fread(bytes,2,2,f)==1&&!memcmp(bytes,"abc",3)&&feof(f));
    CHECK(fclose(f)==0);
    int fd=open(path,O_RDONLY);CHECK(fd>=0);
    CHECK(fdopen(fd,"w")==NULL&&errno==EINVAL&&fcntl(fd,F_GETFL)>=0);
    f=fdopen(fd,"r");CHECK(f!=NULL);
    CHECK(fwrite("x",1,1,f)==0&&ferror(f)&&errno==EBADF);
    clearerr(f);CHECK(!ferror(f)&&getc(f)=='a');
    CHECK(fclose(f)==0&&fcntl(fd,F_GETFL)==-1&&errno==EBADF);
    fd=open(path,O_RDWR);CHECK(fd>=0&&lseek(fd,0,SEEK_SET)==0);
    f=fdopen(fd,"a+");CHECK(f!=NULL);
    CHECK(fwrite("d",1,1,f)==1&&fseek(f,0,SEEK_SET)==0);
    CHECK(fread(bytes,1,4,f)==4&&!memcmp(bytes,"abcd",4));
    CHECK(freopen(path,"w+",f)==f&&ftell(f)==0);
    CHECK(putw(0x12345678,f)==0&&putw(-1,f)==0&&ftell(f)==8);
    rewind(f);CHECK(getw(f)==0x12345678&&getw(f)==-1&&!feof(f)&&getw(f)==EOF&&feof(f));
    CHECK(freopen(path,"w+",f)==f);
    CHECK(fputs("17 -2.5 tail!",f)==0&&fseek(f,0,SEEK_SET)==0);
    int number=0;double real=0;
    CHECK(fscanf(f,"%d %lf %4s",&number,&real,bytes)==3);
    CHECK(number==17&&real==-2.5&&!strcmp(bytes,"tail")&&getc(f)=='!');
    CHECK(fseek(f,0,SEEK_SET)==0&&fscanf(f,"x")==0&&getc(f)=='1');
    CHECK(fseek(f,0,SEEK_END)==0&&fscanf(f,"%d",&number)==EOF&&feof(f));
    CHECK(fseek(f,0,SEEK_SET)==0&&fclose(f)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        if(!freopen(path,"r",stdin))_exit(1);
        if(scanf("%d %lf",&number,&real)!=2||number!=17||real!=-2.5)_exit(2);
        if(!gets(bytes)||strcmp(bytes," tail!"))_exit(4);
        _exit(fclose(stdin)?3:0);
    }
    int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    f=fopen(path,"r");CHECK(f!=NULL);fd=fileno(f);
    CHECK(freopen("/missing/file","r",f)==NULL&&errno==ENOENT);
    CHECK(fcntl(fd,F_GETFL)==-1&&errno==EBADF);
    CHECK(fdopen(-1,"r")==NULL&&errno==EBADF);
    CHECK(fopen(path,"invalid")==NULL&&errno==EINVAL);
    CHECK(unlink(path)==0);
    return buffering_test();
}
