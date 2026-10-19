#ifndef NEXIX_ABI_HOSTNAME_H
#define NEXIX_ABI_HOSTNAME_H
#include <stddef.h>
#include <nexix/utsname.h>
#define NX_HOST_NAME_MAX (NX_UTS_FIELD_SIZE-1)
/* Host labels are ASCII letters/digits with internal hyphens. Dots retain the
 * configured domain; no DNS lookup, whitespace or shell metacharacters. */
static inline int nx_hostname_valid(const char *name,size_t length)
{
    size_t label=0,i;
    if(!length || length>NX_HOST_NAME_MAX)return 0;
    for(i=0;i<length;i++) {
        unsigned char c=name[i];
        if(c=='.') {
            if(!label || name[i-1]=='-')return 0;
            label=0;continue;
        }
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))return 0;
        if((!label && c=='-') || ++label>63)return 0;
    }
    return label && name[length-1]!='-';
}
#endif
