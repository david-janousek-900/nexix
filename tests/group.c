#include <grp.h>
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
    if(strcmp(path,"/etc/group")||strcmp(mode,"r")){errno=EINVAL;return NULL;}
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
{endgrent();database=text;open_error=0;fail_after=-1;}
int main(void)
{
    set_database("staff:hash:12:alice,bob\nempty::13:\nodd::14:,a,,b,\n");
    struct group *p=getgrent();CHECK(p&&p->gr_gid==12&&!strcmp(p->gr_passwd,"hash"));
    CHECK(!strcmp(p->gr_mem[0],"alice")&&!strcmp(p->gr_mem[1],"bob")&&!p->gr_mem[2]);
    struct group *same=p;
    p=getgrent();CHECK(p==same&&p->gr_gid==13&&!p->gr_mem[0]);
    p=getgrent();CHECK(p&&p->gr_gid==14&&!strcmp(p->gr_mem[0],"")&&!strcmp(p->gr_mem[1],"a"));
    CHECK(!strcmp(p->gr_mem[2],"")&&!strcmp(p->gr_mem[3],"b")&&!p->gr_mem[4]);
    CHECK(getgrent()==NULL);setgrent();CHECK(getgrent()->gr_gid==12);endgrent();CHECK(live==0);
    CHECK(getgrnam("staff")->gr_gid==12&&live==0);
    CHECK(getgrgid(13)->gr_mem[0]==NULL&&live==0);
    set_database("bad\nshort:x:1\nvalid::65535: a,b:c\nfinal::22:a");
    p=getgrent();CHECK(p&&p->gr_gid==65535&&!strcmp(p->gr_mem[0]," a")&&!strcmp(p->gr_mem[1],"b:c"));
    CHECK(getgrent()->gr_gid==22&&getgrent()==NULL);endgrent();
    set_database("broken::7:alice,bob\n");fail_after=8;
    CHECK(getgrent()==NULL&&errno==EIO);endgrent();CHECK(live==0);
    set_database("");open_error=EACCES;
    CHECK(getgrnam("x")==NULL&&errno==EACCES&&live==0);
    char many[512];strcpy(many,"many::42:");size_t n=strlen(many);
    for(int i=0;i<99;++i){many[n++]='a';many[n++]=',';}
    many[n++]='\n';many[n]=0;set_database(many);
    p=getgrent();CHECK(p&&p->gr_gid==42&&!p->gr_mem[99]);endgrent();
    many[n-1]='a';strcpy(many+n,"\ngood::7:a\n");set_database(many);
    CHECK(getgrent()->gr_gid==7);endgrent();
    char boundary[BUFSIZ+64];strcpy(boundary,"edge::1:");n=strlen(boundary);
    memset(boundary+n,'x',BUFSIZ-1-n);boundary[BUFSIZ-1]='\n';boundary[BUFSIZ]=0;
    set_database(boundary);CHECK(getgrent()->gr_gid==1);endgrent();
    boundary[BUFSIZ-1]='x';strcpy(boundary+BUFSIZ,"\ngood::2:a\n");set_database(boundary);
    CHECK(getgrent()->gr_gid==2);endgrent();
    CHECK(getgrnam(NULL)==NULL&&errno==EINVAL&&live==0);
    puts("PASS: V7 group fields, member delimiters, EOF, malformed/I/O failures, 99-member and record-size boundaries");
    return 0;
}
