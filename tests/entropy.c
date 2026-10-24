#include "entropy.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
static unsigned calls;
static int fail;
bool entropy_arch_word(uint64_t *out)
{++calls;if(fail)return false;*out=UINT64_C(0x0807060504030201);return true;}
int main(void)
{
    unsigned char b[258]={0};
    assert(entropy_fill(b+1,256)==0&&calls==32&&b[0]==0&&b[257]==0);
    for(unsigned i=0;i<256;++i)assert(b[i+1]==i%8+1);
    assert(entropy_fill(b,257)==-NX_EIO&&calls==32);
    assert(entropy_fill(NULL,0)==0&&calls==32);
    fail=1;assert(entropy_fill(b,1)==-NX_EIO&&calls==33);
    fail=0;assert(entropy_fill(b+1,3)==0&&b[1]==1&&b[3]==3);
    puts("PASS: bounded entropy, complete words, tail bytes and source failure");return 0;
}
