#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
static int write_database(const char *text)
{
    FILE *f=fopen("/etc/passwd","w");
    if(!f)return 0;
    int good=fputs(text,f)==0;
    return fclose(f)==0&&good;
}
int passwd_test(void)
{
    endpwent();endpwent();
    CHECK(getpwent()==NULL&&errno==ENOENT);
    CHECK(getpwnam("root")==NULL&&errno==ENOENT);
    char raw[BUFSIZ];strcpy(raw,"unchanged");
    CHECK(getpw(0,raw)!=0&&errno==ENOENT&&!strcmp(raw,"unchanged"));
    /* This disposable self-test filesystem has no /etc initially. */
    CHECK(mknod("/etc",S_IFDIR|0755,0)==0);
    CHECK(link("/etc","/etc/.")==0&&link("/","/etc/..")==0);
    const char *database="root::0:0:Root:/:/bin/sh\n"
                         "malformed\n"
                         "user:hash:123:456:User:/home/user:/bin/sh\n"
                         "user:other:124:457:Duplicate:/home/other:\n"
                         "max::65535:65535::/:\n"
                         "prefix:x:12junk:34junk:Text:/tmp:/bin/sh\n"
                         "last::99:1:Last:/tmp:";
    CHECK(write_database(database));
    struct passwd *p=getpwent();CHECK(p&&!strcmp(p->pw_name,"root")&&p->pw_uid==0&&p->pw_gid==0);
    CHECK(!strcmp(p->pw_passwd,"")&&p->pw_quota==0&&!strcmp(p->pw_comment,"")&&!strcmp(p->pw_gecos,"Root"));
    struct passwd *same=p;
    p=getpwent();CHECK(p==same&&!strcmp(p->pw_name,"user")&&p->pw_uid==123&&p->pw_gid==456);
    CHECK(!strcmp(p->pw_dir,"/home/user")&&!strcmp(p->pw_shell,"/bin/sh"));
    p=getpwent();CHECK(p&&p->pw_uid==124&&!strcmp(p->pw_shell,""));
    p=getpwent();CHECK(p&&p->pw_uid==65535&&p->pw_gid==65535);
    p=getpwent();CHECK(p&&p->pw_uid==12&&p->pw_gid==34);
    p=getpwent();CHECK(p&&p->pw_uid==99&&!strcmp(p->pw_shell,""));
    CHECK(getpwent()==NULL&&getpwent()==NULL);
    setpwent();CHECK(getpwent()->pw_uid==0);endpwent();
    CHECK(getpwent()->pw_uid==0); /* Close/reopen starts at the first record. */
    CHECK(getpw(123,raw)==0&&!strcmp(raw,"user:hash:123:456:User:/home/user:/bin/sh"));
    CHECK(getpwent()->pw_uid==123); /* getpw uses an independent stream. */
    p=getpwnam("user");CHECK(p&&p->pw_uid==123&&p==same);
    CHECK(!strcmp(p->pw_name,"user")); /* Result survives iterator close. */
    CHECK(getpwent()->pw_uid==0); /* Lookup resets/closes the shared iterator. */
    p=getpwuid(65535);CHECK(p&&p->pw_gid==65535);
    CHECK(getpw(65535,raw)==0&&!strcmp(raw,"max::65535:65535::/:"));
    CHECK(getpwnam("missing")==NULL&&getpwuid(777)==NULL);
    CHECK(getpwnam(NULL)==NULL&&errno==EINVAL);
    strcpy(raw,"unchanged");CHECK(getpw(99,raw)!=0&&!strcmp(raw,"unchanged"));
    CHECK(getpw(0,NULL)!=0&&errno==EINVAL);
    endpwent();
    CHECK(write_database("partial:x:1\nlegacy:x:1x2:0:Legacy:/:/bin/sh\n"));
    p=getpwent();CHECK(p&&!strcmp(p->pw_name,"legacy")&&p->pw_uid==1);endpwent();
    CHECK(getpw(12,raw)==0&&!strcmp(raw,"legacy:x:1x2:0:Legacy:/:/bin/sh"));
    /* Oversized record fragments must not become invented accounts. */
    FILE *f=fopen("/etc/passwd","w");CHECK(f);
    for(int i=0;i<BUFSIZ*3;++i)CHECK(putc('x',f)=='x');
    CHECK(fputs(":0:0:0:/:/bin/sh\ngood::42:7:Good:/tmp:\n",f)==0&&fclose(f)==0);
    CHECK(getpwent()->pw_uid==42&&getpwent()==NULL);endpwent();
    CHECK(getpw(42,raw)==0&&!strcmp(raw,"good::42:7:Good:/tmp:"));
    for(int i=0;i<30;++i){CHECK(getpwnam("good")!=NULL&&getpw(42,raw)==0);}
    /* A real credential failure must not be mistaken for EOF. */
    CHECK(chmod("/etc/passwd",0600)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        endpwent();if(setuid(123))_exit(1);
        if(getpwent()!=NULL||errno!=EACCES)_exit(2);
        if(getpw(42,raw)==0||errno!=EACCES)_exit(3);
        _exit(0);
    }
    int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    endpwent();CHECK(unlink("/etc/passwd")==0);
    CHECK(unlink("/etc/..") ==0&&unlink("/etc/.")==0&&unlink("/etc")==0);
    return 0;
}
