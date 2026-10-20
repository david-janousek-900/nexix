#include <sys/kinfo.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <stdlib.h>
#include <nexix/libc.h>
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
static unsigned char *buffer;
#define CAPACITY 32784U
static struct nx_kinfo_header *header(void){return (void *)buffer;}
static void *records(void){return buffer+48;}
static int load(unsigned kind)
{
    ssize_t required=nx_kinfo(1,kind,NULL,0);CHECK(required>=48 && required<=32768);
    memset(buffer,0xa5,CAPACITY);
    ssize_t n=nx_kinfo(1,kind,buffer,CAPACITY);CHECK(n>=48 && n<=32768);
    struct nx_kinfo_header *h=header();
    CHECK(h->version==1 && h->kind==kind && h->header_size==48 && h->total_bytes==(uint64_t)n);
    CHECK(h->total_bytes==48+(uint64_t)h->count*h->record_size && h->tick_hz==100 && h->flags==NX_KINFO_RESIDENT && !h->reserved);
    CHECK(buffer[n]==0xa5);return 0;
}
int kinfo_test(void)
{
    int e;buffer=malloc(CAPACITY);CHECK(buffer);
    for(unsigned k=1;k<=7;++k){e=load(k);if(e)return e;}
    e=load(NX_KINFO_MEMORY);if(e)return e;
    const struct nx_kinfo_memory *m=records();
    CHECK(header()->count==1 && header()->record_size==32 && m->page_size==4096);
    CHECK(m->total_pages==m->free_pages+m->allocated_pages && m->total_pages && m->flags==NX_KINFO_RESIDENT);
    e=load(NX_KINFO_IO);if(e)return e;
    const struct nx_kinfo_io *io=records();
    CHECK(header()->count==1 && header()->record_size==96);
    CHECK(io->user_ticks+io->nice_ticks+io->system_ticks+io->idle_ticks==header()->ticks);
    struct nx_kinfo_io before=*io;
    e=load(NX_KINFO_IO);if(e)return e;io=records();
    CHECK(io->read_sectors==before.read_sectors && io->write_sectors==before.write_sectors);
    int fd=creat("/tmp/kinfo-file",0600);CHECK(fd>=0);
    CHECK(write(fd,"abcdef",6)==6 && lseek(fd,2,SEEK_SET)==2);
    int duplicate=dup(fd);CHECK(duplicate>=0);
    struct stat st;CHECK(fstat(fd,&st)==0);
    e=load(NX_KINFO_FILE);if(e)return e;
    const struct nx_kinfo_file *files=records();uint32_t fileid=0,inodeid=0;
    for(unsigned i=0;i<header()->count;++i)if(files[i].dev==st.st_dev && files[i].ino==st.st_ino){
        CHECK(files[i].refs==2 && files[i].offset==2 && files[i].kind==NX_KINFO_FILE_REGULAR);
        CHECK(!files[i].reserved && files[i].inode_id);fileid=files[i].slot;inodeid=files[i].inode_id;
    }
    CHECK(fileid && inodeid);
    e=load(NX_KINFO_INODE);if(e)return e;
    const struct nx_kinfo_inode *inodes=records();int found=0;
    for(unsigned i=0;i<header()->count;++i)if(inodes[i].slot==inodeid){
        CHECK(inodes[i].dev==st.st_dev && inodes[i].ino==st.st_ino && inodes[i].mode==st.st_mode);
        CHECK(inodes[i].size==6 && inodes[i].nlink==1 && !inodes[i].reserved16 && !inodes[i].reserved32);
        CHECK(!inodes[i].rdev);found=1;
    }
    CHECK(found);
    CHECK(link("/tmp/kinfo-file","/tmp/kinfo-alias")==0);
    e=load(NX_KINFO_INODE);if(e)return e;inodes=records();found=0;
    for(unsigned i=0;i<header()->count;++i)if(inodes[i].slot==inodeid){CHECK(inodes[i].nlink==2);found=1;}
    CHECK(found && unlink("/tmp/kinfo-alias")==0);
    int truncated=creat("/tmp/kinfo-file",0644);CHECK(truncated>=0 && close(truncated)==0);
    CHECK(chmod("/tmp/kinfo-file",0640)==0);
    e=load(NX_KINFO_INODE);if(e)return e;inodes=records();found=0;
    for(unsigned i=0;i<header()->count;++i)if(inodes[i].slot==inodeid){
        CHECK(inodes[i].nlink==1 && inodes[i].size==0 && (inodes[i].mode&0777)==0640);found=1;
    }
    CHECK(found);
    e=load(NX_KINFO_IO);if(e)return e;io=records();
    CHECK(io->write_sectors>before.write_sectors && io->cache_lookups>=before.cache_lookups && io->cache_hits<=io->cache_lookups);
    e=load(NX_KINFO_PROCESS);if(e)return e;
    const struct nx_kinfo_process *ps=records();uint32_t textid=0;found=0;
    for(unsigned i=0;i<header()->count;++i)if(ps[i].pid==getpid()){
        CHECK(ps[i].ppid==getppid() && ps[i].euid==geteuid() && ps[i].ruid==getuid());
        CHECK(ps[i].state==NX_KPROC_RUN && ps[i].size_bytes && !ps[i].reserved);
        CHECK(!ps[i].command[14] && !ps[i].command[15]);textid=ps[i].text_id;found=1;
    }
    CHECK(found);
    e=load(NX_KINFO_TEXT);if(e)return e;
    if(textid){const struct nx_kinfo_text *ts=records();found=0;
        for(unsigned i=0;i<header()->count;++i)if(ts[i].slot==textid){
            CHECK(ts[i].refs && ts[i].resident_refs==ts[i].refs && ts[i].size && !ts[i].reserved);found=1;
        }CHECK(found);
    }
    e=load(NX_KINFO_MOUNT);if(e)return e;
    const struct nx_kinfo_mount *mounts=records();found=0;
    for(unsigned i=0;i<header()->count;++i){
        CHECK(mounts[i].cache_used<=mounts[i].cache_capacity && mounts[i].fs_blocks<=mounts[i].device_blocks);
        CHECK(!mounts[i].reserved16 && !mounts[i].reserved32);
        if(mounts[i].flags&NX_KINFO_MOUNT_ROOT){CHECK(mounts[i].slot==1 && mounts[i].point_ino==2 && mounts[i].inodes);found=1;}
    }
    CHECK(found);
    ssize_t required=nx_kinfo(1,NX_KINFO_MEMORY,NULL,0);CHECK(required==80);
    memset(buffer,0xa5,CAPACITY);
    CHECK(nx_kinfo(1,NX_KINFO_MEMORY,buffer,required-1)==-1 && errno==ERANGE);
    CHECK(nx_kinfo(2,NX_KINFO_MEMORY,buffer,CAPACITY)==-1 && errno==EINVAL);
    CHECK(nx_kinfo(1,99,buffer,CAPACITY)==-1 && errno==EINVAL);
    CHECK(nx_syscall6(NX_SYS_KINFO,0x100000001L,1,(long)(uintptr_t)buffer,CAPACITY,0,0)==-EINVAL);
    CHECK(nx_kinfo(1,NX_KINFO_MEMORY,(void *)1,required)==-1 && errno==EFAULT);
    CHECK(nx_kinfo(1,NX_KINFO_MEMORY,(void *)(uintptr_t)kinfo_test,required)==-1 && errno==EFAULT);
    CHECK(nx_kinfo(1,NX_KINFO_MEMORY,NULL,required)==-1 && errno==EFAULT);
    for(unsigned i=0;i<CAPACITY;++i)CHECK(buffer[i]==0xa5);
    CHECK(close(duplicate)==0 && close(fd)==0 && unlink("/tmp/kinfo-file")==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        CHECK(setuid(123)==0);
        CHECK(nx_kinfo(1,NX_KINFO_PROCESS,buffer,CAPACITY)>0);
        for(unsigned k=2;k<=7;++k){
            CHECK(nx_kinfo(1,k,NULL,0)==-1 && errno==EPERM);
            CHECK(nx_kinfo(1,k,buffer,CAPACITY)==-1 && errno==EPERM);
        }
        _exit(37);
    }
    /* Observe a published zombie without consuming it. */
    found=0;
    for(unsigned tries=0;tries<1000 && !found;++tries){
        e=load(NX_KINFO_PROCESS);if(e)return e;ps=records();
        for(unsigned i=0;i<header()->count;++i)if(ps[i].pid==child && ps[i].state==NX_KPROC_ZOMBIE){
            CHECK(ps[i].ppid==getpid() && ps[i].status==(37<<8));found=1;
        }
        if(!found)CHECK(sched_yield()==0);
    }
    CHECK(found);int status;CHECK(wait(&status)==child && WIFEXITED(status) && WEXITSTATUS(status)==37);
    e=load(NX_KINFO_PROCESS);if(e)return e;ps=records();
    for(unsigned i=0;i<header()->count;++i)CHECK(ps[i].pid!=child);
    free(buffer);buffer=NULL;
    static const char marker[]="NEXIX_KINFO_OK\n";
    CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);return 0;
}
