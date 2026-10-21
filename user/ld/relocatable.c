/* ELF64 -r preserves input section identities, rewriting symbol/RELA indices. */
#include "ld.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
struct buffer {unsigned char *p;size_t n,cap;};
struct rela {uint64_t offset,info;int64_t addend;};
struct section {Elf64_Shdr h;const unsigned char *data;};
static int fail(struct ld_state *s,const char *p) {if(!s->failed)snprintf(s->error,sizeof(s->error),"%s",p);s->failed=1;return -1;}
static int append(struct buffer *b,const void *p,size_t n) {
    if(n>LD_BYTE_LIMIT-b->n)return -1;
    size_t end=b->n+n;
    if(end>b->cap) {size_t cap=b->cap?b->cap:256;while(cap<end)cap*=2;void *q=realloc(b->p,cap);if(!q)return -1;b->p=q;b->cap=cap;}
    if(n) {if(p)memcpy(b->p+b->n,p,n);else memset(b->p+b->n,0,n);}b->n=end;return 0;
}
static int string(struct buffer *b,const char *p,uint32_t *offset) {*offset=(uint32_t)b->n;return append(b,p,strlen(p)+1);}
static const char *section_name(const struct ld_input *o,Elf64_Shdr h) {
    Elf64_Shdr str;if(object_section(&o->object,o->object.h.e_shstrndx,&str) || str.sh_type!=SHT_STRTAB || h.sh_name>=str.sh_size)return NULL;
    const char *p=(const char *)o->data+str.sh_offset+h.sh_name;
    for(uint64_t i=h.sh_name;i<str.sh_size;i++)if(!o->data[str.sh_offset+i])return p;
    return NULL;
}
static int debug_name(const char *p) {
    return p && (!strncmp(p,".debug",6) || !strncmp(p,".zdebug",7) || !strncmp(p,".stab",5));
}
int ld_relocatable(struct ld_state *s,int common,unsigned char **bytes,size_t *length) {
    if(s->failed || !bytes || !length)return -1;
    struct buffer out={0},str={0},names={0},syms={0};struct section *sections=NULL;
    uint32_t **sm=NULL,**ym=NULL,*gm=NULL;uint64_t *commons=NULL;unsigned count=1,symindex=0,result=1;
    sm=calloc(s->input_count?s->input_count:1,sizeof(*sm));ym=calloc(s->input_count?s->input_count:1,sizeof(*ym));
    gm=calloc(s->symbol_count?s->symbol_count:1,sizeof(*gm));commons=calloc(s->symbol_count?s->symbol_count:1,sizeof(*commons));
    if(!sm || !ym || !gm || !commons)goto resource;
    for(size_t i=0;i<s->input_count;i++)if(s->inputs[i].selected) {
        struct ld_input *o=s->inputs+i;
        if(count+o->object.h.e_shnum+4>=SHN_LORESERVE) {fail(s,"too many relocatable sections");goto done;}
        count+=o->object.h.e_shnum;
        sm[i]=calloc(o->object.h.e_shnum,sizeof(**sm));ym[i]=calloc(o->symbol_count,sizeof(**ym));
        if(!sm[i] || !ym[i])goto resource;
    }
    sections=calloc(count+5,sizeof(*sections));if(!sections)goto resource;count=1;
    if(append(&names,"",1) || append(&str,"",1) || append(&syms,NULL,24))goto resource;
    for(size_t oi=0;oi<s->input_count;oi++) {
        struct ld_input *o=s->inputs+oi;if(!o->selected)continue;
        for(unsigned i=1;i<o->object.h.e_shnum;i++) {
            Elf64_Shdr h;object_section(&o->object,i,&h);
            if(h.sh_type==SHT_SYMTAB || h.sh_type==SHT_STRTAB)continue;
            if((h.sh_type!=SHT_PROGBITS && h.sh_type!=SHT_NOBITS && h.sh_type!=4 && h.sh_type!=7) ||
               h.sh_flags&~(uint64_t)(1|2|4|16|32|(h.sh_type==4?64:0)) || h.sh_addralign>LD_BYTE_LIMIT) {
                fail(s,"unsupported relocatable section");goto done;
            }
            const char *text=section_name(o,h);if(!text) {fail(s,"invalid section names");goto done;}
            if(s->strip&8) {
                if(debug_name(text))continue;
                if(h.sh_type==4) {
                    Elf64_Shdr target;
                    if(!object_section(&o->object,h.sh_info,&target) && debug_name(section_name(o,target)))continue;
                }
            }
            if(string(&names,text,&h.sh_name))goto resource;
            sm[oi][i]=count;sections[count++]=(struct section){h,h.sh_type==SHT_NOBITS?NULL:o->data+h.sh_offset};
        }
    }
    if(s->minimum_data) {
        uint64_t size=0;
        for(unsigned i=1;i<count;i++)if(sections[i].h.sh_type==SHT_PROGBITS && (sections[i].h.sh_flags&3)==3) {
            if(sections[i].h.sh_size>LD_BYTE_LIMIT-size)goto resource;
            size+=sections[i].h.sh_size;
        }
        if(size<s->minimum_data) {
            if(s->minimum_data>LD_BYTE_LIMIT)goto resource;
            Elf64_Shdr *h=&sections[count++].h;h->sh_type=SHT_PROGBITS;h->sh_flags=SHF_ALLOC|SHF_WRITE;h->sh_addralign=1;h->sh_size=s->minimum_data-size;
            if(string(&names,".data.padding",&h->sh_name))goto resource;
        }
    }
    /* Mark local symbols that relocations require before applying strip flags. */
    for(size_t oi=0;oi<s->input_count;oi++) {
        struct ld_input *o=s->inputs+oi;if(!o->selected)continue;
        for(unsigned i=1;i<o->object.h.e_shnum;i++) {
            Elf64_Shdr h;object_section(&o->object,i,&h);if(h.sh_type!=4 || !sm[oi][i])continue;
            if(h.sh_entsize!=24 || h.sh_size%24) {fail(s,"invalid relocation table");goto done;}
            for(uint64_t j=0;j<h.sh_size;j+=24) {
                struct rela r;memcpy(&r,o->data+h.sh_offset+j,24);uint64_t si=r.info>>32;
                if(si>=o->symbol_count) {fail(s,"invalid relocation symbol");goto done;}
                ym[oi][si]=1;
            }
        }
    }
    unsigned common_index=0;uint64_t common_size=0,common_align=1;
    if(common) {
        common_index=count++;Elf64_Shdr *h=&sections[common_index].h;
        h->sh_type=SHT_NOBITS;h->sh_flags=SHF_ALLOC|SHF_WRITE;
        if(string(&names,".bss.common",&h->sh_name))goto resource;
        for(size_t i=0;i<s->symbol_count;i++)if(s->symbols[i].common) {
            struct ld_symbol *g=s->symbols+i;
            if(g->alignment>LD_BYTE_LIMIT || g->size>LD_BYTE_LIMIT || common_size>LD_BYTE_LIMIT-(g->alignment-1))goto resource;
            common_size=(common_size+g->alignment-1)&~(g->alignment-1);
            if(g->size>LD_BYTE_LIMIT-common_size)goto resource;
            commons[i]=common_size;common_size+=g->size;if(g->alignment>common_align)common_align=g->alignment;
        }
        h->sh_size=common_size;h->sh_addralign=common_align;
    }
    for(size_t oi=0;oi<s->input_count;oi++) {
        struct ld_input *o=s->inputs+oi;if(!o->selected)continue;
        for(size_t i=1;i<o->symbol_count;i++) {
            Elf64_Sym n=o->symbols[i];if(ELF64_ST_BIND(n.st_info)!=STB_LOCAL)continue;
            if(n.st_shndx!=SHN_UNDEF && n.st_shndx!=SHN_ABS) {
                if(!sm[oi][n.st_shndx])continue;
                n.st_shndx=(Elf64_Half)sm[oi][n.st_shndx];
            }
            const char *text=ld_symbol_name(o,i);
            if((s->strip&1) || ((s->strip&2) && (*text=='L' || !strncmp(text,".L",2)))) {
                if(!ym[oi][i])continue;
                text="";
            }
            if(string(&str,text,&n.st_name))goto resource;
            ym[oi][i]=(uint32_t)(syms.n/24);if(append(&syms,&n,24))goto resource;
        }
    }
    unsigned first_global=(unsigned)(syms.n/24);
    for(size_t i=0;i<s->symbol_count;i++) {
        struct ld_symbol *g=s->symbols+i;Elf64_Sym n={0};
        if(g->defined) {
            if(g->object==LD_NONE) {n.st_shndx=SHN_ABS;n.st_value=g->value;}
            else {
                n=s->inputs[g->object].symbols[g->symbol];
                if(g->common) {
                    n.st_shndx=common?(Elf64_Half)common_index:SHN_COMMON;
                    n.st_value=common?commons[i]:g->alignment;n.st_size=g->size;
                } else if(n.st_shndx!=SHN_ABS) {
                    if(!sm[g->object][n.st_shndx]) {fail(s,"symbol in unsupported section");goto done;}
                    n.st_shndx=(Elf64_Half)sm[g->object][n.st_shndx];
                }
            }
        }
        n.st_info=ELF64_ST_INFO(g->bind,ELF64_ST_TYPE(n.st_info));
        if(string(&str,g->name,&n.st_name))goto resource;
        gm[i]=(uint32_t)(syms.n/24);if(append(&syms,&n,24))goto resource;
    }
    if(syms.n/24>LD_SYMBOL_LIMIT)goto resource;
    for(size_t oi=0;oi<s->input_count;oi++) {
        struct ld_input *o=s->inputs+oi;if(!o->selected)continue;
        for(size_t i=1;i<o->symbol_count;i++)if(ELF64_ST_BIND(o->symbols[i].st_info)!=STB_LOCAL) {
            size_t gi=ld_lookup(s,ld_symbol_name(o,i));if(gi==LD_NONE) {fail(s,"missing global symbol");goto done;}ym[oi][i]=gm[gi];
        }
    }
    symindex=count;count+=3;
    Elf64_Shdr *st=&sections[symindex].h,*strings=&sections[symindex+1].h,*sn=&sections[symindex+2].h;
    st->sh_type=SHT_SYMTAB;st->sh_link=symindex+1;st->sh_info=first_global;st->sh_entsize=24;st->sh_addralign=8;st->sh_size=syms.n;sections[symindex].data=syms.p;
    strings->sh_type=SHT_STRTAB;strings->sh_addralign=1;strings->sh_size=str.n;sections[symindex+1].data=str.p;
    if(string(&names,".symtab",&st->sh_name) || string(&names,".strtab",&strings->sh_name) || string(&names,".shstrtab",&sn->sh_name))goto resource;
    sn->sh_type=SHT_STRTAB;sn->sh_addralign=1;sn->sh_size=names.n;sections[symindex+2].data=names.p;
    if(append(&out,NULL,64))goto resource;
    for(unsigned i=1;i<count;i++) {
        Elf64_Shdr *h=&sections[i].h;uint64_t a=h->sh_addralign?h->sh_addralign:1;
        if(append(&out,NULL,(-out.n)&(a-1)))goto resource;
        h->sh_offset=out.n;h->sh_addr=0;
        if(h->sh_type!=SHT_NOBITS && append(&out,sections[i].data,(size_t)h->sh_size))goto resource;
    }
    for(size_t oi=0;oi<s->input_count;oi++) {
        struct ld_input *o=s->inputs+oi;if(!o->selected)continue;
        for(unsigned i=1;i<o->object.h.e_shnum;i++) {
            unsigned ni=sm[oi][i];if(!ni || sections[ni].h.sh_type!=4)continue;
            Elf64_Shdr *h=&sections[ni].h,old,target;object_section(&o->object,i,&old);
            if(!old.sh_info || object_section(&o->object,old.sh_info,&target) || !sm[oi][old.sh_info] ||
               old.sh_link>=o->object.h.e_shnum || h->sh_entsize!=24 || h->sh_size%24) {fail(s,"invalid relocation table");goto done;}
            Elf64_Shdr tab;object_section(&o->object,old.sh_link,&tab);
            if(tab.sh_type!=SHT_SYMTAB) {fail(s,"invalid relocation symbol table");goto done;}
            h->sh_link=symindex;h->sh_info=sm[oi][old.sh_info];
            for(uint64_t j=0;j<h->sh_size;j+=24) {
                struct rela r;memcpy(&r,out.p+h->sh_offset+j,24);uint64_t si=r.info>>32;unsigned type=(unsigned)r.info;
                unsigned width=type==0?0:type==1?8:(type==2 || type==4 || type==10 || type==11)?4:99;
                if(si>=o->symbol_count || width==99 || r.offset>target.sh_size || width>target.sh_size-r.offset ||
                   (width && target.sh_type==SHT_NOBITS)) {fail(s,"invalid/unsupported relocation");goto done;}
                r.info=((uint64_t)ym[oi][si]<<32)|type;memcpy(out.p+h->sh_offset+j,&r,24);
            }
        }
    }
    if(append(&out,NULL,(-out.n)&7))goto resource;
    Elf64_Ehdr e={0};memcpy(e.e_ident,ELFMAG,4);e.e_ident[EI_CLASS]=ELFCLASS64;e.e_ident[EI_DATA]=ELFDATA2LSB;e.e_ident[EI_VERSION]=1;
    e.e_type=ET_REL;e.e_machine=EM_X86_64;e.e_version=1;e.e_ehsize=64;e.e_shoff=out.n;e.e_shentsize=64;e.e_shnum=(Elf64_Half)count;e.e_shstrndx=(Elf64_Half)(symindex+2);
    memcpy(out.p,&e,64);for(unsigned i=0;i<count;i++)if(append(&out,&sections[i].h,64))goto resource;
    *bytes=out.p;*length=out.n;out.p=NULL;result=0;goto done;
resource:
    fail(s,"relocatable output resource limit or allocation failure");
done:
    if(sm)for(size_t i=0;i<s->input_count;i++)free(sm[i]);
    if(ym)for(size_t i=0;i<s->input_count;i++)free(ym[i]);
    free(sm);free(ym);free(gm);free(commons);free(sections);free(out.p);free(str.p);free(names.p);free(syms.p);return result?-1:0;
}
