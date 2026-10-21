/* Private native acceptance consumer for the assembler first-pass API. */
#include "as.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    struct as_program p;int result=as_parse(argv[1],&p);
    if(result){as_free(&p);return 1;}
    unsigned instructions=0,labels=0,data=0,memory=0;
    int rip=0,indexed=0,local=0,strings=0,size=0,constant=0,float32=0,float64=0,alignment=0,symbols=0;
    for(size_t i=0;i<p.count;i++) {
        struct as_record *r=p.records+i;
        if(r->kind==AS_LABEL){labels++;if(!strcmp(p.symbols[r->symbol].name,".N1_1"))local=1;}
        if(r->kind==AS_INSTRUCTION) {
            instructions++;
            for(unsigned j=0;j<r->count;j++)if(r->operands[j].kind==AS_MEMORY){memory++;
                if(!strcmp(r->operands[j].base,"rip")&&r->operands[j].expression.plus!=AS_NONE)rip=1;
                if(!strcmp(r->operands[j].base,"rbp")&&!strcmp(r->operands[j].index,"rcx")&&r->operands[j].scale==8&&r->operands[j].expression.value==(uint64_t)-16)indexed=1;
            }
        }
        if(r->kind==AS_BYTES&&r->length==7&&!memcmp(r->bytes,"hello\n\0",7))strings=1;
        if(r->kind==AS_SIZE&&r->expression.plus==AS_DOT&&r->expression.minus!=AS_NONE)size=1;
        if(r->kind==AS_ALIGN&&r->expression.value==16)alignment=1;
        if(r->kind==AS_DATA){data++;if(r->width==4&&r->expression.value==0x3fc00000)float32=1;if(r->width==8&&r->expression.value==0x3ff8000000000000UL)float64=1;if(r->width==4&&r->expression.value==14&&r->expression.plus==AS_NONE)constant=1;}
    }
    unsigned contract=0;
    for(size_t i=0;i<p.symbol_count;i++) {
        struct as_symbol *s=p.symbols+i;
        if(!strcmp(s->name,"entry")&&s->defined==1&&s->binding==1&&s->type==2)contract|=1;
        if(!strcmp(s->name,"external")&&!s->defined&&s->binding==2)contract|=2;
        if(!strcmp(s->name,"common")&&s->common&&s->size==8&&s->alignment==8)contract|=4;
        if(!strcmp(s->name,"private")&&s->common&&!s->binding&&s->size==4)contract|=8;
    }
    symbols=contract==15;
    printf("instructions=%u labels=%u data=%u memory=%u\n",instructions,labels,data,memory);
    result=!(rip&&indexed&&local&&strings&&size&&constant&&float32&&float64&&alignment&&symbols);
    as_free(&p);return result;
}
