#include <pwd.h>
#include <grp.h>
#include <crypt.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#define CHECK(c) do{if(!(c)){printf("ACCOUNT_CONFIG_ERROR %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    const char *names[]={"root","alice","bob"},*passwords[]={"nexix","alice","bob"};
    const char *homes[]={"/root","/home/alice","/home/bob"};uint16_t ids[]={0,100,101};
    struct stat s;
    for(unsigned n=0;n<3;++n){
        struct passwd *p=getpwnam(names[n]);
        CHECK(p && p->pw_uid==ids[n] && p->pw_gid==(n?100:0) && !strcmp(p->pw_dir,homes[n]) && !strcmp(p->pw_shell,"/bin/sh"));
        CHECK(!strcmp(crypt(passwords[n],p->pw_passwd),p->pw_passwd));
        CHECK(stat(homes[n],&s)==0 && S_ISDIR(s.st_mode) && s.st_uid==ids[n] && s.st_gid==(n?100:0) && (s.st_mode&0777)==0700);
    }
    CHECK(getpwnam("daemon") && !strcmp(getpwnam("daemon")->pw_passwd,"x"));endpwent();
    struct group *g=getgrnam("staff");CHECK(g && g->gr_gid==100 && !strcmp(g->gr_mem[0],"root") && !strcmp(g->gr_mem[1],"alice") && !strcmp(g->gr_mem[2],"bob") && !g->gr_mem[3]);endgrent();
    const char *config[]={"/etc/passwd","/etc/group","/etc/ttys","/etc/rc","/etc/motd"};
    for(unsigned n=0;n<5;++n)CHECK(!stat(config[n],&s) && S_ISREG(s.st_mode) && (s.st_mode&0777)==0644 && !s.st_uid && !s.st_gid);
    CHECK(!stat("/tmp",&s) && s.st_mode==(S_IFDIR|0777));
    for(unsigned n=1;n<3;++n){
        pid_t pid=fork();CHECK(pid>=0);
        if(!pid){
            if(setgid(100) || setuid(ids[n]) || getuid()!=ids[n] || geteuid()!=ids[n] || getgid()!=100 || getegid()!=100 || chdir(homes[n]))_exit(2);
            int fd=creat("owned",0640);if(fd<0 || fstat(fd,&s) || s.st_uid!=ids[n] || s.st_gid!=100 || close(fd) || unlink("owned"))_exit(3);
            if(access(homes[n==1?2:1],R_OK|X_OK)!=-1 || errno!=EACCES)_exit(4);
            if(creat("/etc/forbidden",0600)!=-1 || errno!=EACCES)_exit(5);
            fd=open("/etc/passwd",O_RDONLY);if(fd<0 || close(fd))_exit(6);
            fd=open(".profile",O_RDONLY);if(fd<0 || close(fd))_exit(7);
            fd=creat("/tmp/account-temp",0600);if(fd<0 || close(fd) || unlink("/tmp/account-temp"))_exit(8);
            _exit(0);
        }
        int status;CHECK(wait(&status)==pid && status==0);
    }
    puts("ACCOUNT_CONFIG_OK");return 0;
}
