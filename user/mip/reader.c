/* Shared one/two-pass transport, independent of the C and Fortran parsers. */
#include "pass.h"
#include <stdlib.h>
#include <string.h>
enum {END=0,TEXT=1,EXPRESSION=2,INITIALIZER=3};
static const unsigned char signature[16]={'N','X','M','P','S',0,2,0,1,0,0,0,0,0,0,0};
static void put32(unsigned char *p,uint32_t v) {for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static uint32_t get32(const unsigned char *p) {uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static int bits_valid(unsigned bits) {return bits==8 || bits==16 || bits==32 || bits==64 || bits==128;}
static int has_zero(const unsigned char *p,size_t n) {for(size_t i=0;i<n;i++)if(!p[i])return 1;return 0;}
static int write_bytes(struct mip_pass_writer *w,const void *p,size_t n) {
    if(!w || !w->file || w->failed || w->ended)return -1;
    if(w->bytes>MIP_PASS_LIMIT || n>MIP_PASS_LIMIT-w->bytes || (n && (!p || fwrite(p,1,n,w->file)!=n))) {w->failed=1;return -1;}
    w->bytes+=n;return 0;
}
static int record(struct mip_pass_writer *w,unsigned kind,unsigned bits,const void *p,size_t n) {
    if(!w || !w->file || w->failed || w->ended)return -1;
    if(n>UINT32_MAX || w->bytes>MIP_PASS_LIMIT-12 || n>MIP_PASS_LIMIT-w->bytes-12) {w->failed=1;return -1;}
    unsigned char header[12];put32(header,kind);put32(header+4,bits);put32(header+8,(uint32_t)n);
    if(write_bytes(w,header,sizeof(header)) || write_bytes(w,p,n))return -1;
    return 0;
}
int mip_pass_begin(struct mip_pass_writer *w,FILE *file) {
    if(!w || !file)return -1;
    *w=(struct mip_pass_writer){.file=file};return write_bytes(w,signature,sizeof(signature));
}
int mip_pass_text(struct mip_pass_writer *w,const void *text,size_t n) {
    if(!w)return -1;
    if(n>MIP_PASS_TEXT_LIMIT || (n && (!text || has_zero(text,n)))) {w->failed=1;return -1;}
    return record(w,TEXT,0,text,n);
}
int mip_pass_tree(struct mip_pass_writer *w,const struct mip_arena *a,uint32_t root,unsigned bits) {
    if(!w || !w->file || w->failed || w->ended)return -1;
    unsigned char *wire=NULL;size_t length=0;
    if((bits && !bits_valid(bits)) || mip_encode(a,root,&wire,&length)) {w->failed=1;return -1;}
    int status=record(w,bits?INITIALIZER:EXPRESSION,bits,wire,length);free(wire);return status;
}
int mip_pass_end(struct mip_pass_writer *w) {
    if(record(w,END,0,NULL,0))return -1;
    w->ended=1;if(fflush(w->file)) {w->failed=1;return -1;}return 0;
}
int mip_pass_read(FILE *file,const struct mip_pass_consumer *consumer,void *context) {
    unsigned char header[16];size_t total=sizeof(signature);
    if(!file || !consumer || !consumer->text || !consumer->tree ||
       fread(header,1,sizeof(header),file)!=sizeof(header) || memcmp(header,signature,sizeof(header)))return -1;
    for(;;) {
        if(total>MIP_PASS_LIMIT-12 || fread(header,1,12,file)!=12)return -1;
        total+=12;
        unsigned kind=get32(header),bits=get32(header+4);size_t n=get32(header+8);
        if(kind==END) return !bits && !n && fgetc(file)==EOF && !ferror(file)?0:-1;
        if(kind>INITIALIZER || (kind==INITIALIZER?!bits_valid(bits):bits!=0) ||
           n>MIP_PASS_LIMIT-total || n>(kind==TEXT?MIP_PASS_TEXT_LIMIT:MIP_WIRE_LIMIT))return -1;
        unsigned char *data=malloc(n?n:1);if(!data)return -1;
        int status=-1;
        if(fread(data,1,n,file)!=n) {free(data);return -1;}
        total+=n;
        if(kind==TEXT) {
            if(!has_zero(data,n))status=consumer->text(context,data,n);
        } else if(n>=16) {
            uint32_t count=get32(data+8);
            if(count && count<=MIP_NODE_LIMIT && count<=(n-16)/MIP_WIRE_NODE_SIZE) {
                struct mip_node *nodes=calloc(count,sizeof(*nodes));
                if(nodes) {
                    struct mip_arena arena={nodes,0,count};uint32_t root;
                    if(!mip_decode(data,n,&arena,&root))status=consumer->tree(context,&arena,root,bits);
                    free(nodes);
                }
            }
        }
        free(data);if(status)return -1;
    }
}
