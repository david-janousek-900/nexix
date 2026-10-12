#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <nexix/core.h>
#include <nexix/fpu.h>
#include <errno.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"core test line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
extern _Noreturn void core_fault(void);
extern const unsigned char core_fault_site[],core_fp_pattern[];
static unsigned char data_tag[24]="NEXIX_CORE_DATA";
static void caught(int n){(void)n;}
static long gate(long n,uintptr_t arg)
{__asm__ volatile("int $0x81":"+a"(n):"D"(arg):"memory","cc");return n;}
static int child_status(pid_t child,int signal,int dumped)
{
    int status;CHECK(wait(&status)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==signal&&WCOREDUMP(status)==dumped);
    return 0;
}
static int death(int sig,int dump,int unprivileged,int full)
{
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        if(unprivileged&&setuid(123))_exit(98);
        if(full)while(open("/dev/null",O_RDONLY)>=0){}
        raise(sig);_exit(99);
    }
    return child_status(child,sig,dump);
}
static int read_at(int fd,off_t at,void *data,size_t size)
{return lseek(fd,at,SEEK_SET)==at&&read(fd,data,size)==(ssize_t)size;}
static int payload(int fd,const struct nx_core_program *ph,const void *address,const void *expected,size_t size)
{
    uintptr_t va=(uintptr_t)address;unsigned char bytes[32];
    if(va<ph->vaddr||size>ph->memsz||va-ph->vaddr>ph->memsz-size)return 0;
    return size<=sizeof(bytes)&&read_at(fd,ph->offset+va-ph->vaddr,bytes,size)&&!memcmp(bytes,expected,size);
}
static int inspect(pid_t pid,int sig,const void *stack,const void *heap,int fault)
{
    int fd=open("core",O_RDONLY);CHECK(fd>=0);
    struct stat st;CHECK(fstat(fd,&st)==0&&S_ISREG(st.st_mode));
    struct nx_core_elf_header header;CHECK(read_at(fd,0,&header,sizeof(header)));
    CHECK(!memcmp(header.ident,"\177ELF\002\001\001",7)&&header.type==4&&header.machine==62&&header.version==1);
    CHECK(header.ehsize==64&&header.phoff==64&&header.phentsize==56&&header.phnum>1&&header.phnum<=NX_CORE_MAX_LOADS+1);
    struct nx_core_program ph;
    CHECK(read_at(fd,header.phoff,&ph,sizeof(ph))&&ph.type==4&&ph.filesz<=1024);
    uint64_t cursor=ph.offset,end=cursor+ph.filesz;
    int seen=0;struct nx_core_registers regs={0};
    while(cursor<end){
        struct nx_core_note note;char name[8];
        CHECK(read_at(fd,cursor,&note,sizeof(note))&&note.namesz==6);cursor+=12;
        CHECK(read_at(fd,cursor,name,sizeof(name))&&!memcmp(name,"NEXIX\0\0\0",8));cursor+=8;
        CHECK(cursor+note.descsz<=end);
        if(note.type==NX_CORE_INFO){
            struct nx_core_info info;CHECK(note.descsz==sizeof(info)&&read_at(fd,cursor,&info,sizeof(info)));
            CHECK(info.version==1&&info.signal==(unsigned)sig&&info.pid==pid&&info.ppid==getpid()&&info.time>0);
            if(fault)CHECK(info.ruid==0&&info.euid==0&&info.rgid==0&&info.egid==0);
            else CHECK(info.ruid==123&&info.euid==123&&info.rgid==456&&info.egid==456&&!strcmp(info.command,"core-probe"));
            seen|=1;
        }else if(note.type==NX_CORE_REGISTERS){
            CHECK(note.descsz==sizeof(regs)&&read_at(fd,cursor,&regs,sizeof(regs)));
            CHECK(regs.cs==0x23&&regs.ss==0x1b&&(regs.rflags&0x202)==0x202);
            if(fault)CHECK(regs.rip==(uintptr_t)core_fault_site&&regs.r12==0x123456789abcdef0UL&&regs.rax==0x700000000000UL&&regs.vector==14);
            seen|=2;
        }else if(note.type==NX_CORE_FPU){
            struct nx_fpu_state fp;CHECK(note.descsz==sizeof(fp)&&read_at(fd,cursor,&fp,sizeof(fp)));
            if(fault)CHECK(!memcmp(fp.bytes+160,core_fp_pattern,16));
            for(unsigned i=416;i<512;++i)CHECK(fp.bytes[i]==0);
            seen|=4;
        }else CHECK(0);
        cursor+=(note.descsz+3)&~3;
    }
    CHECK(cursor==end&&seen==7);
    int data=0,stacked=0,heaped=0,sp=0;
    for(unsigned i=1;i<header.phnum;++i){
        CHECK(read_at(fd,header.phoff+i*56,&ph,sizeof(ph))&&ph.type==1&&(ph.flags&4)&&!(ph.flags&1));
        CHECK(ph.paddr==0&&ph.filesz==ph.memsz&&ph.align==4096&&ph.offset%4096==ph.vaddr%4096&&ph.offset+ph.filesz<=(uint64_t)st.st_size);
        CHECK(ph.vaddr>=0x40000000UL&&ph.vaddr+ph.memsz<=0x800000000000UL);
        CHECK(regs.rip<ph.vaddr||regs.rip>=ph.vaddr+ph.memsz);
        if(fault){data|=payload(fd,&ph,data_tag,data_tag,sizeof(data_tag));stacked|=payload(fd,&ph,stack,stack,24);heaped|=payload(fd,&ph,heap,heap,24);}
        if(regs.rsp>=ph.vaddr&&regs.rsp<ph.vaddr+ph.memsz)sp=1;
    }
    CHECK(sp&&(!fault||(data&&stacked&&heaped))&&close(fd)==0);return 0;
}
static int sentinel(void)
{
    int fd=creat("core",0666);CHECK(fd>=0&&write(fd,"unchanged",9)==9&&close(fd)==0);return 0;
}
static int preserved(void)
{char text[10];int fd=open("core",O_RDONLY);CHECK(fd>=0&&read(fd,text,sizeof(text))==9&&!memcmp(text,"unchanged",9)&&close(fd)==0);return 0;}
int core_test(void)
{
    CHECK(mknod("/tmp/core-dir",S_IFDIR|0777,0)==0&&link("/tmp/core-dir","/tmp/core-dir/.")==0&&link("/tmp","/tmp/core-dir/..")==0&&chdir("/tmp/core-dir")==0);
    mode_t mask=umask(027);int error;
    if((error=death(SIGTERM,0,0,0)))return error;
    CHECK(access("core",F_OK)==-1&&errno==ENOENT);
    for(int pass=0;pass<2;++pass){
        pid_t child=fork();CHECK(child>=0);
        if(!child){signal(SIGSEGV,pass?SIG_IGN:caught);raise(SIGSEGV);_exit(0);}
        int status;CHECK(wait(&status)==child&&status==0&&access("core",F_OK)==-1&&errno==ENOENT);
    }
    volatile unsigned char stack_tag[24];memcpy((void *)stack_tag,"NEXIX_CORE_STACK",16);
    memset((void *)(stack_tag+16),0,8);
    unsigned char *heap=sbrk(4096);CHECK(heap!=(void *)-1);memcpy(heap,"NEXIX_CORE_HEAP",16);memset(heap+16,0,8);
    pid_t child=fork();CHECK(child>=0);if(!child)core_fault();
    if((error=child_status(child,SIGSEGV,1)))return error;
    if((error=inspect(child,SIGSEGV,(const void *)stack_tag,heap,1)))return error;
    struct stat st;CHECK(stat("core",&st)==0&&(st.st_mode&0777)==0640&&st.st_uid==0&&st.st_gid==0);
    CHECK(unlink("core")==0&&sbrk(-4096)==heap+4096);
    const int signals[]={SIGQUIT,SIGILL,SIGTRAP,SIGABRT,SIGEMT,SIGFPE,SIGBUS,SIGSEGV,SIGSYS};
    for(unsigned i=0;i<sizeof(signals)/sizeof(signals[0]);++i){
        if((error=death(signals[i],1,0,0)))return error;
        CHECK(unlink("core")==0);
    }
    if((error=death(SIGABRT,1,0,1)))return error;
    CHECK(unlink("core")==0);
    CHECK(mknod("core",S_IFDIR|0755,0)==0&&link("core","core/.")==0&&link(".","core/..")==0);
    if((error=death(SIGABRT,0,0,0)))return error;
    CHECK(stat("core",&st)==0&&S_ISDIR(st.st_mode)&&unlink("core/..")==0&&unlink("core/.")==0&&unlink("core")==0);
    if((error=sentinel()))return error;
    CHECK(chmod("core",0400)==0);
    if((error=death(SIGABRT,0,1,0)))return error;
    if((error=preserved()))return error;
    CHECK(gate(0x908,1)==0);
    if((error=death(SIGABRT,0,0,0)))return error;
    CHECK(gate(0x908,0)==0);if((error=preserved()))return error;
    CHECK(chmod("core",0666)==0);
    int fill=creat("fill",0600);CHECK(fill>=0);char bytes[512]={0};
    while(write(fill,bytes,sizeof(bytes))==(ssize_t)sizeof(bytes)){}
    CHECK(errno==ENOSPC&&close(fill)==0);
    if((error=death(SIGABRT,0,0,0)))return error;
    CHECK(unlink("fill")==0&&unlink("core")==0);
    /* Install a real executable only after larger direct-fork dumps are removed. */
    CHECK(gate(0x907,0)==0);
    CHECK(link("core-probe","core")==0);child=fork();CHECK(child>=0);
    if(!child){execl("core-probe","core-probe",(char *)NULL);_exit(99);}
    if((error=child_status(child,SIGABRT,0)))return error;
    CHECK(stat("core",&st)==0&&st.st_size>64&&unlink("core")==0);
    if((error=sentinel()))return error;
    CHECK(chmod("core",0666)==0);
    for(int pass=0;pass<3;++pass){
        CHECK(chown("core-probe",pass==2?0:789,321)==0&&chmod("core-probe",pass==1?02755:04755)==0);
        child=fork();CHECK(child>=0);
        if(!child){if(setgid(456)||setuid(123))_exit(98);execl("core-probe","core-probe",(char *)NULL);_exit(99);}
        if((error=child_status(child,SIGABRT,0)))return error;
    if((error=preserved()))return error;
    }
    CHECK(unlink("core")==0&&chmod("core-probe",0755)==0);child=fork();CHECK(child>=0);
    if(!child){if(setgid(456)||setuid(123))_exit(98);execl("core-probe","core-probe",(char *)NULL);_exit(99);}
    if((error=child_status(child,SIGABRT,1)))return error;
    if((error=inspect(child,SIGABRT,NULL,NULL,0)))return error;
    CHECK(stat("core",&st)==0&&st.st_uid==123&&st.st_gid==456&&(st.st_mode&0777)==0640);
    CHECK(unlink("core")==0&&unlink("core-probe")==0);
    (void)umask(mask);CHECK(chdir("/")==0&&unlink("/tmp/core-dir/..")==0&&unlink("/tmp/core-dir/.")==0&&unlink("/tmp/core-dir")==0);
    CHECK(write(1,"NEXIX_CORE_OK\n",14)==14);return 0;
}
