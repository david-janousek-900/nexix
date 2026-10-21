/* Native ELF counterpart of V7 strip: discard static symbols/debug/relocations.
 * Program offsets and code/data stay fixed; ELF section indexes are remapped.
 */
#include "object.h"
#include "archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { RELA=4, REL=9, GROUP=17, SYMTAB_SHNDX=18, INFO_LINK=0x40, GROUP_FLAG=0x200 };
struct program {uint32_t type,flags;uint64_t offset,vaddr,paddr,filesz,memsz,align;};
static int region(uint64_t offset,uint64_t size,size_t limit) {return offset<=limit&&size<=limit-offset;}
static int align_size(size_t *n,uint64_t alignment) {
    if(alignment>ARCHIVE_LIMIT)return -1;
    size_t a=alignment?alignment:1;
    if(*n>ARCHIVE_LIMIT-(a-1))return -1;
    *n=(*n+a-1)&~(a-1);return 0;
}
static const char *section_name(const struct object *o,const Elf64_Shdr *s,const Elf64_Shdr *strings) {
    if(!o->h.e_shstrndx)return s->sh_name?NULL:"";
    if(s->sh_name>=strings->sh_size)return NULL;
    const char *p=(const char*)o->bytes+strings->sh_offset+s->sh_name;
    for(size_t i=0;i<strings->sh_size-s->sh_name;i++)if(!p[i])return p;
    return NULL;
}
static int strip_member(struct archive_member *m) {
    struct object o;struct object_symbol *syms=NULL;size_t nsyms;
    if(object_open(&o,m->data,m->size)||object_symbols(&o,&syms,&nsyms))return -1;
    free(syms);
    size_t end=sizeof(Elf64_Ehdr);int result=-1;
    if(o.h.e_phnum) {
        end=o.h.e_phoff+(size_t)o.h.e_phnum*56;
        for(unsigned i=0;i<o.h.e_phnum;i++) {
            struct program p;memcpy(&p,m->data+o.h.e_phoff+i*56,56);
            if(!region(p.offset,p.filesz,m->size)||p.filesz>p.memsz)return -1;
            if(p.offset+p.filesz>end)end=p.offset+p.filesz;
        }
    }
    if(!o.h.e_shnum)return 0;
    Elf64_Shdr strings;
    if(object_section(&o,o.h.e_shstrndx,&strings))return -1;
    if(o.h.e_shstrndx&&(strings.sh_type!=SHT_STRTAB||!strings.sh_size||m->data[strings.sh_offset]))return -1;
    Elf64_Shdr *sections=calloc(o.h.e_shnum,sizeof(*sections));
    unsigned *map=calloc(o.h.e_shnum,sizeof(*map));unsigned char *keep=calloc(o.h.e_shnum,1);
    unsigned char *output=NULL;
    if(!sections||!map||!keep)goto out;
    for(unsigned i=0;i<o.h.e_shnum;i++) {
        object_section(&o,i,sections+i);Elf64_Shdr *s=sections+i;
        const char *name=section_name(&o,s,&strings);if(!name||s->sh_link>=o.h.e_shnum)goto out;
        int remove=s->sh_type==SHT_SYMTAB||s->sh_type==SYMTAB_SHNDX||s->sh_type==GROUP||
            (!(s->sh_flags&SHF_ALLOC)&&(s->sh_type==REL||s->sh_type==RELA))||
            !strncmp(name,".debug",6)||!strncmp(name,".zdebug",7)||!strncmp(name,".stab",5)||!strcmp(name,".gnu_debuglink");
        if(remove&&(s->sh_flags&SHF_ALLOC))goto out;
        keep[i]=!remove;
    }
    keep[0]=1;keep[o.h.e_shstrndx]=1;
    for(unsigned i=1;i<o.h.e_shnum;i++)if(sections[i].sh_type==SHT_SYMTAB) {
        unsigned str=sections[i].sh_link;int needed=str==o.h.e_shstrndx;
        for(unsigned j=1;j<o.h.e_shnum;j++)if(keep[j]&&sections[j].sh_link==str)needed=1;
        if(!needed)keep[str]=0;
    }
    for(unsigned pass=0;pass<o.h.e_shnum;pass++) {
        int changed=0;
        for(unsigned i=1;i<o.h.e_shnum;i++)if(keep[i]&&sections[i].sh_link&&!keep[sections[i].sh_link]) {
            if(sections[i].sh_flags&SHF_ALLOC)goto out;
            keep[i]=0;changed=1;
        }
        if(!changed)break;
    }
    unsigned used=0;
    for(unsigned i=0;i<o.h.e_shnum;i++)if(keep[i])map[i]=used++;
    /* Keep program bytes fixed; pack all other retained sections after them. */
    size_t cursor=end;
    for(unsigned i=1;i<o.h.e_shnum;i++)if(keep[i]) {
        Elf64_Shdr *s=sections+i;
        if(s->sh_type==SHT_NOBITS)continue;
        if(o.h.e_phnum&&(s->sh_flags&SHF_ALLOC)) {
            if(s->sh_offset+s->sh_size>cursor)cursor=s->sh_offset+s->sh_size;
        }
    }
    size_t *offsets=calloc(o.h.e_shnum,sizeof(*offsets));if(!offsets)goto out;
    for(unsigned i=1;i<o.h.e_shnum;i++)if(keep[i]) {
        Elf64_Shdr *s=sections+i;
        if(o.h.e_phnum&&(s->sh_flags&SHF_ALLOC))offsets[i]=s->sh_offset;
        else {if(align_size(&cursor,s->sh_addralign)){free(offsets);goto out;}offsets[i]=cursor;if(s->sh_type!=SHT_NOBITS) {if(s->sh_size>ARCHIVE_LIMIT-cursor){free(offsets);goto out;}cursor+=s->sh_size;}}
    }
    if(align_size(&cursor,8)||used>(ARCHIVE_LIMIT-cursor)/64){free(offsets);goto out;}
    size_t table=cursor,total=cursor+used*64;
    output=calloc(total?total:1,1);if(!output){free(offsets);goto out;}
    memcpy(output,m->data,end);
    /* Remove non-loadable bytes left in holes of the original file. */
    for(unsigned i=1;i<o.h.e_shnum;i++)if(!keep[i]&&sections[i].sh_type!=SHT_NOBITS) {
        Elf64_Shdr *s=sections+i;
        for(unsigned j=0;j<o.h.e_phnum;j++) {
            struct program p;memcpy(&p,m->data+o.h.e_phoff+j*56,56);
            if(p.type==1&&s->sh_size&&s->sh_offset<p.offset+p.filesz&&p.offset<s->sh_offset+s->sh_size){free(offsets);goto out;}
        }
        if(s->sh_offset<end) {size_t n=s->sh_size;if(n>end-s->sh_offset)n=end-s->sh_offset;memset(output+s->sh_offset,0,n);}
    }
    for(unsigned i=1;i<o.h.e_shnum;i++)if(keep[i]) {
        Elf64_Shdr s=sections[i];
        if(s.sh_type!=SHT_NOBITS)memcpy(output+offsets[i],m->data+s.sh_offset,s.sh_size);
        s.sh_offset=offsets[i];s.sh_flags&=~(uint64_t)GROUP_FLAG;
        if(s.sh_link)s.sh_link=map[s.sh_link];
        if((s.sh_type==REL||s.sh_type==RELA||(s.sh_flags&INFO_LINK))&&s.sh_info) {
            if(s.sh_info>=o.h.e_shnum||!keep[s.sh_info]){free(offsets);goto out;}s.sh_info=map[s.sh_info];
        }
        if(s.sh_type==SHT_DYNSYM) {
            if(s.sh_entsize!=24||s.sh_size%24){free(offsets);goto out;}
            for(size_t j=0;j<s.sh_size;j+=24) {
                Elf64_Sym symbol;memcpy(&symbol,output+s.sh_offset+j,24);
                if(symbol.st_shndx<SHN_LORESERVE&&symbol.st_shndx) {
                    if(symbol.st_shndx>=o.h.e_shnum||!keep[symbol.st_shndx]){free(offsets);goto out;}
                    symbol.st_shndx=map[symbol.st_shndx];memcpy(output+s.sh_offset+j,&symbol,24);
                }
            }
        }
        memcpy(output+table+map[i]*64,&s,64);
    }
    free(offsets);
    o.h.e_shoff=table;o.h.e_shnum=used;o.h.e_shstrndx=map[o.h.e_shstrndx];
    memcpy(output,&o.h,sizeof(o.h));free(m->data);m->data=output;m->size=total;output=NULL;result=0;
out:
    free(output);free(sections);free(map);free(keep);return result;
}
int main(int argc,char **argv) {
    if(argc<2){fprintf(stderr,"usage: strip file ...\n");return 2;}
    int status=0;
    for(int i=1;i<argc;i++) {
        struct archive_member m;
        if(archive_file(argv[i],&m)){fprintf(stderr,"strip: cannot read %s\n",argv[i]);status=1;continue;}
        int container=m.size>=8&&!memcmp(m.data,"!<arch>\n",8);
        if(m.size>=2&&(m.data[0]==0x65||m.data[0]==0x6d)&&m.data[1]==0xff)container=1;
        if(container) {
            struct archive a;archive_member_free(&m);
            if(archive_load(argv[i],&a)){fprintf(stderr,"strip: invalid archive %s\n",argv[i]);status=1;continue;}
            int failed=0;for(size_t j=0;j<a.count;j++)if(strip_member(a.members+j)){failed=1;break;}
            if(!failed)failed=archive_write(argv[i],&a,a.indexed);
            if(failed){fprintf(stderr,"strip: cannot strip %s\n",argv[i]);status=1;}
            archive_free(&a);
        } else {
            if(strip_member(&m)||archive_replace(argv[i],m.data,m.size)){fprintf(stderr,"strip: cannot strip %s\n",argv[i]);status=1;}
            archive_member_free(&m);
        }
    }
    return status;
}
