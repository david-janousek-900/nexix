#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
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
    return 0;
}
