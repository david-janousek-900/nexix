/* Private native resolution consumer, shared by host and QEMU tests. */
#include "ld.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
    struct ld_state s;ld_init(&s);int r=0;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"-u") && i+1<argc)r=ld_require(&s,argv[++i]);
        else r=ld_add_file(&s,argv[i]);
        if(r)break;
    }
    if(!r)r=ld_check(&s);
    if(r)fprintf(stderr,"ld: %s\n",s.error);
    else {
        for(size_t i=0;i<s.input_count;i++)if(s.inputs[i].selected) {
            const char *p=strrchr(s.inputs[i].path,'/');p=p?p+1:s.inputs[i].path;
            printf("input %s%s%s%s\n",p,s.inputs[i].member?"(":"",s.inputs[i].member?s.inputs[i].member:"",s.inputs[i].member?")":"");
        }
        for(size_t i=0;i<s.symbol_count;i++) {
            struct ld_symbol *g=s.symbols+i;
            printf("symbol %s %s %s %lu %lu\n",g->name,g->defined?(g->common?"common":"defined"):"weak-undefined",g->bind==STB_WEAK?"weak":"global",(unsigned long)g->size,g->common?(unsigned long)g->alignment:0UL);
        }
    }
    ld_free(&s);return r?1:0;
}
