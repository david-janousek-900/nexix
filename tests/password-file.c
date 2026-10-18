/* Inject publication failures around real, isolated host V7-style link steps. */
#define _DEFAULT_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../user/v7/password-file.h"
static char candidate[256],password[256],backup[256];
static unsigned faults,syncs;
int nx_password_chown(const char *path,uid_t uid,gid_t gid)
{assert(!strcmp(path,candidate) && !uid && !gid);return 0;}
void nx_password_sync(void){++syncs;}
int nx_password_link(const char *old,const char *new)
{
    if((faults&1) && !strcmp(old,candidate)) {errno=EIO;return -1;}
    if((faults&2) && !strcmp(old,backup)) {errno=EIO;return -1;}
    if((faults&4) && !strcmp(new,backup)) {errno=EEXIST;return -1;}
    return link(old,new);
}
int nx_password_unlink(const char *path)
{
    if((faults&8) && !strcmp(path,password)){errno=EIO;return -1;}
    return unlink(path);
}
static void save(const char *path,const char *data)
{int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0600);assert(fd>=0 && write(fd,data,strlen(data))==(ssize_t)strlen(data) && !close(fd));}
static void check(const char *path,const char *expected)
{char data[32]={0};int fd=open(path,O_RDONLY);assert(fd>=0 && read(fd,data,sizeof(data))==(ssize_t)strlen(expected) && !strcmp(data,expected) && !close(fd));}
int main(void)
{
    char base[]="/tmp/nexix-password-XXXXXX";assert(mkdtemp(base));
    snprintf(candidate,sizeof(candidate),"%s/ptmp",base);snprintf(password,sizeof(password),"%s/passwd",base);snprintf(backup,sizeof(backup),"%s/opasswd",base);
    const unsigned scenarios[]={0,1,3,4,8};
    for(unsigned n=0;n<sizeof(scenarios)/sizeof(*scenarios);++n){
        faults=scenarios[n];syncs=0;save(password,"old\n");save(candidate,"new\n");
        int result=v7_password_publish(candidate,password,backup);
        if(!faults){assert(!result && syncs>=3);check(password,"new\n");assert(access(backup,F_OK)<0);}
        else if(faults==3){assert(result==-2 && access(password,F_OK)<0);check(backup,"old\n");check(candidate,"new\n");assert(!link(backup,password));}
        else{assert(result==-1);check(password,"old\n");check(candidate,"new\n");assert(access(backup,F_OK)<0);}
        unlink(password);unlink(candidate);unlink(backup);
    }
    faults=0;save(password,"old\n");save(candidate,"new\n");save(backup,"existing\n");
    assert(v7_password_publish(candidate,password,backup)==-1);check(password,"old\n");check(backup,"existing\n");
    unlink(password);unlink(candidate);unlink(backup);
    assert(!rmdir(base));puts("PASS: password publication, exclusive backup, failed unlink/link, rollback and retained recovery copies");
}
