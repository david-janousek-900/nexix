/* Native linker command; consumes only native ELF64 objects and archives. */
#include "ld.h"
#include "archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define PATH_LIMIT 4096
struct action {const char *name;int kind;};
static int usage(void) {
    fprintf(stderr,"usage: ld [-rdsxXStniO] [-o output] [-e entry] [-u symbol] [-D bytes] [-L directory] [-l library] object ...\n");return 2;
}
static int number(const char *p,uint64_t *value) {
    uint64_t n=0;if(!*p)return -1;
    for(;*p;p++) {unsigned d=(unsigned char)*p-'0';if(d>9 || n>(UINT64_MAX-d)/10)return -1;n=n*10+d;}
    *value=n;return 0;
}
int main(int argc,char **argv) {
    struct ld_state s;ld_init(&s);int result=0,reloc=0,common=0,trace=0,options=1;
    const char *output="a.out",*entry="_start";
    const char **dirs=calloc((size_t)argc+2,sizeof(*dirs));struct action *actions=calloc((size_t)argc,sizeof(*actions));
    size_t ndirs=0,na=0;
    if(!dirs || !actions) {result=1;goto done;}
    for(int i=1;i<argc;i++) {
        const char *arg=argv[i];if(options&&!strcmp(arg,"--")) {options=0;continue;}
        if(!options || arg[0]!='-' || !arg[1]) {actions[na++]=(struct action){arg,0};continue;}
        for(size_t j=1;arg[j];j++) {
            char flag=arg[j];const char *value=NULL;
            if(strchr("oeuDLl",flag)) {
                if(arg[j+1]) {value=arg+j+1;j=strlen(arg)-1;}
                else if(i+1<argc)value=argv[++i];
                else {result=usage();goto done;}
            }
            switch(flag) {
            case 'o':output=value;break;
            case 'e':entry=value;actions[na++]=(struct action){value,2};break;
            case 'u':actions[na++]=(struct action){value,2};break;
            case 'L':dirs[ndirs++]=value;break;
            case 'l':actions[na++]=(struct action){value,1};break;
            case 'D':if(number(value,&s.minimum_data)) {result=usage();goto done;}break;
            case 'r':reloc=1;break;
            case 'd':common=1;break;
            case 's':s.strip|=13;break;
            case 'x':s.strip|=1;break;
            case 'X':s.strip|=2;break;
            case 't':trace=1;break;
            /* Native ELF always has separate RX/RW pages, flat addressing and
             * symbol/debug formats; PDP-11 magic/segmentation choices collapse. */
            case 'S':s.strip|=8;break;
            case 'n':case 'i':case 'O':break;
            default:result=usage();goto done;
            }
            if(value)break;
        }
    }
    if(!na) {result=usage();goto done;}
    dirs[ndirs++]="/lib";dirs[ndirs++]="/usr/lib";
    for(size_t i=0;i<na;i++) {
        const char *path=actions[i].name;char library[PATH_LIMIT];
        if(actions[i].kind==2) {if(ld_require(&s,path))break;continue;}
        if(actions[i].kind==1) {
            int found=0;
            for(size_t j=0;j<ndirs;j++) {
                int n=snprintf(library,sizeof(library),"%s/lib%s.a",dirs[j],path);
                if(n<0 || n>=(int)sizeof(library))continue;
                if(!access(library,R_OK)) {found=1;break;}
            }
            if(!found) {fprintf(stderr,"ld: cannot find library: %s\n",path);result=1;goto done;}path=library;
        }
        if(trace)printf("%s\n",path);
        if(ld_add_file(&s,path))break;
    }
    unsigned char *bytes=NULL;size_t length=0;
    if(s.failed || (reloc?ld_relocatable(&s,common,&bytes,&length):ld_executable(&s,entry,&bytes,&length))) {
        fprintf(stderr,"ld: %s\n",s.error);result=1;
    } else if(archive_replace_mode(output,bytes,length,reloc?0666:0777)) {
        fprintf(stderr,"ld: cannot write %s\n",output);result=1;
    }
    free(bytes);
done:
    free(dirs);free(actions);ld_free(&s);return result;
}
