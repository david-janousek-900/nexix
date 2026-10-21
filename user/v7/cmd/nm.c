/* V7 nm selection/sorting modes over the native ELF/archive contract. */
#include "object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int numeric,undefined,reverse,global,unsorted,prefix,multiple;
static int compare(const void *left,const void *right)
{
    const struct object_symbol *a=left,*b=right;
    int n=numeric?(a->value>b->value)-(a->value<b->value):0;
    if (!n) n=strcmp(a->name,b->name);
    return reverse?-n:n;
}
static int display(const char *path,const char *member,const unsigned char *bytes,size_t size,void *context)
{
    (void)context;struct object o;struct object_symbol *symbols;size_t n;
    if (member && (size<4 || memcmp(bytes,ELFMAG,4))) return 0;
    if (object_open(&o,bytes,size) || object_symbols(&o,&symbols,&n)) return -1;
    if (!unsorted) qsort(symbols,n,sizeof(*symbols),compare);
    if (!prefix && (member || multiple)) printf("\n%s:\n",member?member:path);
    for (size_t i=0;i<n;++i) {
        struct object_symbol *s=symbols+i;
        if (global && s->bind==STB_LOCAL) continue;
        if (undefined && !strchr("Uuwv",s->type)) continue;
        if (prefix) { printf("%s:",path);if (member) printf("%s:",member); }
        if (!undefined) {
            if (strchr("Uuwv",s->type)) printf("                ");
            else printf("%016lx",(unsigned long)s->value);
            printf(" %c ",s->type);
        }
        printf("%s\n",s->name);
    }
    free(symbols);return ferror(stdout)?-1:0;
}
int main(int argc,char **argv)
{
    int first=1,result=0;
    while (first<argc && argv[first][0]=='-' && argv[first][1]) {
        if (!strcmp(argv[first],"--")) { ++first;break; }
        for (const char *p=argv[first]+1;*p;++p) switch (*p) {
            case 'n':numeric=1;break;case 'u':undefined=1;break;case 'r':reverse=1;break;
            case 'g':global=1;break;case 'p':unsorted=1;break;case 'o':prefix=1;break;
            default:fprintf(stderr,"usage: nm [-goprun] [file ...]\n");return 2;
        }
        ++first;
    }
    multiple=argc-first>1;
    if (first==argc) { if (object_files("a.out",display,NULL)) { fprintf(stderr,"nm: cannot read object/archive a.out\n");result=1; } }
    for (int i=first;i<argc;++i) if (object_files(argv[i],display,NULL)) {
        fprintf(stderr,"nm: cannot read object/archive %s\n",argv[i]);result=1;
    }
    return fflush(stdout)==EOF?1:result;
}
