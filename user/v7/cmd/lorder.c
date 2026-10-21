/* Native implementation of the V7 lorder symbol-definition/reference join. */
#include "object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
struct entry { char *symbol;size_t unit;int definition; };
static struct entry *entries;
static char **units;
static size_t nentries,nunits,capacity;
static char *copy(const char *s)
{ size_t n=strlen(s)+1;char *p=malloc(n);if(p)memcpy(p,s,n);return p; }
static int token(const char *s)
{ for (;*s;++s) if (isspace((unsigned char)*s) || (unsigned char)*s==127) return 0;return 1; }
static int collect(const char *path,const char *member,const unsigned char *bytes,size_t length,void *unused)
{
    (void)unused;struct object o;struct object_symbol *symbols;size_t n;
    if (member && (length<4 || memcmp(bytes,ELFMAG,4))) return 0;
    if (!token(path) || (member && !token(member)) || object_open(&o,bytes,length) || object_symbols(&o,&symbols,&n)) return -1;
    if (nunits==1024) { free(symbols);return -1; }
    char **next=realloc(units,(nunits+1)*sizeof(*units));
    if (!next) {free(symbols);return -1;}units=next;
    size_t bytes_needed=strlen(path)+(member?strlen(member)+2:0)+1;
    units[nunits]=malloc(bytes_needed);
    if (!units[nunits]) {free(symbols);return -1;}
    if (member) snprintf(units[nunits],bytes_needed,"%s(%s)",path,member);
    else memcpy(units[nunits],path,bytes_needed);
    size_t unit=nunits++;
    for (size_t i=0;i<n;++i) {
        struct object_symbol *s=symbols+i;
        if (s->bind==STB_LOCAL || s->type=='f' || s->type=='F') continue;
        if (nentries==100000) {free(symbols);return -1;}
        if (nentries==capacity) {
            size_t cap=capacity?capacity*2:64;
            if (cap>100000) cap=100000;
            struct entry *p=realloc(entries,cap*sizeof(*entries));
            if (!p) {free(symbols);return -1;}entries=p;capacity=cap;
        }
        char *name=copy(s->name);if (!name) {free(symbols);return -1;}
        entries[nentries++]=(struct entry){name,unit,strchr("Uuwv",s->type)==NULL};
    }
    free(symbols);return 0;
}
static int compare(const void *a,const void *b)
{
    const struct entry *x=a,*y=b;
    int n=strcmp(x->symbol,y->symbol);
    if (!n) n=(x->unit>y->unit)-(x->unit<y->unit);
    return n;
}
int main(int argc,char **argv)
{
    int status=0;
    if (argc==1) { puts("usage: lorder file ...");return 0; }
    for (int i=1;i<argc;++i) if (!token(argv[i])) {
        fprintf(stderr,"lorder: whitespace in object names is unsupported\n");return 2;
    }
    for (int i=1;i<argc;++i) if (object_files(argv[i],collect,NULL)) {
        fprintf(stderr,"lorder: cannot read object/archive %s\n",argv[i]);status=1;
    }
    /* Preserve the original single-.o script's duplicate self-pair. */
    if (argc==2 && strlen(argv[1])>=2 && !strcmp(argv[1]+strlen(argv[1])-2,".o") && nunits==1)
        printf("%s %s\n",units[0],units[0]);
    for (size_t i=0;i<nunits;++i) printf("%s %s\n",units[i],units[i]);
    if (nentries>1) qsort(entries,nentries,sizeof(*entries),compare);
    for (size_t first=0;first<nentries;) {
        size_t end=first+1;
        while(end<nentries && !strcmp(entries[first].symbol,entries[end].symbol)) ++end;
        for (size_t i=first;i<end;++i) if (!entries[i].definition)
            for (size_t j=first;j<end;++j) if (entries[j].definition && entries[i].unit!=entries[j].unit)
                printf("%s %s\n",units[entries[i].unit],units[entries[j].unit]);
        first=end;
    }
    for(size_t i=0;i<nentries;++i) free(entries[i].symbol);
    for(size_t i=0;i<nunits;++i) free(units[i]);
    free(entries);free(units);
    return fflush(stdout)==EOF?1:status;
}
