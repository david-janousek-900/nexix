/* V7 libc/gen/nlist.c semantics over native little-endian x86_64 ELF. */
#include <a.out.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

static void clear(struct nlist *list)
{
    for (; list->n_name && *list->n_name; ++list) {
        list->n_type=0; list->n_value=0;
    }
}
static int range(uint64_t off, uint64_t bytes, uint64_t size)
{
    return off<=size && bytes<=size-off;
}
static int fetch(int fd, uint64_t off, void *buf, size_t bytes)
{
    if (lseek(fd,(off_t)off,SEEK_SET)<0) return -1;
    unsigned char *p=buf;
    while (bytes) {
        ssize_t n=read(fd,p,bytes);
        if (n<0) { if (errno==EINTR) continue; return -1; }
        if (!n) { errno=ENOEXEC; return -1; }
        p+=n; bytes-=(size_t)n;
    }
    return 0;
}
static int section(int fd, const Elf64_Ehdr *h, unsigned i, Elf64_Shdr *s)
{
    return fetch(fd,h->e_shoff+(uint64_t)i*sizeof(*s),s,sizeof(*s));
}
/* Validate the whole name, including names nobody requested. No file-sized allocation. */
static int name_equal(int fd, const Elf64_Shdr *str, uint32_t offset,
                      const char *want, int *equal)
{
    if (offset>=str->sh_size) { errno=ENOEXEC; return -1; }
    uint64_t left=str->sh_size-offset, off=str->sh_offset+offset;
    int match=1;
    while (left) {
        char block[128];
        size_t n=left<sizeof(block)?(size_t)left:sizeof(block);
        if (fetch(fd,off,block,n)<0) return -1;
        for (size_t i=0;i<n;++i) {
            if (match && block[i]!=*want) match=0;
            if (!block[i]) { *equal=match; return 0; }
            if (match) ++want;
        }
        off+=n; left-=n;
    }
    errno=ENOEXEC; return -1;
}
static int symbols(int fd, uint64_t size, const Elf64_Ehdr *h,
                   const Elf64_Shdr *tab, struct nlist *list)
{
    Elf64_Shdr str;
    if (tab->sh_entsize!=sizeof(Elf64_Sym) || tab->sh_size%sizeof(Elf64_Sym) ||
        !range(tab->sh_offset,tab->sh_size,size) || tab->sh_link>=h->e_shnum) goto bad;
    if (section(fd,h,tab->sh_link,&str)<0) return -1;
    if (str.sh_type!=SHT_STRTAB || !str.sh_size || !range(str.sh_offset,str.sh_size,size)) goto bad;
    char zero;
    if (fetch(fd,str.sh_offset,&zero,1)<0) return -1;
    if (zero) goto bad;
    for (uint64_t off=0;off<tab->sh_size;off+=sizeof(Elf64_Sym)) {
        Elf64_Sym sym; Elf64_Shdr target;
        if (fetch(fd,tab->sh_offset+off,&sym,sizeof(sym))<0) return -1;
        unsigned type=ELF64_ST_TYPE(sym.st_info), bind=ELF64_ST_BIND(sym.st_info);
        int nt;
        uint64_t value=sym.st_value;
        if (sym.st_shndx==SHN_UNDEF) nt=N_UNDF;
        else if (sym.st_shndx==SHN_ABS) nt=N_ABS;
        else if (sym.st_shndx==SHN_COMMON) { nt=N_UNDF; value=sym.st_size; }
        else {
            if (sym.st_shndx>=h->e_shnum || sym.st_shndx>=SHN_LORESERVE) goto bad;
            if (section(fd,h,sym.st_shndx,&target)<0) return -1;
            nt=target.sh_type==SHT_NOBITS?N_BSS:
               (target.sh_flags&SHF_EXECINSTR)?N_TEXT:N_DATA;
        }
        if (type==STT_FILE) nt=N_FN;
        if (bind==STB_GLOBAL || bind==STB_WEAK) nt|=N_EXT;
        int match;
        /* Always validate names, even for an empty request list. */
        if (name_equal(fd,&str,sym.st_name,"",&match)<0) return -1;
        if (type==STT_SECTION || type==STT_TLS) continue;
        for (struct nlist *p=list;p->n_name && *p->n_name;++p) {
            if (name_equal(fd,&str,sym.st_name,p->n_name,&match)<0) return -1;
            if (match) { p->n_type=nt; p->n_value=value; break; }
        }
    }
    return 0;
bad:
    errno=ENOEXEC; return -1;
}
int nlist(const char *filename, struct nlist *list)
{
    if (!list) { errno=EINVAL; return -1; }
    clear(list);
    if (!filename) { errno=EINVAL; return -1; }
    int fd=open(filename,O_RDONLY);
    if (fd<0) return -1;
    struct stat st; Elf64_Ehdr h;
    int error=0, selected=SHT_DYNSYM;
    if (fstat(fd,&st)<0) goto fail;
    if (st.st_size<(off_t)sizeof(h)) goto bad;
    uint64_t size=(uint64_t)st.st_size;
    if (fetch(fd,0,&h,sizeof(h))<0) goto fail;
    if (memcmp(h.e_ident,ELFMAG,SELFMAG) || h.e_ident[EI_CLASS]!=ELFCLASS64 ||
        h.e_ident[EI_DATA]!=ELFDATA2LSB || h.e_ident[EI_VERSION]!=EV_CURRENT ||
        h.e_machine!=EM_X86_64 || h.e_version!=EV_CURRENT ||
        (h.e_type!=ET_REL && h.e_type!=ET_EXEC && h.e_type!=ET_DYN) ||
        h.e_ehsize!=sizeof(h)) goto bad;
    if (!h.e_shnum) {
        /* Sectionless/stripped executables are valid; extended numbering isn't supported. */
        if (h.e_shoff || h.e_shstrndx) goto bad;
        goto done;
    }
    if (h.e_shnum>=SHN_LORESERVE || h.e_shentsize!=sizeof(Elf64_Shdr) ||
        !h.e_shoff || !range(h.e_shoff,(uint64_t)h.e_shnum*sizeof(Elf64_Shdr),size) ||
        (h.e_shstrndx && h.e_shstrndx>=h.e_shnum)) goto bad;
    for (unsigned i=0;i<h.e_shnum;++i) {
        Elf64_Shdr s;
        if (section(fd,&h,i,&s)<0) goto fail;
        if (s.sh_type!=SHT_NULL && s.sh_type!=SHT_NOBITS && !range(s.sh_offset,s.sh_size,size)) goto bad;
        if (s.sh_type==SHT_SYMTAB) selected=SHT_SYMTAB;
    }
    for (unsigned i=0;i<h.e_shnum;++i) {
        Elf64_Shdr s;
        if (section(fd,&h,i,&s)<0) goto fail;
        if (s.sh_type==(unsigned)selected && symbols(fd,size,&h,&s,list)<0) goto fail;
    }
done:
    if (close(fd)<0) { clear(list); return -1; }
    return 0;
bad:
    errno=ENOEXEC;
fail:
    error=errno;
    (void)close(fd); clear(list); errno=error;
    return -1;
}
