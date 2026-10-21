#include "encode.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    struct as_program p;
    if(as_parse(argv[1],&p)){as_free(&p);return 1;}
    for(size_t i=0;i<p.count;i++)if(p.records[i].kind==AS_INSTRUCTION) {
        struct as_code code;
        if(as_encode(p.records+i,&code)){fprintf(stderr,"encode: rejected instruction on line %u\n",p.records[i].line);as_free(&p);return 1;}
        for(unsigned j=0;j<code.length;j++)printf("%02x",code.bytes[j]);
        putchar('\n');
        for(unsigned j=0;j<code.fixup_count;j++) {
            struct as_fixup *f=code.fixups+j;
            const char *plus=f->expression.plus==AS_NONE?"-":f->expression.plus==AS_DOT?".":p.symbols[f->expression.plus].name;
            const char *minus=f->expression.minus==AS_NONE?"-":f->expression.minus==AS_DOT?".":p.symbols[f->expression.minus].name;
            printf("R %u %u %ld %s %s\n",f->offset,f->type,(long)f->expression.value,plus,minus);
        }
    }
    as_free(&p);return fflush(stdout)||ferror(stdout)?1:0;
}
