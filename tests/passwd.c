#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
int errno;
static FILE streams[4];
static const char *database;
static int open_error,fail_after=-1,live;
FILE *fopen(const char *path,const char *mode)
{
    if(strcmp(path,"/etc/passwd")||strcmp(mode,"r")){errno=EINVAL;return NULL;}
    if(open_error){errno=open_error;return NULL;}
    for(unsigned i=0;i<4;++i)if(!streams[i].used){
        streams[i]=(FILE){.used=1,.buffer=(unsigned char *)database};++live;return &streams[i];
    }
    errno=EMFILE;return NULL;
}
int fclose(FILE *f){f->used=0;--live;return 0;}
void rewind(FILE *f){f->position=0;f->error=0;}
int ferror(FILE *f){return f->error;}
int getc(FILE *f)
{
    if(fail_after>=0&&f->position>=(size_t)fail_after){errno=EIO;f->error=1;return EOF;}
    int c=f->buffer[f->position];if(!c)return EOF;
    ++f->position;return c;
}
static void set_database(const char *text)
{endpwent();database=text;open_error=0;fail_after=-1;}
int main(void)
{
    set_database("a::7:8:Text:/:/bin/sh\nb::9:10::/tmp:\n");
    struct passwd *p=getpwent();CHECK(p&&p->pw_uid==7&&live==1);
    char line[BUFSIZ];CHECK(getpw(9,line)==0&&live==1);
    CHECK(getpwent()->pw_uid==9);endpwent();CHECK(live==0);
    CHECK(getpwnam("a")->pw_gid==8&&live==0);
    set_database("a::7:8:Text:/:/bin/sh\n");fail_after=5;
    CHECK(getpwent()==NULL&&errno==EIO);endpwent();CHECK(live==0);
    strcpy(line,"unchanged");CHECK(getpw(7,line)!=0&&errno==EIO&&!strcmp(line,"unchanged")&&live==0);
    set_database("");open_error=EACCES;
    CHECK(getpwnam("a")==NULL&&errno==EACCES&&live==0);
    set_database("bad\nshort:x:1\ngood::12:3::/:\n");
    p=getpwent();CHECK(p&&!strcmp(p->pw_name,"good")&&p->pw_uid==12);endpwent();
    set_database("last::42:0::/:");
    CHECK(getpwent()->pw_uid==42);endpwent();
    CHECK(getpw(42,line)!=0&&live==0); /* getpw needs newline termination. */
    set_database("legacy:x:1x2:0::/:\n");
    CHECK(getpwent()->pw_uid==1);endpwent();CHECK(getpw(12,line)==0);
    set_database("overflow:x:999999999999999999999:0::/:\ngood::8:0::/:\n");
    CHECK(getpw(8,line)==0&&!strcmp(line,"good::8:0::/:"));
    char boundary[BUFSIZ+64];
    const char *prefix="edge::11:0::/:";size_t n=strlen(prefix);
    memcpy(boundary,prefix,n);memset(boundary+n,'x',BUFSIZ-1-n);
    boundary[BUFSIZ-1]='\n';boundary[BUFSIZ]=0;set_database(boundary);
    CHECK(getpwent()->pw_uid==11);endpwent();
    boundary[BUFSIZ-1]='x';strcpy(boundary+BUFSIZ,"\ngood::22:0::/:\n");set_database(boundary);
    CHECK(getpwent()->pw_uid==22);endpwent();
    CHECK(getpw(22,line)==0&&live==0);
    puts("PASS: V7 passwd parser boundaries, malformed records, I/O/permission errors, independent streams and legacy UID rules");
    return 0;
}
