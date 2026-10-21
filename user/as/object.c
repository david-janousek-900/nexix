/* ELF64 relocatable layout and serialization. No host tools at runtime. */
#include "as.h"
#include "encode.h"
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum {RELATYPE=4,R64=1,PC32=2,PLT32=4,R32=10,R32S=11};
struct rela {uint64_t offset,info;int64_t addend;};
_Static_assert(sizeof(struct rela)==24,"ELF64 relocation layout");
struct buffer {unsigned char *data;size_t size,capacity;};
struct output_section {struct buffer bytes,relocations;uint64_t size,alignment;};
struct layout {struct as_program *p;struct output_section *s;size_t *offsets,*symmap;unsigned char *state;unsigned depth;};
struct term {uint64_t value;size_t section,symbol;int reference;};
static int append(struct buffer *b,const void *data,size_t n) {
    if(!n)return 0;
    if(n>AS_LIMIT-b->size)return -1;
    size_t needed=b->size+n;
    if(needed>b->capacity){size_t cap=b->capacity?b->capacity:64;while(cap<needed){if(cap>AS_LIMIT/2){cap=AS_LIMIT;break;}cap*=2;}void *p=realloc(b->data,cap);if(!p)return -1;b->data=p;b->capacity=cap;}
    if(data)memcpy(b->data+b->size,data,n);else memset(b->data+b->size,0,n);
    b->size=needed;return 0;
}
static int pad(struct buffer *b,size_t alignment) {
    if(!alignment||alignment>AS_LIMIT||(alignment&(alignment-1)))return -1;
    return append(b,NULL,(-b->size)&(alignment-1));
}
static int string(struct buffer *b,const char *s,uint32_t *off) {if(b->size>UINT32_MAX)return -1;*off=b->size;return append(b,s,strlen(s)+1);}
static int term(struct layout *,size_t,size_t,uint64_t,struct term *);
static int evaluate(struct layout *l,struct as_expr e,size_t section,uint64_t dot,struct term *plus,struct term *minus,uint64_t *value) {
    *value=e.value;*plus=(struct term){0,AS_NONE,AS_NONE,0};*minus=*plus;
    if(e.plus!=AS_NONE){if(term(l,e.plus,section,dot,plus))return -1;if(!plus->reference){*value+=plus->value;plus->value=0;}}
    if(e.minus!=AS_NONE){if(term(l,e.minus,section,dot,minus))return -1;if(!minus->reference){*value-=minus->value;minus->value=0;}}
    if(plus->reference&&minus->reference&&plus->section!=AS_NONE&&plus->section==minus->section) {*value+=plus->value-minus->value;plus->reference=minus->reference=0;}
    return 0;
}
static int term_impl(struct layout *l,size_t index,size_t section,uint64_t dot,struct term *v) {
    if(index==AS_DOT){*v=(struct term){dot,section,AS_DOT,1};return 0;}
    if(index>=l->p->symbol_count)return -1;
    struct as_symbol *s=l->p->symbols+index;
    if(s->defined==2&&l->state[index]!=2) {
        if(l->state[index]==1||s->record>=l->p->count)return -1;
        l->state[index]=1;
        struct as_record *r=l->p->records+s->record;struct term a,b;uint64_t n;
        if(evaluate(l,s->expression,r->section,l->offsets[s->record],&a,&b,&n)||b.reference)return -1;
        if(a.reference){if(a.section==AS_NONE)return -1;n+=a.value;s->section=a.section;}else s->section=AS_NONE;
        s->value=n;l->state[index]=2;
    }
    if(s->defined==2&&s->section==AS_NONE)*v=(struct term){s->value,AS_NONE,index,0};
    else if(s->defined&&(!s->common||!s->binding))*v=(struct term){s->value,s->section,index,1};
    else *v=(struct term){0,AS_NONE,index,1};
    return 0;
}
static int term(struct layout *l,size_t index,size_t section,uint64_t dot,struct term *v) {
    if(++l->depth>128){l->depth--;return -1;}
    int result=term_impl(l,index,section,dot,v);l->depth--;return result;
}
static int absolute_value(struct layout *l,struct as_expr e,size_t section,uint64_t dot,uint64_t *out) {
    struct term a,b;if(evaluate(l,e,section,dot,&a,&b,out)||a.reference||b.reference)return -1;return 0;
}
static int grow_size(struct output_section *s,uint64_t n) {if(n>AS_LIMIT-s->size)return -1;s->size+=n;return 0;}
static int bss(struct as_program *p,size_t *out) {
    for(size_t i=0;i<p->section_count;i++)if(!strcmp(p->sections[i].name,".bss")){*out=i;return p->sections[i].type==8?0:-1;}
    if(p->section_count>=256)return -1;
    void *next=realloc(p->sections,(p->section_count+1)*sizeof(*p->sections));if(!next)return -1;p->sections=next;
    char *name=malloc(5);if(!name)return -1;memcpy(name,".bss",5);
    *out=p->section_count;p->sections[p->section_count++]=(struct as_section){name,3,1,8};return 0;
}
static int layout(struct layout *l) {
    struct as_program *p=l->p;
    size_t bs=AS_NONE;
    for(size_t i=0;i<p->symbol_count;i++)if(p->symbols[i].common&&!p->symbols[i].binding) {if(bs==AS_NONE&&bss(p,&bs))return -1;}
    l->s=calloc(p->section_count,sizeof(*l->s));l->offsets=calloc(p->count?p->count:1,sizeof(*l->offsets));l->symmap=calloc(p->symbol_count?p->symbol_count:1,sizeof(*l->symmap));l->state=calloc(p->symbol_count?p->symbol_count:1,1);
    if(!l->s||!l->offsets||!l->symmap||!l->state)return -1;
    for(size_t i=0;i<p->section_count;i++)l->s[i].alignment=p->sections[i].alignment;
    for(size_t i=0;i<p->count;i++) {
        struct as_record *r=p->records+i;if(r->section>=p->section_count)return -1;
        struct output_section *s=l->s+r->section;l->offsets[i]=s->size;
        switch(r->kind) {
        case AS_LABEL:p->symbols[r->symbol].value=s->size;break;
        case AS_INSTRUCTION:{struct as_code code;if(p->sections[r->section].type==8||as_encode(r,&code)||grow_size(s,code.length))return -1;break;}
        case AS_DATA:if(grow_size(s,r->width))return -1;break;
        case AS_BYTES:if(grow_size(s,r->length))return -1;break;
        case AS_SPACE:if(grow_size(s,r->expression.value))return -1;break;
        case AS_ALIGN:{uint64_t n=r->expression.value;if(!n||n>AS_LIMIT||(n&(n-1))||grow_size(s,(-s->size)&(n-1)))return -1;if(n>s->alignment)s->alignment=n;break;}
        default:break;
        }
    }
    for(size_t i=0;i<p->symbol_count;i++) {
        struct as_symbol *s=p->symbols+i;
        if(s->common) {
            if(s->size>AS_LIMIT||!s->alignment||s->alignment>AS_LIMIT||(s->alignment&(s->alignment-1)))return -1;
            if(!s->binding) {
                struct output_section *out=l->s+bs;
                if(grow_size(out,(-out->size)&(s->alignment-1)))return -1;
                s->value=out->size;
                if(grow_size(out,s->size))return -1;
                if(s->alignment>out->alignment)out->alignment=s->alignment;
                s->section=bs;s->defined=1;s->type=STT_OBJECT;
            }
        }
        if(!s->defined&&((s->binding_explicit&&!s->binding)||!strncmp(s->name,".L",2)||!strncmp(s->name,".N",2)))return -1;
    }
    for(size_t i=0;i<p->symbol_count;i++)if(p->symbols[i].defined==2){struct term v;if(term(l,i,0,0,&v))return -1;}
    for(size_t i=0;i<p->count;i++)if(p->records[i].kind==AS_SIZE) {
        struct as_record *r=p->records+i;uint64_t n;if(absolute_value(l,r->expression,r->section,l->offsets[i],&n))return -1;p->symbols[r->symbol].size=n;
    }
    for(size_t i=0;i<p->section_count;i++)if(p->sections[i].type!=8&&append(&l->s[i].bytes,NULL,l->s[i].size))return -1;
    return 0;
}
static int fits(uint64_t value,unsigned type) {
    if(type==R32)return value<=UINT32_MAX||((int64_t)value<0&&(int64_t)value>=INT32_MIN);
    if(type==R32S||type==PC32||type==PLT32)return (int64_t)value>=INT32_MIN&&(int64_t)value<=INT32_MAX;
    return 1;
}
static int emit_value(struct layout *l,size_t section,size_t at,unsigned width,unsigned type,unsigned bias,struct as_expr e,uint64_t dot) {
    struct term a,b;uint64_t n;
    if(evaluate(l,e,section,dot,&a,&b,&n))return -1;
    if(l->p->sections[section].type==8)return a.reference||b.reference||n?-1:0;
    int pc=type==PC32||type==PLT32;
    if(b.reference) {
        if(!a.reference||b.section!=section||width!=4||pc)return -1;
        n+=at-b.value;type=PC32;pc=1;b.reference=0;
    }
    if(a.reference&&pc&&a.section==section&&(a.symbol==AS_DOT||!l->p->symbols[a.symbol].binding)) {n+=a.value-at;a.reference=0;}
    else if(!a.reference&&pc) {
        if(type==PC32&&bias)n+=bias;
        else {
            if(width!=4)return -1;
            struct rela relocation={at,PC32,(int64_t)n};
            return append(&l->s[section].relocations,&relocation,sizeof(relocation));
        }
    }
    if(a.reference) {
        if(width!=4&&width!=8)return -1;
        size_t symbol;
        if(a.symbol==AS_DOT){symbol=a.section+1;n+=a.value;}
        else symbol=l->symmap[a.symbol];
        struct rela relocation={at,((uint64_t)symbol<<32)|type,(int64_t)n};
        return append(&l->s[section].relocations,&relocation,sizeof(relocation));
    }
    if(!fits(n,type)&&width>=4)return -1;
    if(l->p->sections[section].type==8)return n? -1:0;
    if(at>l->s[section].bytes.size||width>l->s[section].bytes.size-at)return -1;
    for(unsigned i=0;i<width;i++)l->s[section].bytes.data[at+i]=(unsigned char)(n>>(8*i));
    return 0;
}
static int emit(struct layout *l) {
    for(size_t i=0;i<l->p->count;i++) {
        struct as_record *r=l->p->records+i;struct output_section *s=l->s+r->section;size_t at=l->offsets[i];int nobits=l->p->sections[r->section].type==8;
        if(r->kind==AS_DATA) {if(emit_value(l,r->section,at,r->width,r->width==8?R64:R32,0,r->expression,at))return -1;}
        else if(r->kind==AS_BYTES){if(nobits){for(size_t j=0;j<r->length;j++)if(r->bytes[j])return -1;}else if(r->length)memcpy(s->bytes.data+at,r->bytes,r->length);}
        else if(r->kind==AS_SPACE||r->kind==AS_ALIGN) {
            size_t count=r->kind==AS_SPACE?r->expression.value:(-at)&(r->expression.value-1);
            unsigned fill=r->width;
            if(r->kind==AS_ALIGN&&!r->count&&(l->p->sections[r->section].flags&4))fill=0x90;
            if(nobits){if(fill)return -1;}else if(count)memset(s->bytes.data+at,fill,count);
        } else if(r->kind==AS_INSTRUCTION) {
            struct as_code code;if(as_encode(r,&code))return -1;memcpy(s->bytes.data+at,code.bytes,code.length);
            for(unsigned j=0;j<code.fixup_count;j++){struct as_fixup *f=code.fixups+j;if(emit_value(l,r->section,at+f->offset,f->width,f->type,f->pc_bias,f->expression,at))return -1;}
        }
    }
    return 0;
}
static int symbols(struct layout *l,struct buffer *symbols,struct buffer *strings,unsigned *first_global) {
    Elf64_Sym sym={0};if(append(symbols,&sym,sizeof(sym))||append(strings,"",1))return -1;
    for(size_t i=0;i<l->p->section_count;i++){memset(&sym,0,sizeof(sym));sym.st_info=ELF64_ST_INFO(STB_LOCAL,STT_SECTION);sym.st_shndx=i+1;if(append(symbols,&sym,sizeof(sym)))return -1;}
    for(unsigned pass=0;pass<2;pass++) {
        if(pass)*first_global=symbols->size/24;
        for(size_t i=0;i<l->p->symbol_count;i++) {
            struct as_symbol *s=l->p->symbols+i;unsigned bind=s->binding;
            if(!s->defined&&!s->binding_explicit)bind=STB_GLOBAL;
            if((bind!=STB_LOCAL)!=pass)continue;
            memset(&sym,0,sizeof(sym));if(string(strings,s->name,&sym.st_name))return -1;
            sym.st_info=ELF64_ST_INFO(bind,s->type?s->type:s->common?STT_OBJECT:STT_NOTYPE);sym.st_size=s->size;
            if(s->common&&bind!=STB_LOCAL){sym.st_shndx=SHN_COMMON;sym.st_value=s->alignment;}
            else if(s->defined){sym.st_shndx=s->section==AS_NONE?SHN_ABS:s->section+1;sym.st_value=s->value;}
            l->symmap[i]=symbols->size/24;if(append(symbols,&sym,sizeof(sym)))return -1;
        }
    }
    return 0;
}
int as_object(struct as_program *p,unsigned char **bytes,size_t *length) {
    *bytes=NULL;*length=0;struct layout l={0};l.p=p;
    struct buffer output={0},syms={0},names={0},strings={0};Elf64_Shdr *headers=NULL;int result=-1;
    if(p->errors||layout(&l))goto out;
    unsigned global;if(symbols(&l,&syms,&strings,&global)||emit(&l))goto out;
    unsigned relcount=0;for(size_t i=0;i<p->section_count;i++)if(l.s[i].relocations.size)relcount++;
    unsigned symindex=1+p->section_count+relcount,strindex=symindex+1,nameindex=symindex+2,count=symindex+3;
    headers=calloc(count,sizeof(*headers));if(!headers||append(&names,"",1))goto out;
    Elf64_Ehdr h={0};memcpy(h.e_ident,ELFMAG,4);h.e_ident[EI_CLASS]=ELFCLASS64;h.e_ident[EI_DATA]=ELFDATA2LSB;h.e_ident[EI_VERSION]=1;
    h.e_type=ET_REL;h.e_machine=EM_X86_64;h.e_version=1;h.e_ehsize=64;h.e_shentsize=64;h.e_shnum=count;h.e_shstrndx=nameindex;
    if(append(&output,&h,sizeof(h)))goto out;
    for(size_t i=0;i<p->section_count;i++) {
        Elf64_Shdr *s=headers+i+1;struct output_section *data=l.s+i;
        if(string(&names,p->sections[i].name,&s->sh_name)||pad(&output,data->alignment))goto out;
        s->sh_type=p->sections[i].type;s->sh_flags=p->sections[i].flags;s->sh_offset=output.size;s->sh_size=data->size;s->sh_addralign=data->alignment;
        if(s->sh_type!=8&&append(&output,data->bytes.data,data->bytes.size))goto out;
    }
    unsigned next=1+p->section_count;
    for(size_t i=0;i<p->section_count;i++)if(l.s[i].relocations.size) {
        char name[280];snprintf(name,sizeof(name),".rela%s",p->sections[i].name);Elf64_Shdr *s=headers+next++;
        if(string(&names,name,&s->sh_name)||pad(&output,8))goto out;
        s->sh_type=RELATYPE;s->sh_offset=output.size;s->sh_size=l.s[i].relocations.size;s->sh_link=symindex;s->sh_info=i+1;s->sh_addralign=8;s->sh_entsize=24;
        if(append(&output,l.s[i].relocations.data,l.s[i].relocations.size))goto out;
    }
    Elf64_Shdr *s=headers+symindex;if(string(&names,".symtab",&s->sh_name)||pad(&output,8))goto out;
    s->sh_type=SHT_SYMTAB;s->sh_offset=output.size;s->sh_size=syms.size;s->sh_link=strindex;s->sh_info=global;s->sh_addralign=8;s->sh_entsize=24;
    if(append(&output,syms.data,syms.size))goto out;
    s=headers+strindex;if(string(&names,".strtab",&s->sh_name))goto out;
    s->sh_type=SHT_STRTAB;s->sh_offset=output.size;s->sh_size=strings.size;s->sh_addralign=1;if(append(&output,strings.data,strings.size))goto out;
    s=headers+nameindex;if(string(&names,".shstrtab",&s->sh_name))goto out;
    s->sh_type=SHT_STRTAB;s->sh_offset=output.size;s->sh_size=names.size;s->sh_addralign=1;if(append(&output,names.data,names.size)||pad(&output,8))goto out;
    h.e_shoff=output.size;if(append(&output,headers,(size_t)count*64))goto out;memcpy(output.data,&h,sizeof(h));
    *bytes=output.data;*length=output.size;output.data=NULL;result=0;
out:
    if(l.s)for(size_t i=0;i<p->section_count;i++){free(l.s[i].bytes.data);free(l.s[i].relocations.data);}
    free(l.s);free(l.offsets);free(l.symmap);free(l.state);free(output.data);free(syms.data);free(names.data);free(strings.data);free(headers);return result;
}
