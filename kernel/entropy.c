/* Bounded native entropy generation. No predictable fallback is permitted. */
#include "entropy.h"
#include <nexix/errno.h>
int entropy_fill(void *buffer,size_t length)
{
    if(length>256)return -NX_EIO;
    unsigned char *out=buffer;
    for(size_t pos=0;pos<length;) {
        uint64_t word;
        if(!entropy_arch_word(&word))return -NX_EIO;
        for(unsigned i=0;i<8 && pos<length;++i,++pos) {out[pos]=(unsigned char)word;word>>=8;}
    }
    return 0;
}
