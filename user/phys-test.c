#include <unistd.h>
#include <sys/phys.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <core.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"phys line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
static uintptr_t gate(long n,uintptr_t a,uintptr_t *permissions)
{
    uintptr_t flags;
    __asm__ volatile("int $0x81":"+a"(n),"=d"(flags):"D"(a):"memory","cc");
    if(permissions)*permissions=flags;
    return (uintptr_t)n;
}
static int finished(pid_t p,int code)
{int s;return wait(&s)==p && WIFEXITED(s) && WEXITSTATUS(s)==code;}
static int core_excludes_phys(void)
{
    int fd=open("/tmp/core",O_RDONLY);CHECK(fd>=0);
    struct nx_core_elf_header h;CHECK(read(fd,&h,sizeof(h))==sizeof(h) && h.phnum>0 && h.phnum<=NX_CORE_MAX_LOADS+1);
    CHECK(lseek(fd,h.phoff,SEEK_SET)==(off_t)h.phoff);
    for(unsigned i=0;i<h.phnum;++i){struct nx_core_program p;
        CHECK(read(fd,&p,sizeof(p))==sizeof(p));
        if(p.type==1)CHECK(p.vaddr+p.memsz<=NX_PHYS_BASE || p.vaddr>=NX_PHYS_ADDRESS(8));
    }
    CHECK(close(fd)==0 && unlink("/tmp/core")==0);return 0;
}
static int exec_collision(void)
{
    /* A valid image whose text collides only after two physical leaves have
     * already been copied into the staged VM. Failure must unwind those refs. */
    struct nx_core_elf_header h={.ident={0x7f,'E','L','F',2,1,1},.type=2,.machine=62,
        .version=1,.entry=NX_PHYS_ADDRESS(1),.phoff=64,.ehsize=64,.phentsize=56,.phnum=1};
    struct nx_core_program segment={.type=1,.flags=5,.offset=4096,
        .vaddr=NX_PHYS_ADDRESS(1),.filesz=1,.memsz=4096,.align=4096};
    int fd=creat("/tmp/phys-image",0755);CHECK(fd>=0);
    CHECK(write(fd,&h,sizeof(h))==sizeof(h) && write(fd,&segment,sizeof(segment))==sizeof(segment));
    CHECK(lseek(fd,4096,SEEK_SET)==4096 && write(fd,"\303",1)==1 && close(fd)==0);
    CHECK(execv("/tmp/phys-image",(char *const[]){"collision",NULL})==-1 && errno==EINVAL);
    CHECK(*(volatile uint64_t *)NX_PHYS_ADDRESS(0)==0x5566);
    CHECK(*(volatile uint64_t *)NX_PHYS_ADDRESS(1)==0x7788);
    CHECK(unlink("/tmp/phys-image")==0);return 0;
}
static int exec_shared(void)
{
    CHECK(gate(0x911,0,NULL)==0);
    int ready[2],go[2];CHECK(pipe(ready)==0 && pipe(go)==0);
    char r[12],g[12];snprintf(r,sizeof(r),"%d",ready[1]);snprintf(g,sizeof(g),"%d",go[0]);
    pid_t p=fork();CHECK(p>=0);
    if(!p){
        CHECK(close(ready[0])==0 && close(go[1])==0);
        CHECK(execv("/tmp/no-physical-image",(char *const[]){"none",NULL})==-1 && errno==ENOENT);
        CHECK(*(volatile uint64_t *)NX_PHYS_ADDRESS(0)==0x5566);
        execl("/tmp/trace","trace","7",r,g,(char *)NULL);_exit(99);
    }
    CHECK(close(ready[1])==0 && close(go[0])==0);char c;
    CHECK(read(ready[0],&c,1)==1 && c=='P');
    CHECK(*(volatile uint64_t *)NX_PHYS_ADDRESS(0)==0x9999);
    CHECK(*(volatile uint64_t *)NX_PHYS_ADDRESS(1)==0xaaaa);
    CHECK(write(go[1],"G",1)==1 && close(go[1])==0 && close(ready[0])==0 && finished(p,0));
    CHECK(unlink("/tmp/trace")==0);return 0;
}
int phys_test(void)
{
    uintptr_t pa=gate(0x917,0,NULL),flags;
    CHECK(pa && !(pa&4095));
    volatile uint64_t *a=(void *)NX_PHYS_ADDRESS(0),*b=(void *)NX_PHYS_ADDRESS(1);
    CHECK(phys(8,1,pa)==-1 && errno==EINVAL);
    CHECK(phys(0,3,pa)==-1 && errno==EINVAL);
    CHECK(phys(0,1,pa+1)==-1 && errno==EINVAL);
    CHECK(phys(0,1,0)==-1 && errno==EINVAL);
    CHECK(phys(0,1,0x100000)==-1 && errno==ENXIO);
    uintptr_t private_pa=gate(0x918,(uintptr_t)&pa,NULL)&~4095UL;
    CHECK(phys(0,1,private_pa)==-1 && errno==EBUSY);
    CHECK(phys(0,2,pa)==0 && a[0]==0 && a[512]==0);
    CHECK(gate(0x918,(uintptr_t)a,&flags)==pa && flags==(4|1|8)); /* USER, WRITE, PHYS; no EXEC */
    a[0]=0x1122;a[512]=0x3344;
    CHECK(phys(1,1,pa)==0 && b[0]==0x1122);b[0]=0x5566;CHECK(a[0]==0x5566);
    CHECK(phys(1,1,pa+4096)==0 && b[0]==0x3344);b[0]=0x7788;CHECK(a[512]==0x7788);
    CHECK(phys(1,3,pa)==-1 && errno==EINVAL && b[0]==0x7788);
    CHECK(phys(7,1,NX_PHYS_VGA_BASE)==0);
    volatile uint16_t *v=(void *)NX_PHYS_ADDRESS(7);uint16_t saved=v[0];
    v[0]=0x0741;CHECK(v[0]==0x0741);v[0]=saved;
    CHECK(gate(0x918,(uintptr_t)v,NULL)==NX_PHYS_VGA_BASE);
    CHECK(phys(7,0,UINTPTR_MAX)==0);
    CHECK(write(1,(const void *)v,1)==-1 && errno==EFAULT);
    pid_t p=fork();CHECK(p>=0);
    if(!p){
        CHECK(phys(0,0,0)==0 && phys(1,0,0)==0);
        CHECK(gate(0x902,0,NULL)==0);
        int rc=phys(6,1,pa),error=errno;
        CHECK(gate(0x903,0,NULL)==0);
        CHECK(rc==-1 && error==ENOMEM);
        CHECK(write(1,(void *)NX_PHYS_ADDRESS(6),1)==-1 && errno==EFAULT);
        _exit(0);
    }
    CHECK(finished(p,0) && a[0]==0x5566 && a[512]==0x7788);
    p=fork();CHECK(p>=0);
    if(!p){CHECK(chdir("/tmp")==0);((void (*)(void))(uintptr_t)a)();_exit(99);}
    int status;CHECK(wait(&status)==p && WIFSIGNALED(status) && WTERMSIG(status)==SIGSEGV && WCOREDUMP(status));
    int error=core_excludes_phys();if(error)return error;
    error=exec_collision();if(error)return error;
    error=exec_shared();if(error)return error;
    p=fork();CHECK(p>=0);
    if(!p){
        CHECK(setuid(123)==0);
        CHECK(phys(0,0,0)==-1 && errno==EPERM);
        CHECK(phys(2,1,pa)==-1 && errno==EPERM);
        /* Dropping root revoked the inherited window. */
        a[0]=0x7777;_exit(37);
    }
    CHECK(wait(&status)==p && WIFSIGNALED(status) && WTERMSIG(status)==SIGSEGV && a[0]==0x9999);
    CHECK(phys(0,0,0)==0 && b[0]==0xaaaa && phys(1,0,0)==0);
    CHECK(phys(0,1,pa)==0 && a[0]==0 && phys(0,0,0)==0); /* Last release frees, next claim zeroes. */
    static const char marker[]="NEXIX_PHYS_OK\n";
    CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);return 0;
}
