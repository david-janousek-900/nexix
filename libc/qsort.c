#include <stdlib.h>
#include <stdint.h>
void qsort(void *base,size_t count,size_t size,int (*cmp)(const void *,const void *))
{
    unsigned char *bytes=base;
    if(!size||count>SIZE_MAX/size)return;
    for(size_t i=1;i<count;++i)for(size_t j=i;j&&cmp(bytes+(j-1)*size,bytes+j*size)>0;--j)
        for(size_t k=0;k<size;++k){unsigned char c=bytes[(j-1)*size+k];bytes[(j-1)*size+k]=bytes[j*size+k];bytes[j*size+k]=c;}
}
