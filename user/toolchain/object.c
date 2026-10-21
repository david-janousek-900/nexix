/* Checked native ELF and SysV/GNU, BSD and V7 archive readers. */
#include "object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#define INPUT_LIMIT (32UL*1024*1024)
static const unsigned char *find(const unsigned char *p,unsigned char c,size_t n)
{ for (size_t i=0;i<n;++i) if (p[i]==c) return p+i;return NULL; }
static int span(uint64_t off, uint64_t size, size_t limit)
{ return off<=limit && size<=limit-off; }
static int decimal(const unsigned char *s, size_t n, uint64_t *out)
{
    uint64_t value=0; size_t i=0;
    if (!n || s[0]<'0' || s[0]>'9') return -1;
    while (i<n && s[i]>='0' && s[i]<='9') {
        unsigned d=s[i++]-'0';
        if (value>(UINT64_MAX-d)/10) return -1;
        value=value*10+d;
    }
    while (i<n && s[i]==' ') ++i;
    if (i!=n) return -1;
    *out=value; return 0;
}
static unsigned le16(const unsigned char *p) { return p[0]|(unsigned)p[1]<<8; }
static uint32_t pdp32(const unsigned char *p) { return (uint32_t)le16(p)<<16|le16(p+2); }
int object_files(const char *path, object_visitor visit, void *context)
{
    FILE *f=fopen(path,"r"); unsigned char *data=NULL;
    size_t length,offset=0; long total; int result=-1,format=0;
    const unsigned char *strings=NULL; size_t string_size=0;
    if (!f) return -1;
    if (fseek(f,0,SEEK_END) || (total=ftell(f))<0 || (unsigned long)total>INPUT_LIMIT || fseek(f,0,SEEK_SET)) goto out;
    length=(size_t)total; data=malloc(length?length:1);
    if (!data || fread(data,1,length,f)!=length) goto out;
    if (length>=8 && !memcmp(data,"!<arch>\n",8)) { format=1;offset=8; }
    else if (length>=2 && le16(data)==0177545) { format=2;offset=2; }
    if (!format) { result=visit(path,NULL,data,length,context);goto out; }
    result=0;
    while (offset<length) {
        char name[4097]; uint64_t bytes,extended=0; size_t payload,next,n;
        if (format==2) {
            if (!span(offset,26,length)) goto bad;
            const unsigned char *zero=find(data+offset,0,14);
            n=zero?(size_t)(zero-data-offset):14;
            memcpy(name,data+offset,n);name[n]=0;
            bytes=pdp32(data+offset+22);payload=offset+26;
        } else {
            if (!span(offset,60,length) || memcmp(data+offset+58,"`\n",2) || decimal(data+offset+48,10,&bytes)) goto bad;
            memcpy(name,data+offset,16);name[16]=0;
            n=16;while(n && name[n-1]==' ') name[--n]=0;
            payload=offset+60;
        }
        if (!span(payload,bytes,length)) goto bad;
        next=payload+(size_t)bytes;
        if (bytes&1) { if (next==length) goto bad; ++next; }
        if (format==1 && !strcmp(name,"//")) {
            strings=data+payload;string_size=(size_t)bytes;offset=next;continue;
        }
        if (format==1 && (!strcmp(name,"/") || !strcmp(name,"/SYM64/"))) { offset=next;continue; }
        if (format==1 && !strncmp(name,"#1/",3)) {
            if (decimal((const unsigned char *)name+3,strlen(name)-3,&extended) || extended>bytes || extended>4096) goto bad;
            memcpy(name,data+payload,(size_t)extended);name[extended]=0;
            payload+=(size_t)extended;bytes-=extended;
        } else if (format==1 && name[0]=='/') {
            uint64_t index;
            if (decimal((const unsigned char *)name+1,strlen(name)-1,&index) || !strings || index>=string_size) goto bad;
            const unsigned char *end=find(strings+index,'\n',string_size-(size_t)index);
            if (!end || (n=(size_t)(end-strings-index))>4096) goto bad;
            if (n && strings[index+n-1]=='/') --n;
            memcpy(name,strings+index,n);name[n]=0;
        } else if (format==1 && n && name[n-1]=='/') name[n-1]=0;
        if (!*name) goto bad;
        if (strcmp(name,"__.SYMDEF") && strcmp(name,"__.SYMDEF SORTED"))
            if (visit(path,name,data+payload,(size_t)bytes,context)) result=-1;
        offset=next;
    }
    goto out;
bad:
    result=-1;
out:
    free(data); if (fclose(f)) result=-1;return result;
}
int object_section(const struct object *o,unsigned i,Elf64_Shdr *s)
{
    if (i>=o->h.e_shnum) return -1;
    memcpy(s,o->bytes+o->h.e_shoff+(uint64_t)i*sizeof(*s),sizeof(*s));return 0;
}
int object_open(struct object *o,const unsigned char *data,size_t size)
{
    if (size<sizeof(o->h)) return -1;
    memcpy(&o->h,data,sizeof(o->h));o->bytes=data;o->size=size;
    Elf64_Ehdr *h=&o->h;
    if (memcmp(h->e_ident,ELFMAG,4) || h->e_ident[EI_CLASS]!=ELFCLASS64 ||
        h->e_ident[EI_DATA]!=ELFDATA2LSB || h->e_ident[EI_VERSION]!=1 ||
        h->e_machine!=EM_X86_64 || h->e_version!=1 || h->e_ehsize!=64 ||
        (h->e_type!=ET_REL && h->e_type!=ET_EXEC && h->e_type!=ET_DYN)) return -1;
    if (h->e_phnum && (h->e_phentsize!=56 || !span(h->e_phoff,(uint64_t)h->e_phnum*56,size))) return -1;
    if (!h->e_shnum) return (h->e_shoff || h->e_shstrndx)?-1:0;
    if (!h->e_shoff || h->e_shnum>=SHN_LORESERVE || h->e_shentsize!=64 ||
        h->e_shstrndx>=h->e_shnum || !span(h->e_shoff,(uint64_t)h->e_shnum*64,size)) return -1;
    for (unsigned i=0;i<h->e_shnum;++i) {
        Elf64_Shdr s; object_section(o,i,&s);
        if (s.sh_type!=SHT_NOBITS && s.sh_type!=SHT_NULL && !span(s.sh_offset,s.sh_size,size)) return -1;
        if (s.sh_addralign && (s.sh_addralign&(s.sh_addralign-1))) return -1;
    }
    return 0;
}
int object_symbols(const struct object *o,struct object_symbol **output,size_t *count)
{
    Elf64_Shdr tab={0},str;struct object_symbol *symbols=NULL;
    *output=NULL;*count=0;
    for (unsigned i=0;i<o->h.e_shnum;++i) {
        Elf64_Shdr s;object_section(o,i,&s);
        if (s.sh_type==SHT_SYMTAB) { tab=s;break; }
        if (s.sh_type==SHT_DYNSYM) tab=s;
    }
    if (!tab.sh_type) return 0;
    if (tab.sh_entsize!=24 || tab.sh_size%24 || object_section(o,tab.sh_link,&str) ||
        str.sh_type!=SHT_STRTAB || !str.sh_size || o->bytes[str.sh_offset]) return -1;
    size_t used=0,capacity=(size_t)(tab.sh_size/24);
    if (capacity>SIZE_MAX/sizeof(*symbols)) return -1;
    symbols=calloc(capacity?capacity:1,sizeof(*symbols));if (!symbols) return -1;
    for (size_t i=0;i<capacity;++i) {
        Elf64_Sym s;Elf64_Shdr section;char type='?';
        memcpy(&s,o->bytes+tab.sh_offset+i*24,24);
        if (s.st_name>=str.sh_size || !find(o->bytes+str.sh_offset+s.st_name,0,(size_t)(str.sh_size-s.st_name))) goto bad;
        unsigned bind=ELF64_ST_BIND(s.st_info),kind=ELF64_ST_TYPE(s.st_info);
        if (s.st_shndx==SHN_UNDEF) type='u';
        else if (s.st_shndx==SHN_ABS) type='a';
        else if (s.st_shndx==SHN_COMMON) type='c';
        else {
            if (s.st_shndx>=SHN_LORESERVE || object_section(o,s.st_shndx,&section)) goto bad;
            type=section.sh_type==SHT_NOBITS?'b':(section.sh_flags&SHF_EXECINSTR)?'t':
                 (section.sh_flags&SHF_WRITE)?'d':'r';
        }
        if (kind==STT_SECTION || !o->bytes[str.sh_offset+s.st_name]) continue;
        if (kind==STT_FILE) type='f';
        if (bind==STB_GLOBAL) type=(char)toupper((unsigned char)type);
        if (bind==STB_WEAK) type=kind==STT_OBJECT?(s.st_shndx==SHN_UNDEF?'v':'V'):
                                                            (s.st_shndx==SHN_UNDEF?'w':'W');
        symbols[used++]=(struct object_symbol){(const char *)o->bytes+str.sh_offset+s.st_name,
            s.st_shndx==SHN_COMMON?s.st_size:s.st_value,bind,type};
    }
    *output=symbols;*count=used;return 0;
bad:
    free(symbols);return -1;
}
int object_sizes(const struct object *o,uint64_t *text,uint64_t *data,uint64_t *bss)
{
    *text=*data=*bss=0;
    if (!o->h.e_shnum) return -1; /* No invented section sizes for stripped files. */
    for (unsigned i=0;i<o->h.e_shnum;++i) {
        Elf64_Shdr s;object_section(o,i,&s);
        if (!(s.sh_flags&SHF_ALLOC)) continue;
        uint64_t *sum=s.sh_type==SHT_NOBITS?bss:(s.sh_flags&SHF_WRITE)?data:text;
        if (*sum>UINT64_MAX-s.sh_size) return -1;
        *sum+=s.sh_size;
    }
    return 0;
}
