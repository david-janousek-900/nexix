#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"umount line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
struct held_child {pid_t pid;int ready,go;};
static long gate(long number,unsigned argument)
{__asm__ volatile("int $0x81":"+a"(number):"D"((uintptr_t)argument):"memory","cc");return number;}
static int point(const char *path)
{
    char part[64];if(mknod(path,S_IFDIR|0755,0))return 0;
    snprintf(part,sizeof(part),"%s/.",path);if(link(path,part))return 0;
    snprintf(part,sizeof(part),"%s/..",path);if(link("/tmp",part))return 0;
    snprintf(part,sizeof(part),"%s/hidden",path);
    int fd=creat(part,0644);return fd>=0&&write(fd,"hidden",6)==6&&close(fd)==0;
}
static int remove_point(const char *path)
{
    char part[64];snprintf(part,sizeof(part),"%s/hidden",path);if(unlink(part))return 0;
    snprintf(part,sizeof(part),"%s/.",path);if(unlink(part))return 0;
    snprintf(part,sizeof(part),"%s/..",path);return !unlink(part)&&!unlink(path);
}
static int start_text(const char *path,struct held_child *held)
{
    int ready[2],go[2];CHECK(pipe(ready)==0&&pipe(go)==0);
    char r[12],g[12];snprintf(r,sizeof(r),"%d",ready[1]);snprintf(g,sizeof(g),"%d",go[0]);
    pid_t child=fork();CHECK(child>=0);
    if(!child){close(ready[0]);close(go[1]);execl(path,"text","hold",r,g,(char *)0);_exit(99);}
    CHECK(close(ready[1])==0&&close(go[0])==0);
    *held=(struct held_child){child,ready[0],go[1]};
    char byte;CHECK(read(held->ready,&byte,1)==1&&byte=='R');return 0;
}
static int finish(struct held_child *held)
{
    int status;CHECK(write(held->go,"G",1)==1&&close(held->go)==0&&close(held->ready)==0);
    CHECK(wait(&status)==held->pid&&status==0);return 0;
}
static int process_reference(int mode)
{
    int fd=-1;if(mode==3){fd=open("/tmp/uu1/data",O_RDONLY);CHECK(fd>=0);}
    int ready[2],go[2];CHECK(pipe(ready)==0&&pipe(go)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        close(ready[0]);close(go[1]);
        if(mode==1&&chdir("/tmp/uu1"))_exit(97);
        if(mode==2&&chroot("/tmp/uu1"))_exit(96);
        char byte;_exit(write(ready[1],"R",1)==1&&read(go[0],&byte,1)==1?0:95);
    }
    CHECK(close(ready[1])==0&&close(go[0])==0);
    if(fd>=0)CHECK(close(fd)==0); /* Only the child's inherited descriptor remains. */
    char byte;CHECK(read(ready[0],&byte,1)==1&&byte=='R');
    CHECK(umount("/tmp/ud1")==-1&&errno==EBUSY);
    struct held_child held={child,ready[0],go[1]};return finish(&held);
}
int umount_test(void)
{
    CHECK(gate(0x909,0)==0&&gate(0x90a,0)==0);
    const char *devs[]={"/tmp/ud1","/tmp/ud2","/tmp/ud3"};
    const char *points[]={"/tmp/uu1","/tmp/uu2","/tmp/uu3"};
    for(unsigned i=0;i<3;++i){CHECK(mknod(devs[i],S_IFBLK|0600,i+1)==0);CHECK(point(points[i]));}
    CHECK(mknod("/tmp/ua1",S_IFBLK|0600,1)==0);
    CHECK(mknod("/tmp/ubad",S_IFBLK|0600,0x100)==0);
    CHECK(umount((void *)1)==-1&&errno==EFAULT);
    CHECK(umount("/absent")==-1&&errno==ENOENT);
    CHECK(umount("/dev/null")==-1&&errno==ENOTBLK);
    CHECK(umount("/tmp/ubad")==-1&&errno==ENXIO);
    CHECK(umount(devs[0])==-1&&errno==EINVAL);
    CHECK(umount("/dev/root")==-1&&errno==EBUSY);
    pid_t child=fork();CHECK(child>=0);
    if(!child){if(setuid(123))_exit(99);_exit(umount(devs[0])==-1&&errno==EPERM?0:98);}
    int status;CHECK(wait(&status)==child&&status==0);
    CHECK(mount(devs[0],points[0],0)==0);
    int fd=open("/tmp/uu1/data",O_RDONLY),duplicate=dup(fd);CHECK(fd>=0&&duplicate>=0);
    CHECK(umount(devs[0])==-1&&errno==EBUSY&&close(fd)==0);
    CHECK(umount(devs[0])==-1&&errno==EBUSY&&close(duplicate)==0);
    CHECK(chdir(points[0])==0&&umount(devs[0])==-1&&errno==EBUSY&&chdir("/")==0);
    for(int mode=1;mode<=3;++mode){int error=process_reference(mode);if(error)return error;}
    fd=creat("/tmp/uu1/orphan",0600);CHECK(fd>=0&&write(fd,"orphan",6)==6&&unlink("/tmp/uu1/orphan")==0);
    CHECK(umount(devs[0])==-1&&errno==EBUSY&&close(fd)==0);
    CHECK(mount(devs[1],"/tmp/uu1/inner",0)==0);
    CHECK(umount(devs[0])==-1&&errno==EBUSY&&umount(devs[1])==0);
    CHECK(mount(devs[1],points[0],0)==0); /* Overlay blocks unmount of the covered volume. */
    CHECK(umount(devs[0])==-1&&errno==EBUSY&&umount(devs[1])==0);
    CHECK(mount(devs[1],points[1],0)==0);
    CHECK(chmod("/tmp/uu1/probe",01755)==0&&chmod("/tmp/uu2/probe",01755)==0);
    struct held_child first,second;int error=start_text("/tmp/uu2/probe",&second);if(error)return error;
    error=finish(&second);if(error)return error; /* Inactive sticky ownership on device 2. */
    error=start_text("/tmp/uu1/probe",&first);if(error)return error;
    CHECK(umount(devs[0])==-1&&errno==EBUSY);
    fd=open("/tmp/uu2/probe",O_RDONLY);CHECK(fd>=0);
    error=start_text("/tmp/uu2/probe",&second);if(error)return error; /* Purging device 1 must preserve device 2's sticky entry. */
    error=finish(&second);if(error)return error;CHECK(close(fd)==0);
    CHECK(umount(devs[1])==0); /* Purges idle sticky text while device 1's text is active. */
    CHECK(umount(devs[0])==-1&&errno==EBUSY);
    error=finish(&first);if(error)return error;
    fd=open("/tmp/uu1/data",O_WRONLY|O_TRUNC);CHECK(fd>=0&&write(fd,"saved!",6)==6&&close(fd)==0);
    CHECK(umount("/tmp/ua1")==0); /* Device identity, not the pathname spelling. */
    CHECK(umount(devs[0])==-1&&errno==EINVAL);
    char bytes[8]={0};fd=open("/tmp/uu1/hidden",O_RDONLY);CHECK(fd>=0&&read(fd,bytes,6)==6&&!memcmp(bytes,"hidden",6)&&close(fd)==0);
    CHECK(mount(devs[0],points[0],0)==0);
    fd=open("/tmp/uu1/data",O_RDONLY);CHECK(fd>=0&&read(fd,bytes,6)==6&&!memcmp(bytes,"saved!",6)&&close(fd)==0);
    CHECK(mount(devs[1],points[1],0)==0);
    CHECK(gate(0x90c,1)==0); /* An unrelated volume's superblock failure retains the target. */
    CHECK(umount(devs[1])==-1&&errno==EIO);
    CHECK(open(devs[1],O_WRONLY)==-1&&errno==EBUSY);
    CHECK(gate(0x90c,0)==0&&gate(0x90d,1)==0);
    CHECK(gate(0x90c,2)==0&&umount(devs[1])==-1&&errno==EIO);
    CHECK(open(devs[1],O_WRONLY)==-1&&errno==EBUSY);
    CHECK(gate(0x90c,0)==0&&gate(0x90d,2)==0);
    struct stat st;CHECK(stat(points[1],&st)==0&&st.st_dev==2&&umount(devs[1])==0);
    CHECK(mknod("/tmp/uu1/localdev",S_IFBLK|0600,1)==0);
    CHECK(umount("/tmp/uu1/localdev")==0); /* The device lookup must not retain a target inode. */
    CHECK(mount(devs[0],points[0],0)==0);
    CHECK(unlink("/tmp/uu1/localdev")==0&&umount(devs[0])==0);
    int raw=open(devs[0],O_RDWR);CHECK(raw>=0);
    CHECK(mount(devs[0],points[0],0)==0&&write(raw,bytes,2)==-1&&errno==EBUSY);
    CHECK(umount(devs[0])==0&&lseek(raw,0,SEEK_SET)==0&&write(raw,bytes,2)==2&&close(raw)==0);
    CHECK(mount(devs[0],points[0],0)==0); /* Boot-sector-only raw change does not invalidate the filesystem. */
    CHECK(mount(devs[2],points[2],1)==0&&umount(devs[0])==0);
    CHECK(mount(devs[1],"/tmp/uu3/inner",0)==0); /* Child reuses a lower-numbered mount slot. */
    CHECK(umount(devs[2])==-1&&errno==EBUSY&&umount(devs[1])==0&&umount(devs[2])==0);
    CHECK(open(devs[2],O_WRONLY)==-1&&errno==EROFS);
    for(unsigned i=0;i<3;++i){CHECK(remove_point(points[i]));CHECK(unlink(devs[i])==0);}
    CHECK(unlink("/tmp/ua1")==0&&unlink("/tmp/ubad")==0);
    const char marker[]="NEXIX_UMOUNT_OK\n";CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);
    return 0;
}
