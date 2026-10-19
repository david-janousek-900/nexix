#ifndef NEXIX_HOSTNAME_FILE_H
#define NEXIX_HOSTNAME_FILE_H
#include <stdio.h>
#include <errno.h>
#include <nexix/hostname.h>
/* One hostname, optional final LF/CRLF; no silently ignored extra lines. */
static int read_hostname_file(const char *path,char name[NX_UTS_FIELD_SIZE])
{
    unsigned char bytes[NX_UTS_FIELD_SIZE+2];
    FILE *file=fopen(path,"r");
    size_t n,i;
    if(!file)return -1;
    n=fread(bytes,1,sizeof bytes,file);
    if(ferror(file)){int error=errno;fclose(file);errno=error;return -1;}
    if(fclose(file)==EOF)return -1;
    if(n && bytes[n-1]=='\n') { n--;if(n && bytes[n-1]=='\r')n--; }
    if(!nx_hostname_valid((const char *)bytes,n)){errno=EINVAL;return -1;}
    for(i=0;i<n;i++)name[i]=bytes[i];
    name[n]=0;return 0;
}
#endif
