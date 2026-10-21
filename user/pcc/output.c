/* Explicit output context replaces the supplied convert/fconvert printf edits. */
#include "backend.h"
#include <stdarg.h>
#undef printf
#undef puts
#undef putchar
static struct mip_pass_writer pass;
static unsigned split;
int pcc_output_begin(void) {split=1;return mip_pass_begin(&pass,stdout);}
static int text(const char *p,size_t n) {
    if(!split)return fwrite(p,1,n,stdout)==n?0:-1;
    while(n) {
        size_t chunk=n>MIP_PASS_TEXT_LIMIT?MIP_PASS_TEXT_LIMIT:n;
        if(mip_pass_text(&pass,p,chunk))return -1;
        p+=chunk;n-=chunk;
    }
    return 0;
}
int pcc_target_printf(const char *format,...) {
    va_list ap;va_start(ap,format);
    if(!split) {int n=vfprintf(stdout,format,ap);va_end(ap);return n;}
    char small[256];va_list copy;va_copy(copy,ap);
    int n=vsnprintf(small,sizeof(small),format,copy);va_end(copy);
    if(n<0) {va_end(ap);cerror("target text formatting failed");}
    char *p=small;
    if((size_t)n>=sizeof(small)) {
        p=malloc((size_t)n+1);if(!p)cerror("no memory for target text");
        if(vsnprintf(p,(size_t)n+1,format,ap)!=n)cerror("target text formatting changed");
    }
    va_end(ap);int status=text(p,(size_t)n);if(p!=small)free(p);
    if(status)cerror("compiler pass output failed");
    return n;
}
int pcc_target_puts(const char *p) {
    if(text(p,strlen(p)) || text("\n",1))cerror("target text output failed");
    return 0;
}
int pcc_target_putchar(int c) {
    char byte=(char)c;if(text(&byte,1))cerror("target character output failed");return (unsigned char)c;
}
int pcc_output_end(void) {return split?mip_pass_end(&pass):fflush(stdout);}
int pcc_output_tree(const struct mip_arena *arena,uint32_t root,unsigned bits) {
    if(split)return mip_pass_tree(&pass,arena,root,bits);
    if(bits)pcc_emit_initializer(arena,root,bits);else pcc_emit_expression(arena,root);
    return ferror(stdout)?-1:0;
}
static int consume_text(void *p,const unsigned char *s,size_t n) {(void)p;return fwrite(s,1,n,stdout)!=n;}
static int consume_tree(void *p,const struct mip_arena *a,uint32_t root,unsigned bits) {
    (void)p;return pcc_output_tree(a,root,bits);
}
int pcc_read_pass(FILE *file) {
    static const struct mip_pass_consumer consumer={consume_text,consume_tree};
    return mip_pass_read(file,&consumer,NULL) || fflush(stdout);
}
