/* V7 sig.c core policy with a native ELF64 data/stack and register image. */
#include "core.h"
#include "files.h"
#include "fpu.h"
#include "clock.h"
#include <nexix/errno.h>
_Static_assert(sizeof(struct interrupt_frame)==sizeof(struct nx_core_registers),"core frame ABI");
static struct nx_core_program programs[NX_CORE_MAX_LOADS+1];
static uint8_t notes[1024];
static unsigned loads;
static size_t note_size;
static void copy(void *target,const void *source,size_t size)
{uint8_t *d=target;const uint8_t *s=source;while(size--)*d++=*s++;}
bool core_signal(unsigned n)
{
    return n==NX_SIGQUIT||n==NX_SIGILL||n==NX_SIGTRAP||n==NX_SIGABRT||
        n==NX_SIGEMT||n==NX_SIGFPE||n==NX_SIGBUS||n==NX_SIGSEGV||n==NX_SIGSYS;
}
static bool mapping(uintptr_t va,uintptr_t pa,unsigned flags,void *context)
{
    (void)pa;(void)context;
    if(!(flags&VM_USER)||(flags&VM_EXEC))return true; /* V7 excludes pure text. */
    unsigned permissions=4|((flags&VM_WRITE)?2:0);
    if(loads&&programs[loads].vaddr+programs[loads].memsz==va&&programs[loads].flags==permissions){
        programs[loads].filesz+=PAGE_SIZE;programs[loads].memsz+=PAGE_SIZE;return true;
    }
    if(loads==NX_CORE_MAX_LOADS)return false;
    programs[++loads]=(struct nx_core_program){.type=1,.flags=permissions,.vaddr=va,
        .filesz=PAGE_SIZE,.memsz=PAGE_SIZE,.align=PAGE_SIZE};
    return true;
}
static void note(unsigned type,const void *data,size_t size)
{
    struct nx_core_note header={6,(uint32_t)size,type};
    copy(notes+note_size,&header,sizeof(header));note_size+=sizeof(header);
    const char name[8]={'N','E','X','I','X',0,0,0};
    copy(notes+note_size,name,sizeof(name));note_size+=sizeof(name);
    copy(notes+note_size,data,size);note_size+=size;
    while(note_size&3)notes[note_size++]=0;
}
bool core_dump(unsigned n,const struct interrupt_frame *frame)
{
    struct proc *p=proc_current();
    if(!p||!frame||!core_signal(n)||(frame->cs&3)!=3)return false;
    struct user *u=p->p_user;
    if(u->u_uid!=u->u_ruid||u->u_gid!=u->u_rgid)return false;
    loads=0;note_size=0;
    if(!vm_walk_user(&p->p_vm,mapping,NULL))return false;
    struct nx_core_info info={.version=NX_CORE_VERSION,.signal=n,.pid=p->p_pid,.ppid=p->p_ppid,
        .ruid=u->u_ruid,.euid=u->u_uid,.rgid=u->u_rgid,.egid=u->u_gid,.time=clock_now()};
    copy(info.command,u->u_comm,V7_DIRSIZ);
    struct nx_fpu_state fp;fpu_save(&fp);
    note(NX_CORE_INFO,&info,sizeof(info));note(NX_CORE_REGISTERS,frame,sizeof(*frame));note(NX_CORE_FPU,&fp,sizeof(fp));
    struct nx_core_elf_header header={.ident={0x7f,'E','L','F',2,1,1},.type=4,.machine=62,.version=1,
        .phoff=sizeof(header),.ehsize=sizeof(header),.phentsize=sizeof(programs[0]),.phnum=loads+1};
    uint64_t offset=sizeof(header)+(loads+1)*sizeof(programs[0]);
    programs[0]=(struct nx_core_program){.type=4,.offset=offset,.filesz=note_size,.align=4};
    offset=(offset+note_size+PAGE_SIZE-1)&~(PAGE_SIZE-1);
    for(unsigned i=1;i<=loads;++i){programs[i].offset=offset;offset+=programs[i].filesz;}
    struct inode *file=NULL;
    if(files_core_begin(&file))return false;
    bool good=!files_core_write(file,0,&header,sizeof(header))&&
        !files_core_write(file,sizeof(header),programs,(loads+1)*sizeof(programs[0]))&&
        !files_core_write(file,programs[0].offset,notes,note_size);
    for(unsigned i=1;good&&i<=loads;++i){
        for(uint64_t used=0;good&&used<programs[i].filesz;used+=PAGE_SIZE){
            uintptr_t pa;unsigned flags;
            if(!vm_translate(&p->p_vm,programs[i].vaddr+used,&pa,&flags)||!(flags&VM_USER)||(flags&VM_EXEC))good=false;
            else good=!files_core_write(file,programs[i].offset+used,(const void *)pa,PAGE_SIZE);
        }
    }
    int error=files_inode_put(file);
    return good&&!error;
}
