#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
struct block { size_t size; struct block *next; };
static struct block *available;
void *malloc(size_t n)
{
    if(n>SIZE_MAX-15-sizeof(struct block)){errno=ENOMEM;return NULL;}
    n=(n+15)&~(size_t)15;if(!n)n=16;
    struct block **link=&available;
    while(*link){struct block *b=*link;if(b->size>=n){*link=b->next;return b+1;}link=&b->next;}
    size_t total=n+sizeof(struct block);
    if(total>INTPTR_MAX){errno=ENOMEM;return NULL;}
    struct block *b=sbrk(total);if(b==(void *)-1)return NULL;b->size=n;b->next=NULL;return b+1;
}
void free(void *p){if(p){struct block *b=(struct block *)p-1;b->next=available;available=b;}}
void *calloc(size_t n,size_t size){if(size&&n>SIZE_MAX/size){errno=ENOMEM;return NULL;}size_t total=n*size;void *p=malloc(total);if(p)memset(p,0,total);return p;}
void *realloc(void *p,size_t n){if(!p)return malloc(n);if(!n){free(p);return NULL;}struct block *b=(struct block *)p-1;if(n<=b->size)return p;void *q=malloc(n);if(q){memcpy(q,p,b->size);free(p);}return q;}
