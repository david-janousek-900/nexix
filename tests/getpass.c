#include <stdio.h>
#include <unistd.h>
#include <sgtty.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
int errno;
static FILE input,standard,error_stream;
FILE *stdin=&standard,*stderr=&error_stream;
static struct sgttyb settings;
static const char *text;
static char output[128];
static size_t position;
static int live,fallback,fail_gtty,fail_stty,fail_signal,fail_read,fail_write,fail_flush,fail_close;
static int settings_calls,signal_calls,flushes,setup_violation;
static void handler(int n){(void)n;}
static sighandler_t disposition;
FILE *fopen(const char *path,const char *mode)
{
    if(strcmp(path,"/dev/tty")||strcmp(mode,"r")){errno=EINVAL;return NULL;}
    if(fallback){errno=ENXIO;return NULL;}
    ++live;return &input;
}
void setbuf(FILE *f,char *buffer)
{if(f!=&input||buffer)setup_violation=1;}
int fileno(FILE *f){return f==&input?3:0;}
int fclose(FILE *f)
{if(f!=&input)setup_violation=1;--live;if(fail_close){errno=EIO;return -1;}return 0;}
sighandler_t signal(int n,sighandler_t next)
{
    ++signal_calls;if(n!=SIGINT)setup_violation=1;
    if(signal_calls==fail_signal){errno=EINVAL;return SIG_ERR;}
    sighandler_t old=disposition;disposition=next;return old;
}
int gtty(int fd,struct sgttyb *out)
{(void)fd;if(fail_gtty){errno=ENOTTY;return -1;}*out=settings;return 0;}
int stty(int fd,const struct sgttyb *in)
{
    (void)fd;++settings_calls;
    if(settings_calls==fail_stty){errno=EIO;return -1;}
    settings=*in;return 0;
}
int fputs(const char *s,FILE *f)
{
    if(f!=stderr)setup_violation=1;
    if(fail_write){errno=EBADF;return EOF;}
    strcat(output,s);return 0;
}
int fflush(FILE *f)
{(void)f;++flushes;if(fail_flush){errno=EIO;return EOF;}return 0;}
int getc(FILE *f)
{
    if((settings.sg_flags&ECHO)||disposition!=SIG_IGN)setup_violation=1;
    if(fail_read){f->error=1;errno=EINTR;return EOF;}
    if(!text[position])return EOF;
    return (unsigned char)text[position++];
}
int ferror(FILE *f){return f->error;}
static void reset(const char *value)
{
    input=(FILE){0};standard=(FILE){0};settings=(struct sgttyb){1,2,3,4,ECHO|CRMOD};
    text=value;position=0;output[0]=0;disposition=handler;
    fallback=fail_gtty=fail_stty=fail_signal=fail_read=fail_write=fail_flush=fail_close=0;
    settings_calls=signal_calls=flushes=setup_violation=0;
}
static int restored(void)
{return settings.sg_flags==(ECHO|CRMOD)&&settings.sg_erase==3&&disposition==handler&&live==0&&!setup_violation;}
int main(void)
{
    reset("123456789EXCESS\n");char *p=getpass("Password %s: ");
    CHECK(p&&!strcmp(p,"12345678")&&position==16&&restored());
    CHECK(!strcmp(output,"Password %s: \n")&&flushes==2);
    reset("a\n");CHECK(getpass("")==p&&!strcmp(p,"a")&&restored());
    reset("");CHECK(getpass("EOF")==p&&!strcmp(p,"")&&restored());
    reset("last");CHECK(getpass("")==p&&!strcmp(p,"last")&&restored());
    reset("fallback\n");fallback=1;CHECK(getpass("")==p&&!strcmp(p,"fallback")&&restored());
    reset("x\n");CHECK(getpass(NULL)==NULL&&errno==EINVAL&&signal_calls==0&&restored());
    reset("x\n");fail_gtty=1;CHECK(getpass("")==NULL&&errno==ENOTTY&&position==0&&restored());
    reset("x\n");fail_stty=1;CHECK(getpass("")==NULL&&errno==EIO&&position==0&&restored());
    reset("x\n");fail_signal=1;CHECK(getpass("")==NULL&&errno==EINVAL&&position==0&&restored());
    reset("x\n");fail_read=1;CHECK(getpass("")==NULL&&errno==EINTR&&restored());
    reset("x\n");fail_write=1;CHECK(getpass("")==NULL&&errno==EBADF&&position==0&&restored());
    reset("x\n");fail_flush=1;CHECK(getpass("")==NULL&&errno==EIO&&position==0&&restored());
    reset("x\n");fail_close=1;CHECK(getpass("")==NULL&&errno==EIO&&restored());
    reset("x\n");fail_stty=2;CHECK(getpass("")==NULL&&errno==EIO&&live==0&&disposition==handler);
    reset("x\n");fail_signal=2;CHECK(getpass("")==NULL&&errno==EINVAL&&live==0&&(settings.sg_flags&ECHO));
    return 0;
}
