/* ELF64 adaptation of V7 ld.c load1/ldrand/lookup/enter resolution.
 * The original two-stage selection and per-archive fixed-point search remain;
 * PDP-11 record overlays and eight-character symbols are replaced explicitly.
 */
#include "ld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define HASH_SIZE 65537
static int fail(struct ld_state *s,const char *fmt,...) {
    if (!s->failed) {
        va_list ap;va_start(ap,fmt);vsnprintf(s->error,sizeof(s->error),fmt,ap);va_end(ap);
    }
    s->failed=1;return -1;
}
static char *copy(const char *p) {
    size_t n=strlen(p)+1;char *r=malloc(n);if(r)memcpy(r,p,n);return r;
}
static uint32_t hash_name(const char *name) {
    uint32_t h=2166136261U;
    for(;*name;name++)h=(h^(unsigned char)*name)*16777619U;
    return h%HASH_SIZE;
}
void ld_init(struct ld_state *s) { memset(s,0,sizeof(*s)); }
void ld_free(struct ld_state *s) {
    for(size_t i=0;i<s->input_count;i++) {
        struct ld_input *o=s->inputs+i;
        free(o->data);free(o->path);free(o->member);free(o->symbols);
    }
    for(size_t i=0;i<s->symbol_count;i++)free((void *)s->symbols[i].name);
    free(s->inputs);free(s->symbols);free(s->hash);ld_init(s);
}
size_t ld_lookup(const struct ld_state *s,const char *name) {
    if(!s->hash)return LD_NONE;
    uint32_t slot=hash_name(name);
    while(s->hash[slot]) {
        size_t i=s->hash[slot]-1;
        if(!strcmp(s->symbols[i].name,name))return i;
        slot=(slot+1)%HASH_SIZE;
    }
    return LD_NONE;
}
static size_t intern(struct ld_state *s,const char *name) {
    size_t i=ld_lookup(s,name);if(i!=LD_NONE)return i;
    if(s->symbol_count==LD_SYMBOL_LIMIT) {fail(s,"too many global symbols");return LD_NONE;}
    if(!s->hash) {s->hash=calloc(HASH_SIZE,sizeof(*s->hash));if(!s->hash)goto memory;}
    if(s->symbol_count==s->symbol_capacity) {
        size_t cap=s->symbol_capacity?s->symbol_capacity*2:128;
        void *p=realloc(s->symbols,cap*sizeof(*s->symbols));if(!p)goto memory;
        s->symbols=p;s->symbol_capacity=cap;
    }
    char *text=copy(name);if(!text)goto memory;
    i=s->symbol_count++;
    s->symbols[i]=(struct ld_symbol){.name=text,.object=LD_NONE,.symbol=LD_NONE};
    uint32_t slot=hash_name(name);while(s->hash[slot])slot=(slot+1)%HASH_SIZE;
    s->hash[slot]=(uint32_t)i+1;return i;
memory:
    fail(s,"out of memory");return LD_NONE;
}
int ld_require(struct ld_state *s,const char *name) {
    if(s->failed)return -1;
    if(!*name)return fail(s,"empty required symbol");
    size_t i=intern(s,name);if(i==LD_NONE)return -1;
    s->symbols[i].required=1;return 0;
}
const char *ld_symbol_name(const struct ld_input *o,size_t i) {
    return (const char *)o->data+o->strings.sh_offset+o->symbols[i].st_name;
}
static int zero_name(const unsigned char *p,uint64_t n) {
    for(uint64_t i=0;i<n;i++)if(!p[i])return 1;
    return 0;
}
static int validate(struct ld_state *s,struct ld_input *o) {
    struct object *e=&o->object;
    if(object_open(e,o->data,e->size) || e->h.e_type!=ET_REL || e->h.e_phnum || !e->h.e_shnum)
        return fail(s,"%s: invalid ELF64 relocatable object",o->path);
    unsigned tables=0;
    for(unsigned i=0;i<e->h.e_shnum;i++) {
        Elf64_Shdr h;object_section(e,i,&h);
        if(i==0 && (h.sh_type!=SHT_NULL || h.sh_size))return fail(s,"%s: invalid null section",o->path);
        if(h.sh_type==SHT_SYMTAB) {o->symtab=h;tables++;}
    }
    Elf64_Shdr *t=&o->symtab;
    if(tables!=1 || t->sh_entsize!=sizeof(Elf64_Sym) || t->sh_size%sizeof(Elf64_Sym) ||
       !t->sh_size || object_section(e,t->sh_link,&o->strings) || o->strings.sh_type!=SHT_STRTAB ||
       !o->strings.sh_size || o->data[o->strings.sh_offset] || t->sh_info>t->sh_size/sizeof(Elf64_Sym))
        return fail(s,"%s: invalid symbol table",o->path);
    o->symbol_count=(size_t)(t->sh_size/sizeof(Elf64_Sym));
    if(o->symbol_count>LD_SYMBOL_LIMIT)return fail(s,"%s: too many object symbols",o->path);
    o->symbols=malloc((size_t)t->sh_size);if(!o->symbols)return fail(s,"out of memory");
    memcpy(o->symbols,o->data+t->sh_offset,(size_t)t->sh_size);
    Elf64_Sym empty={0};if(memcmp(o->symbols,&empty,sizeof(empty)))return fail(s,"%s: invalid null symbol",o->path);
    for(size_t i=1;i<o->symbol_count;i++) {
        Elf64_Sym *n=o->symbols+i;unsigned b=ELF64_ST_BIND(n->st_info),k=ELF64_ST_TYPE(n->st_info);
        if(n->st_name>=o->strings.sh_size ||
           !zero_name(o->data+o->strings.sh_offset+n->st_name,o->strings.sh_size-n->st_name) ||
           b>STB_WEAK || k>STT_COMMON || n->st_other&~3U || (i<t->sh_info)!=(b==STB_LOCAL))
            return fail(s,"%s: invalid/unsupported symbol",o->path);
        if(b!=STB_LOCAL && !*ld_symbol_name(o,i))return fail(s,"%s: unnamed global symbol",o->path);
        if(n->st_shndx==SHN_COMMON) {
            if(b==STB_LOCAL || (n->st_value && (n->st_value&(n->st_value-1))))
                return fail(s,"%s: invalid common alignment/binding",o->path);
        } else if(n->st_shndx!=SHN_ABS && n->st_shndx!=SHN_UNDEF) {
            Elf64_Shdr h;
            if(n->st_shndx>=SHN_LORESERVE || object_section(e,n->st_shndx,&h) ||
               n->st_value>h.sh_size || n->st_size>h.sh_size-n->st_value)
                return fail(s,"%s: invalid symbol section/range",o->path);
        }
    }
    return 0;
}
static int load(const char *path,const char *member,const unsigned char *bytes,size_t size,void *ctx) {
    struct ld_state *s=ctx;
    if(s->failed)return -1;
    if(s->input_count==LD_OBJECT_LIMIT || size>LD_BYTE_LIMIT-s->bytes)return fail(s,"link input limit exceeded");
    if(s->input_count==s->input_capacity) {
        size_t cap=s->input_capacity?s->input_capacity*2:16;
        void *p=realloc(s->inputs,cap*sizeof(*s->inputs));if(!p)return fail(s,"out of memory");
        s->inputs=p;s->input_capacity=cap;
    }
    struct ld_input *o=s->inputs+s->input_count++;memset(o,0,sizeof(*o));
    o->path=copy(path);o->member=member?copy(member):NULL;o->data=malloc(size?size:1);
    if(!o->path || (member&&!o->member) || !o->data)return fail(s,"out of memory");
    memcpy(o->data,bytes,size);o->object.size=size;s->bytes+=size;
    return validate(s,o);
}
static int merge(struct ld_state *s,size_t oi) {
    struct ld_input *o=s->inputs+oi;o->selected=1;
    for(size_t j=1;j<o->symbol_count;j++) {
        Elf64_Sym *n=o->symbols+j;unsigned b=ELF64_ST_BIND(n->st_info);
        if(b==STB_LOCAL)continue;
        size_t i=intern(s,ld_symbol_name(o,j));if(i==LD_NONE)return -1;
        struct ld_symbol *g=s->symbols+i;
        if(n->st_shndx==SHN_UNDEF) {
            if(b!=STB_WEAK)g->required=1;
            if(!g->defined)g->bind=g->required?STB_GLOBAL:STB_WEAK;
            continue;
        }
        int common=n->st_shndx==SHN_COMMON;
        if(g->defined && g->bind==STB_GLOBAL && b==STB_GLOBAL && !g->common && !common)
            return fail(s,"multiple definition: %s",g->name);
        if(g->defined && g->common && common && g->bind==b) {
            if(n->st_size>g->size)g->size=n->st_size;
            if(n->st_value>g->alignment)g->alignment=n->st_value;
            continue;
        }
        if(g->defined && (g->bind<b || (g->bind==b && (!g->common || common))))continue;
        g->defined=1;g->common=common;g->bind=b;g->object=oi;g->symbol=j;
        g->size=n->st_size;g->alignment=n->st_value?n->st_value:1;
    }
    return 0;
}
static int needed(const struct ld_state *s,const struct ld_input *o) {
    for(size_t j=1;j<o->symbol_count;j++) {
        const Elf64_Sym *n=o->symbols+j;
        if(ELF64_ST_BIND(n->st_info)==STB_LOCAL || n->st_shndx==SHN_UNDEF)continue;
        size_t i=ld_lookup(s,ld_symbol_name(o,j));if(i==LD_NONE)continue;
        const struct ld_symbol *g=s->symbols+i;
        if((!g->defined && g->required) ||
           (g->defined && g->common && n->st_shndx!=SHN_COMMON && ELF64_ST_BIND(n->st_info)==STB_GLOBAL))return 1;
    }
    return 0;
}
int ld_add_file(struct ld_state *s,const char *path) {
    if(s->failed)return -1;
    size_t start=s->input_count;
    if(object_files(path,load,s))return fail(s,"%s: cannot read object/archive",path);
    int changed;
    do {
        changed=0;
        for(size_t i=start;i<s->input_count;i++) {
            struct ld_input *o=s->inputs+i;
            if(!o->selected && (!o->member || needed(s,o))) {
                if(merge(s,i))return -1;
                changed=1;
            }
        }
    } while(changed);
    return 0;
}
int ld_check(struct ld_state *s) {
    if(s->failed)return -1;
    for(size_t i=0;i<s->symbol_count;i++)
        if(s->symbols[i].required && !s->symbols[i].defined)return fail(s,"undefined symbol: %s",s->symbols[i].name);
    return 0;
}
