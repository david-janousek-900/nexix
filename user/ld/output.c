/* Static ELF64 AMD64 layout and explicit-addend relocation pass. */
#include "ld.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define BASE 0x40000000UL
#define IMAGE_LIMIT (8UL*1024*1024)
#define PAGE 4096UL
struct buffer {unsigned char *p;size_t n,cap;};
struct mapping {uint64_t address,offset;unsigned group;};
struct layout {struct ld_state *s;struct mapping **map;uint64_t *globals;uint64_t size[4],start[4];};
struct program {uint32_t type,flags;uint64_t offset,address,physical,filesz,memsz,align;};
struct relocation {uint64_t offset,info;int64_t addend;};
_Static_assert(sizeof(struct program)==56,"ELF program header");
_Static_assert(sizeof(struct relocation)==24,"ELF RELA");
static int error(struct ld_state *s,const char *text) {
    if(!s->failed)snprintf(s->error,sizeof(s->error),"%s",text);
    s->failed=1;return -1;
}
static int add(struct buffer *b,const void *p,size_t n) {
    if(n>LD_BYTE_LIMIT-b->n)return -1;
    size_t need=b->n+n;
    if(need>b->cap) {
        size_t cap=b->cap?b->cap:256;
        while(cap<need)cap*=2;
        void *q=realloc(b->p,cap);if(!q)return -1;b->p=q;b->cap=cap;
    }
    if(n) {if(p)memcpy(b->p+b->n,p,n);else memset(b->p+b->n,0,n);}
    b->n=need;return 0;
}
static int pad(struct buffer *b,unsigned n) {return add(b,NULL,(-b->n)&(n-1));}
static int name(struct buffer *b,const char *p,uint32_t *n) {
    *n=(uint32_t)b->n;return add(b,p,strlen(p)+1);
}
static uint64_t align(uint64_t v,uint64_t a) {return (v+a-1)&~(a-1);}
static const char *section_name(const struct ld_input *o,unsigned index) {
    Elf64_Shdr h,names;object_section(&o->object,index,&h);
    if(object_section(&o->object,o->object.h.e_shstrndx,&names) || names.sh_type!=SHT_STRTAB || h.sh_name>=names.sh_size)return NULL;
    const char *p=(const char *)o->data+names.sh_offset+h.sh_name;
    for(uint64_t i=h.sh_name;i<names.sh_size;i++)if(!o->data[names.sh_offset+i])return p;
    return NULL;
}
static int classify(struct ld_state *s,const struct ld_input *o,unsigned i,Elf64_Shdr h) {
    const char *n=section_name(o,i);
    if(!n)return error(s,"invalid section name table");
    if(!strcmp(n,".eh_frame") || !strncmp(n,".note",5))return 0;
    if(!(h.sh_flags&SHF_ALLOC))return 0;
    if(h.sh_flags&~(uint64_t)(1|2|4|16|32) || (h.sh_flags&5)==5 ||
       (h.sh_type!=SHT_PROGBITS && h.sh_type!=SHT_NOBITS))return error(s,"unsupported allocated section");
    if(h.sh_type==SHT_NOBITS) {
        if(!(h.sh_flags&SHF_WRITE))return error(s,"unsupported read-only NOBITS section");
        return 3;
    }
    return (h.sh_flags&SHF_WRITE)?2:1;
}
static int place(struct layout *l) {
    struct ld_state *s=l->s;
    for(unsigned group=1;group<=3;group++) {
        l->start[group]=group==1?0:group==2?align(l->size[1],PAGE):l->start[2]+l->size[2];
        uint64_t cursor=BASE+l->start[group];
        for(size_t oi=0;oi<s->input_count;oi++) {
            struct ld_input *o=s->inputs+oi;if(!o->selected)continue;
            for(unsigned i=1;i<o->object.h.e_shnum;i++) {
                Elf64_Shdr h;object_section(&o->object,i,&h);
                int g=classify(s,o,i,h);if(g<0)return -1;if((unsigned)g!=group)continue;
                uint64_t a=h.sh_addralign?h.sh_addralign:1;
                if(a>IMAGE_LIMIT || h.sh_size>IMAGE_LIMIT)return error(s,"section exceeds native image limit");
                cursor=align(cursor,a);
                if(cursor-BASE>IMAGE_LIMIT || h.sh_size>IMAGE_LIMIT-(cursor-BASE))return error(s,"native image exceeds 8 MiB");
                l->map[oi][i]=(struct mapping){cursor,PAGE+cursor-BASE,group};cursor+=h.sh_size;
            }
        }
        if(group==3) {
            for(size_t i=0;i<s->symbol_count;i++) {
                struct ld_symbol *g=s->symbols+i;if(!g->common)continue;
                if(g->alignment>IMAGE_LIMIT || g->size>IMAGE_LIMIT)return error(s,"common exceeds native image limit");
                cursor=align(cursor,g->alignment);
                if(cursor-BASE>IMAGE_LIMIT || g->size>IMAGE_LIMIT-(cursor-BASE))return error(s,"native image exceeds 8 MiB");
                l->globals[i]=cursor;cursor+=g->size;
            }
        }
        l->size[group]=cursor-BASE-l->start[group];
    }
    if(align(l->start[3]+l->size[3],PAGE)>IMAGE_LIMIT)return error(s,"native mapped image exceeds 8 MiB");
    return 0;
}
static int local_value(struct layout *l,size_t oi,size_t si,uint64_t *value,unsigned *section) {
    struct ld_input *o=l->s->inputs+oi;Elf64_Sym *n=o->symbols+si;
    if(!si || n->st_shndx==SHN_UNDEF) {*value=0;*section=SHN_UNDEF;return 0;}
    if(n->st_shndx==SHN_ABS) {*value=n->st_value;*section=SHN_ABS;return 0;}
    if(n->st_shndx==SHN_COMMON)return error(l->s,"unexpected local common");
    struct mapping *m=l->map[oi]+n->st_shndx;
    if(!m->group)return error(l->s,"reference to discarded section");
    *value=m->address+n->st_value;*section=m->group;return 0;
}
static int global_value(struct layout *l,size_t gi,uint64_t *value,unsigned *section) {
    struct ld_symbol *g=l->s->symbols+gi;
    if(!g->defined) {*value=0;*section=SHN_UNDEF;return 0;}
    if(g->common) {*value=l->globals[gi];*section=3;return 0;}
    if(g->object==LD_NONE) {*value=g->value;*section=SHN_ABS;return 0;}
    return local_value(l,g->object,g->symbol,value,section);
}
static int symbol_value(struct layout *l,size_t oi,size_t si,uint64_t *value) {
    struct ld_input *o=l->s->inputs+oi;Elf64_Sym *n=o->symbols+si;unsigned section;
    if(ELF64_ST_BIND(n->st_info)==STB_LOCAL)return local_value(l,oi,si,value,&section);
    size_t i=ld_lookup(l->s,ld_symbol_name(o,si));
    if(i==LD_NONE)return error(l->s,"missing resolution record");
    return global_value(l,i,value,&section);
}
static void provide(struct ld_state *s,const char *text,uint64_t value) {
    size_t i=ld_lookup(s,text);if(i==LD_NONE || s->symbols[i].defined)return;
    struct ld_symbol *g=s->symbols+i;g->defined=1;g->value=value;g->bind=STB_GLOBAL;
}
static int relocate(struct layout *l,struct buffer *out) {
    struct ld_state *s=l->s;
    for(size_t oi=0;oi<s->input_count;oi++) {
        struct ld_input *o=s->inputs+oi;if(!o->selected)continue;
        for(unsigned i=1;i<o->object.h.e_shnum;i++) {
            Elf64_Shdr h;object_section(&o->object,i,&h);
            if(h.sh_type!=4 && h.sh_type!=9)continue; /* RELA / unsupported REL */
            Elf64_Shdr target;
            if(!h.sh_info || object_section(&o->object,h.sh_info,&target) || h.sh_link>=o->object.h.e_shnum)
                return error(s,"invalid relocation section references");
            Elf64_Shdr sym;object_section(&o->object,h.sh_link,&sym);
            if(sym.sh_type!=SHT_SYMTAB || h.sh_type!=4 || h.sh_entsize!=24 || h.sh_size%24)
                return error(s,"invalid/unsupported relocation table");
            struct mapping *m=l->map[oi]+h.sh_info;
            for(uint64_t j=0;j<h.sh_size;j+=24) {
                struct relocation r;memcpy(&r,o->data+h.sh_offset+j,24);
                uint64_t si=r.info>>32;unsigned type=(unsigned)r.info;
                if(si>=o->symbol_count || r.offset>target.sh_size)return error(s,"invalid relocation symbol/offset");
                if(!m->group)continue; /* debug/unwind/notes have no runtime output */
                unsigned width=type==0?0:type==1?8:(type==2 || type==4 || type==10 || type==11)?4:99;
                if(width==99)return error(s,"unsupported x86_64 relocation");
                if(width>target.sh_size-r.offset || (width && target.sh_type==SHT_NOBITS))return error(s,"relocation outside file-backed section");
                if(!width)continue;
                uint64_t sv;if(symbol_value(l,oi,(size_t)si,&sv))return -1;
                /* ELF64 arithmetic is modulo 2^64; narrow forms must reproduce
                 * the complete result after zero/sign extension. */
                uint64_t bits=sv+(uint64_t)r.addend;
                if(type==2 || type==4)bits-=m->address+r.offset;
                if((type==10 && bits>UINT32_MAX) ||
                   ((type==2 || type==4 || type==11) &&
                    (uint64_t)(int64_t)(int32_t)bits!=bits))
                    return error(s,"x86_64 relocation overflow");
                size_t at=(size_t)(m->offset+r.offset);
                if(at>out->n || width>out->n-at)return error(s,"invalid output relocation span");
                for(unsigned k=0;k<width;k++)out->p[at+k]=(unsigned char)(bits>>(8*k));
            }
        }
    }
    return 0;
}
static int symbols(struct layout *l,struct buffer *syms,struct buffer *strs,unsigned *first_global) {
    Elf64_Sym z={0};if(add(syms,&z,sizeof(z)) || add(strs,"",1))return -1;
    struct ld_state *s=l->s;
    for(size_t oi=0;oi<s->input_count;oi++) {
        struct ld_input *o=s->inputs+oi;if(!o->selected)continue;
        for(size_t i=1;i<o->symbol_count;i++) {
            Elf64_Sym n=o->symbols[i];const char *text=ld_symbol_name(o,i);
            if(ELF64_ST_BIND(n.st_info)!=STB_LOCAL || !*text || n.st_shndx==SHN_UNDEF ||
               (n.st_shndx!=SHN_ABS && !l->map[oi][n.st_shndx].group))continue;
            unsigned section;
            if(local_value(l,oi,i,&n.st_value,&section) || name(strs,text,&n.st_name))return -1;
            n.st_shndx=(Elf64_Half)section;if(add(syms,&n,sizeof(n)))return -1;
        }
    }
    *first_global=(unsigned)(syms->n/sizeof(Elf64_Sym));
    for(size_t i=0;i<s->symbol_count;i++) {
        struct ld_symbol *g=s->symbols+i;Elf64_Sym n={0};unsigned section;
        if(global_value(l,i,&n.st_value,&section) || name(strs,g->name,&n.st_name))return -1;
        n.st_shndx=(Elf64_Half)section;n.st_size=g->size;n.st_info=ELF64_ST_INFO(g->bind,STT_NOTYPE);
        if(g->object!=LD_NONE && g->defined) {
            Elf64_Sym *old=s->inputs[g->object].symbols+g->symbol;
            n.st_info=ELF64_ST_INFO(g->bind,g->common?STT_OBJECT:ELF64_ST_TYPE(old->st_info));n.st_other=old->st_other;
        }
        if(add(syms,&n,sizeof(n)))return -1;
    }
    return 0;
}
int ld_executable(struct ld_state *s,const char *entry,unsigned char **bytes,size_t *length) {
    struct layout l={.s=s};struct buffer out={0},syms={0},strs={0},shnames={0};int result=-1;
    if(s->failed || !bytes || !length || !entry)return -1;
    l.map=calloc(s->input_count?s->input_count:1,sizeof(*l.map));
    l.globals=calloc(s->symbol_count?s->symbol_count:1,sizeof(*l.globals));
    if(!l.map || !l.globals)goto memory;
    for(size_t i=0;i<s->input_count;i++)if(s->inputs[i].selected) {
        l.map[i]=calloc(s->inputs[i].object.h.e_shnum,sizeof(**l.map));if(!l.map[i])goto memory;
    }
    if(place(&l))goto cleanup;
    uint64_t text_end=BASE+l.size[1],data_end=BASE+l.start[2]+l.size[2],end=BASE+l.start[3]+l.size[3];
    provide(s,"__nx_text_end",text_end);provide(s,"etext",text_end);provide(s,"_etext",text_end);
    provide(s,"edata",data_end);provide(s,"_edata",data_end);provide(s,"end",end);provide(s,"_end",end);
    if(ld_check(s))goto cleanup;
    size_t ei=ld_lookup(s,entry);uint64_t address=0;unsigned section;
    if(ei==LD_NONE || global_value(&l,ei,&address,&section) || !s->symbols[ei].defined ||
       address<BASE || address>=text_end) {error(s,"entry is not in executable text");goto cleanup;}
    if(add(&out,NULL,(size_t)(PAGE+l.start[2]+l.size[2])) || add(&shnames,"",1))goto memory;
    for(size_t oi=0;oi<s->input_count;oi++) {
        struct ld_input *o=s->inputs+oi;if(!o->selected)continue;
        for(unsigned i=1;i<o->object.h.e_shnum;i++) {
            struct mapping *m=l.map[oi]+i;if(m->group!=1 && m->group!=2)continue;
            Elf64_Shdr h;object_section(&o->object,i,&h);
            memcpy(out.p+m->offset,o->data+h.sh_offset,(size_t)h.sh_size);
        }
    }
    if(relocate(&l,&out))goto cleanup;
    unsigned first_global;
    if(symbols(&l,&syms,&strs,&first_global)) {if(!s->failed)error(s,"cannot build symbol output");goto cleanup;}
    Elf64_Shdr h[7]={{0}};
    const char *names[]={"",".text",".data",".bss",".symtab",".strtab",".shstrtab"};
    for(unsigned i=1;i<7;i++)if(name(&shnames,names[i],&h[i].sh_name))goto memory;
    for(unsigned i=1;i<=3;i++) {
        h[i].sh_type=i==3?SHT_NOBITS:SHT_PROGBITS;h[i].sh_flags=SHF_ALLOC|(i==1?SHF_EXECINSTR:SHF_WRITE);
        h[i].sh_addr=BASE+l.start[i];h[i].sh_offset=PAGE+l.start[i];h[i].sh_size=l.size[i];h[i].sh_addralign=i==3?1:PAGE;
    }
    if(pad(&out,8))goto memory;
    h[4].sh_offset=out.n;h[4].sh_type=SHT_SYMTAB;h[4].sh_size=syms.n;h[4].sh_link=5;h[4].sh_info=first_global;h[4].sh_addralign=8;h[4].sh_entsize=24;
    if(add(&out,syms.p,syms.n))goto memory;
    h[5].sh_offset=out.n;h[5].sh_type=SHT_STRTAB;h[5].sh_size=strs.n;h[5].sh_addralign=1;
    if(add(&out,strs.p,strs.n))goto memory;
    h[6].sh_offset=out.n;h[6].sh_type=SHT_STRTAB;h[6].sh_size=shnames.n;h[6].sh_addralign=1;
    if(add(&out,shnames.p,shnames.n) || pad(&out,8))goto memory;
    Elf64_Ehdr e={0};memcpy(e.e_ident,ELFMAG,4);e.e_ident[EI_CLASS]=ELFCLASS64;e.e_ident[EI_DATA]=ELFDATA2LSB;e.e_ident[EI_VERSION]=1;
    e.e_type=ET_EXEC;e.e_machine=EM_X86_64;e.e_version=1;e.e_entry=address;e.e_phoff=64;e.e_shoff=out.n;
    e.e_ehsize=64;e.e_phentsize=56;e.e_phnum=3;e.e_shentsize=64;e.e_shnum=7;e.e_shstrndx=6;
    struct program ph[3]={{1,5,PAGE,BASE,BASE,l.size[1],l.size[1],PAGE},
        {1,6,PAGE+l.start[2],BASE+l.start[2],BASE+l.start[2],l.size[2],l.size[2]+l.size[3],PAGE},
        {0x6474e551,6,0,0,0,0,0,16}};
    /* BSS starts immediately after data, including alignment padding in size[3]. */
    memcpy(out.p,&e,sizeof(e));memcpy(out.p+64,ph,sizeof(ph));
    if(add(&out,h,sizeof(h)))goto memory;
    *bytes=out.p;*length=out.n;out.p=NULL;result=0;goto cleanup;
memory:
    error(s,"link output resource limit or allocation failure");
cleanup:
    if(l.map)for(size_t i=0;i<s->input_count;i++)free(l.map[i]);
    free(l.map);free(l.globals);free(out.p);free(syms.p);free(strs.p);free(shnames.p);return result;
}
