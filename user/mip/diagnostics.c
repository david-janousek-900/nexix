/* Real native varargs replace common's PDP-11 unprototyped stack arguments. */
#include "diagnostics.h"
#include <stdarg.h>
void mip_diagnostics_init(struct mip_diagnostics *d,FILE *stream,const char *file,unsigned limit) {
    *d=(struct mip_diagnostics){.stream=stream,.filename=file?file:"<input>",.limit=limit?limit:30};
}
int mip_diagnostic(struct mip_diagnostics *d,unsigned line,unsigned warning,const char *format,...) {
    if(!d || !d->stream || !format || warning>2 || d->fatal)return -1;
    if(fprintf(d->stream,"%s:%u: %s",d->filename,line,warning==1?"warning: ":warning==2?"compiler error: ":"error: ")<0) {d->fatal=1;return -1;}
    va_list ap;va_start(ap,format);int result=vfprintf(d->stream,format,ap);va_end(ap);
    if(result<0 || fputc('\n',d->stream)==EOF) {d->fatal=1;return -1;}
    if(warning==1)d->warnings++;else d->errors++;
    if(warning==2 || d->errors>=d->limit) {d->fatal=1;return -1;}
    return 0;
}
