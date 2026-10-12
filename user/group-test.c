#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
static int database(const char *path,const char *text)
{FILE *f=fopen(path,"w");if(!f)return 0;int good=fputs(text,f)==0;return fclose(f)==0&&good;}
int group_test(void)
{
    endgrent();endgrent();CHECK(getgrent()==NULL&&errno==ENOENT);
    CHECK(getgrnam("root")==NULL&&errno==ENOENT);
    CHECK(mknod("/etc",S_IFDIR|0755,0)==0&&link("/etc","/etc/.")==0&&link("/","/etc/..")==0);
    CHECK(database("/etc/group","staff:hash:12:alice,bob\nbad\nstaff::13:carol\n"
                                "alias::12:duplicate\nempty::14:\nmax::65535:root\n"
                                "prefix::15junk:,a,,b,\nlast::16:z"));
    CHECK(database("/etc/passwd","one::42:12:One:/:\ntwo::43:13:Two:/:\n"));
    CHECK(getpwent()->pw_uid==42);
    struct group *p=getgrent();CHECK(p&&!strcmp(p->gr_name,"staff")&&p->gr_gid==12);
    CHECK(!strcmp(p->gr_passwd,"hash")&&!strcmp(p->gr_mem[0],"alice")&&!strcmp(p->gr_mem[1],"bob")&&!p->gr_mem[2]);
    struct group *same=p;
    CHECK(getpwent()->pw_uid==43);endpwent();CHECK(!strcmp(p->gr_name,"staff"));
    p=getgrent();CHECK(p==same&&p->gr_gid==13);
    CHECK(getgrent()->gr_gid==12);
    p=getgrent();CHECK(p&&p->gr_gid==14&&!p->gr_mem[0]);
    CHECK(getgrent()->gr_gid==65535);
    p=getgrent();CHECK(p&&p->gr_gid==15&&!strcmp(p->gr_mem[0],"")&&!strcmp(p->gr_mem[1],"a"));
    CHECK(!strcmp(p->gr_mem[2],"")&&!strcmp(p->gr_mem[3],"b")&&!p->gr_mem[4]);
    CHECK(getgrent()->gr_gid==16&&getgrent()==NULL&&getgrent()==NULL);
    setgrent();CHECK(getgrent()->gr_gid==12);endgrent();CHECK(getgrent()->gr_gid==12);
    p=getgrnam("staff");CHECK(p==same&&p->gr_gid==12&&!strcmp(p->gr_mem[1],"bob"));
    CHECK(getgrent()->gr_gid==12);p=getgrgid(12);CHECK(p&&!strcmp(p->gr_name,"staff"));
    CHECK(getgrgid(65535)->gr_gid==65535);
    CHECK(getgrnam("missing")==NULL&&getgrgid(77)==NULL);
    CHECK(getgrnam(NULL)==NULL&&errno==EINVAL);endgrent();
    FILE *f=fopen("/etc/group","w");CHECK(f);
    CHECK(fputs("too-many::1:",f)==0);
    for(int i=0;i<100;++i)CHECK(fputs("a,",f)==0);
    CHECK(fputs("\n",f)==0);
    for(int i=0;i<BUFSIZ*2;++i)CHECK(putc('x',f)=='x');
    CHECK(fputs("\ngood::42:a\n",f)==0&&fclose(f)==0);
    CHECK(getgrent()->gr_gid==42&&getgrent()==NULL);endgrent();
    for(int i=0;i<30;++i)CHECK(getgrnam("good")&&getgrgid(42));
    CHECK(chmod("/etc/group",0600)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){endgrent();if(setuid(123))_exit(1);_exit(getgrent()==NULL&&errno==EACCES?0:2);}
    int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    endgrent();endpwent();CHECK(unlink("/etc/group")==0&&unlink("/etc/passwd")==0);
    CHECK(unlink("/etc/..")==0&&unlink("/etc/.")==0&&unlink("/etc")==0);
    return 0;
}
