#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"namespace line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
static long gate(long number)
{__asm__ volatile("int $0x81":"+a"(number)::"memory","cc");return number;}
static int same(const char *a,const char *b)
{struct stat x,y;return !stat(a,&x)&&!stat(b,&y)&&x.st_dev==y.st_dev&&x.st_ino==y.st_ino;}
static int covered(const char *path,int directory)
{
    char child[64];
    if(directory){
        if(mknod(path,S_IFDIR|0755,0))return 0;
        snprintf(child,sizeof(child),"%s/.",path);if(link(path,child))return 0;
        snprintf(child,sizeof(child),"%s/..",path);if(link("/tmp",child))return 0;
        snprintf(child,sizeof(child),"%s/hidden",path);
    }else snprintf(child,sizeof(child),"%s",path);
    int fd=open(child,O_WRONLY|O_CREAT|O_TRUNC,0644);
    return fd>=0&&write(fd,"covered",7)==7&&close(fd)==0;
}
int namespace_test(void)
{
    CHECK(gate(0x909)==0);
    const char *devices[]={"/tmp/md1","/tmp/md2","/tmp/md3","/tmp/md4","/tmp/md5","/tmp/md6","/tmp/md7"};
    const char *points[]={"/tmp/mnt","/tmp/ro","/tmp/m4","/tmp/m5","/tmp/m6","/tmp/m7"};
    for(unsigned i=0;i<7;++i)CHECK(mknod(devices[i],S_IFBLK|0600,i+1)==0);
    for(unsigned i=0;i<6;++i)CHECK(covered(points[i],i!=5));
    CHECK(mount("/dev/null","/tmp/mnt",0)==-1&&errno==ENOTBLK);
    CHECK(mount((void *)1,"/tmp/mnt",0)==-1&&errno==EFAULT);
    CHECK(mount(devices[0],(void *)1,0)==-1&&errno==EFAULT);
    CHECK(mount("/absent","/tmp/mnt",0)==-1&&errno==ENOENT);
    CHECK(mount(devices[0],"/absent",0)==-1&&errno==ENOENT);
    CHECK(mount("/dev/root","/tmp/mnt",0)==-1&&errno==EBUSY);
    CHECK(mount(devices[0],"/",0)==-1&&errno==EBUSY);
    CHECK(mount(devices[2],"/tmp/ro",0)==-1&&errno==EROFS);
    CHECK(mount(devices[3],"/tmp/m4",0)==-1&&errno==EIO);
    CHECK(mount(devices[4],"/tmp/m5",0)==-1&&errno==EIO);
    CHECK(gate(0x90a)==0); /* Failure paths released their driver handles. */
    int held=open("/tmp/mnt",O_RDONLY);CHECK(held>=0);
    CHECK(mount(devices[0],"/tmp/mnt",0)==-1&&errno==EBUSY);
    CHECK(close(held)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){if(setuid(123))_exit(99);int e=mount(devices[0],"/tmp/mnt",0);_exit(e==-1&&errno==EPERM?0:98);}
    int status;CHECK(wait(&status)==child&&status==0);
    int raw_held=open(devices[0],O_RDWR);CHECK(raw_held>=0);
    CHECK(mount(devices[0],"/tmp/mnt",0)==0);
    CHECK(write(raw_held,"x",1)==-1&&errno==EBUSY&&close(raw_held)==0);
    CHECK(mount(devices[0],"/tmp/m4",0)==-1&&errno==EBUSY);
    held=open("/tmp/mnt",O_RDONLY);CHECK(held>=0);
    CHECK(mount(devices[1],"/tmp/mnt",0)==-1&&errno==EBUSY&&close(held)==0);
    CHECK(unlink("/tmp/mnt")==-1&&errno==EBUSY);
    CHECK(same("/tmp/mnt/..","/tmp"));
    CHECK(same("/tmp/mnt/../..","/"));
    CHECK(mount(devices[1],"/tmp/mnt/inner",0)==0);
    CHECK(same("/tmp/mnt/inner/..","/tmp/mnt"));
    CHECK(same("/tmp/mnt/inner/../..","/tmp"));
    CHECK(mount(devices[2],"/tmp/ro",1)==0);
    CHECK(open(devices[0],O_WRONLY)==-1&&errno==EBUSY);
    CHECK(open("/tmp/ro/data",O_WRONLY)==-1&&errno==EROFS);
    CHECK(creat("/tmp/ro/new",0644)==-1&&errno==EROFS);
    CHECK(chmod("/tmp/ro/data",0600)==-1&&errno==EROFS);
    CHECK(unlink("/tmp/ro/data")==-1&&errno==EROFS);
    CHECK(access("/tmp/ro/data",W_OK)==-1&&errno==EROFS);
    CHECK(same("/tmp/ro/..","/tmp"));
    struct stat a,b;CHECK(stat("/tmp/mnt/data",&a)==0&&stat("/tmp/mnt/inner/data",&b)==0);
    CHECK(a.st_ino==b.st_ino&&a.st_dev==1&&b.st_dev==2);
    held=open("/etc",O_RDONLY);int fd=open("/tmp/mnt/data",O_RDWR);CHECK(held>=0&&fd>=0);
    CHECK(fstat(held,&a)==0&&fstat(fd,&b)==0&&a.st_ino==b.st_ino&&a.st_dev!=b.st_dev&&S_ISDIR(a.st_mode)&&S_ISREG(b.st_mode));
    CHECK(write(fd,"changed",7)==7&&close(fd)==0&&close(held)==0);
    char bytes[16]={0};fd=open("/tmp/mnt/inner/data",O_RDONLY);CHECK(fd>=0&&read(fd,bytes,7)==7&&!memcmp(bytes,"volume\n",7)&&close(fd)==0);
    fd=creat("/tmp/mnt/new",0644);CHECK(fd>=0&&close(fd)==0);
    CHECK(link("/tmp/mnt/new","/tmp/mnt/alias")==0);
    CHECK(link("/tmp/mnt/new","/tmp/root-alias")==-1&&errno==EXDEV);
    CHECK(link("/tmp/mnt/new","/tmp/mnt/inner/alias")==-1&&errno==EXDEV);
    CHECK(chmod("/tmp/mnt/new",0600)==0&&stat("/tmp/mnt/new",&a)==0&&a.st_dev==1&&(a.st_mode&0777)==0600);
    CHECK(unlink("/tmp/mnt/new")==0&&unlink("/tmp/mnt/alias")==0);
    CHECK(chdir("/tmp/mnt/inner")==0&&same("..","/tmp/mnt"));
    CHECK(chdir("../..") ==0&&same(".","/tmp")&&chdir("/")==0);
    child=fork();CHECK(child>=0);
    if(!child){
        if(chdir("/tmp")||chroot("/tmp/mnt"))_exit(97);
        if(stat("../etc/passwd",&a)||a.st_dev!=0)_exit(94); /* V7 keeps the old cwd. */
        if(chdir("/inner"))_exit(93);
        if(!same("../..","/")||!same("../../../../", "/"))_exit(96);
        if(stat("/etc/passwd",&a)!=-1||errno!=ENOENT)_exit(95);
        _exit(0);
    }
    CHECK(wait(&status)==child&&status==0);
    CHECK(chmod("/tmp/mnt",0700)==0);
    child=fork();CHECK(child>=0);
    if(!child){if(setuid(123))_exit(99);_exit(stat("/tmp/mnt/data",&a)==-1&&errno==EACCES?0:98);}
    CHECK(wait(&status)==child&&status==0&&chmod("/tmp/mnt",0755)==0);
    held=open("/dev",O_RDONLY);CHECK(held>=0); /* Root inode 5 must not exclude mounted executable inode 5. */
    child=fork();CHECK(child>=0);
    if(!child){execl("/tmp/mnt/echo","echo","NEXIX_MOUNT_EXEC_OK",(char *)0);_exit(99);}
    CHECK(wait(&status)==child&&status==0&&close(held)==0);
    child=fork();CHECK(child>=0);
    if(!child){if(chdir("/tmp/mnt"))_exit(98);raise(SIGQUIT);_exit(99);}
    CHECK(wait(&status)==child&&WIFSIGNALED(status)&&WCOREDUMP(status));
    CHECK(stat("/tmp/mnt/core",&a)==0&&a.st_dev==1&&unlink("/tmp/mnt/core")==0);
    CHECK(mount(devices[3],"/tmp/mnt",0)==0); /* V7 permits covering an unreferenced mounted root. */
    CHECK(stat("/tmp/mnt/data",&a)==0&&a.st_dev==4&&same("/tmp/mnt/..","/tmp"));
    for(unsigned i=4;i<7;++i)CHECK(mount(devices[i],points[i-1],0)==0);
    CHECK(mount(devices[0],"/tmp/mnt/a",0)==-1&&errno==EBUSY);
    fd=open("/tmp/mnt/data",O_RDONLY);CHECK(fd>=0&&gate(0x90b)==-EBUSY&&close(fd)==0);
    sync();CHECK(gate(0x90b)==0);
    for(unsigned i=0;i<6;++i){
        char child_path[64];
        if(i!=5)snprintf(child_path,sizeof(child_path),"%s/hidden",points[i]);
        else snprintf(child_path,sizeof(child_path),"%s",points[i]);
        memset(bytes,0,sizeof(bytes));fd=open(child_path,O_RDONLY);
        CHECK(fd>=0&&read(fd,bytes,7)==7&&!memcmp(bytes,"covered",7)&&close(fd)==0&&unlink(child_path)==0);
        if(i!=5){
            snprintf(child_path,sizeof(child_path),"%s/.",points[i]);CHECK(unlink(child_path)==0);
            snprintf(child_path,sizeof(child_path),"%s/..",points[i]);CHECK(unlink(child_path)==0);
            CHECK(unlink(points[i])==0);
        }
    }
    for(unsigned i=0;i<7;++i)CHECK(unlink(devices[i])==0);
    const char marker[]="NEXIX_NAMESPACE_OK\n";CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);
    return 0;
}
