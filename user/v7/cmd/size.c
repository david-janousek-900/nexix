/* V7 text+data+bss report over native ELF section and archive records. */
#include "object.h"
#include <stdio.h>
#include <string.h>
static int multiple;
static int display(const char *path,const char *member,const unsigned char *bytes,size_t length,void *unused)
{
    (void)unused;struct object o;uint64_t text,data,bss,total;
    if (member && (length<4 || memcmp(bytes,ELFMAG,4))) return 0;
    if (object_open(&o,bytes,length) || object_sizes(&o,&text,&data,&bss) ||
        data>UINT64_MAX-text || bss>UINT64_MAX-text-data) return -1;
    total=text+data+bss;
    if (member) printf("%s(%s): ",path,member);
    else if (multiple) printf("%s: ",path);
    printf("%lu+%lu+%lu = %lub = 0%lob\n",(unsigned long)text,(unsigned long)data,
           (unsigned long)bss,(unsigned long)total,(unsigned long)total);
    return ferror(stdout)?-1:0;
}
int main(int argc,char **argv)
{
    int first=1,result=0;
    if (argc>1 && !strcmp(argv[1],"--")) ++first;
    else if (argc>1 && argv[1][0]=='-') {
        fprintf(stderr,"usage: size [file ...]\n");return 2;
    }
    multiple=argc-first>1;
    if (first==argc) {
        if (object_files("a.out",display,NULL)) { fprintf(stderr,"size: cannot read object/archive a.out\n");result=1; }
    }
    for (int i=first;i<argc;++i) if (object_files(argv[i],display,NULL)) {
        fprintf(stderr,"size: cannot read object/archive %s\n",argv[i]);result=1;
    }
    return fflush(stdout)==EOF?1:result;
}
