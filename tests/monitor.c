#include <mon.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
int errno;
extern void __nx_mcount(uintptr_t);
extern int __nx_profile_start(void *,void *);
static _Alignas(16) unsigned short buffer[128],allocation[8192];
static unsigned char output[256];
static size_t outsize,profile_size,profile_offset;
static unsigned profile_scale;
static unsigned short *profile_buffer;
static int creates,closes,write_mode,profile_error,allocation_error,atexit_error,freed,flushed,reenter;
static int writes,close_error;
static void (*callback)(void);
static uint64_t get(const void *p,unsigned n)
{const unsigned char *b=p;uint64_t v=0;for(unsigned i=0;i<n;++i)v|=(uint64_t)b[i]<<(8*i);return v;}
int profil(unsigned short *buf,size_t size,uintptr_t off,unsigned scale)
{
    if(profile_error){errno=ENOSYS;return -1;}
    profile_buffer=buf;profile_size=size;profile_offset=off;profile_scale=scale;return 0;
}
int creat(const char *path,mode_t mode)
{if(strcmp(path,"mon.out")||mode!=0666)__builtin_trap();++creates;outsize=0;return write_mode==4?(errno=EACCES,-1):7;}
ssize_t write(int fd,const void *data,size_t size)
{
    if(fd!=7)__builtin_trap();
    ++writes;
    if(reenter){reenter=0;__nx_mcount(0x123456780008UL);if(monitor(NULL)!=-1||errno!=EBUSY)__builtin_trap();}
    if(write_mode==1&&writes==1){errno=EINTR;return -1;}
    if(write_mode==2){errno=ENOSPC;return -1;}
    if(write_mode==3)return 0;
    if(size>7)size=7;
    if(outsize+size>sizeof(output))size=sizeof(output)-outsize;
    if(!size){errno=ENOSPC;return -1;}
    memcpy(output+outsize,data,size);outsize+=size;return size;
}
int close(int fd)
{if(fd!=7)__builtin_trap();++closes;if(close_error){errno=EIO;return -1;}return 0;}
int fflush(FILE *stream){if(stream)__builtin_trap();++flushed;return 0;}
void *calloc(size_t count,size_t size)
{if(allocation_error||count!=1||size>sizeof(allocation))return NULL;memset(allocation,0,sizeof(allocation));return allocation;}
void free(void *pointer){if(pointer!=allocation)__builtin_trap();++freed;}
int atexit(void (*fn)(void)){if(atexit_error)return -1;callback=fn;return 0;}
int main(void)
{
    const uintptr_t low=0x123456780000UL,high=low+128;
    CHECK(monitor(NULL)==0&&creates==0); /* Stop before start. */
    memset(buffer,0,sizeof(buffer));buffer[48]=19; /* Histogram begins after 2 call slots. */
    CHECK(monitor((void *)low,(void *)high,buffer,(size_t)128,(size_t)2)==0);
    CHECK(!memcmp(buffer,NX_MON_MAGIC,8));
    const unsigned char *b=(unsigned char *)buffer;
    CHECK(get(b+8,4)==1&&get(b+12,2)==62&&get(b+14,2)==64);
    CHECK(get(b+16,8)==low&&get(b+24,8)==high&&get(b+32,4)==100);
    CHECK(get(b+36,4)==65534&&get(b+40,4)==2&&get(b+44,4)==0&&get(b+48,8)==160);
    CHECK(get(b+56,4)==16&&get(b+60,2)==2&&get(b+62,2)==0);
    CHECK(profile_buffer==buffer+48&&profile_size==160&&profile_offset==low&&profile_scale==65534&&buffer[48]==19);
    errno=17;__nx_mcount(low+8);__nx_mcount(low+8);__nx_mcount(high);__nx_mcount(low-1);
    CHECK(get(b+64,8)==low+8&&get(b+72,8)==2&&get(b+44,4)==1&&errno==17);
    countbase=NULL;__nx_mcount(low+16);CHECK(get(b+44,4)==1);__nx_mcount(low+8);CHECK(get(b+72,8)==3);
    countbase=(void *)(b+80);__nx_mcount(low+16);__nx_mcount(low+24);
    CHECK(!countbase&&get(b+44,4)==2&&get(b+80,8)==low+16&&get(b+88,8)==1&&buffer[48]==19);
    memset((unsigned char *)buffer+72,0xff,8);__nx_mcount(low+8);CHECK(get(b+72,8)==0); /* 64-bit wrap. */
    CHECK(monitor((void *)high,(void *)low,buffer,(size_t)128,(size_t)2)==-1&&errno==EINVAL&&profile_scale==65534);
    CHECK(monitor((void *)low,(void *)high,buffer,(size_t)31,(size_t)0)==-1&&errno==EINVAL);
    CHECK(monitor((void *)low,(void *)high,buffer,SIZE_MAX,(size_t)0)==-1&&errno==EINVAL);
    CHECK(monitor((void *)low,(void *)high,buffer,(size_t)128,(size_t)UINT32_MAX+1)==-1&&errno==EINVAL);
    reenter=1;write_mode=1;writes=0;CHECK(monitor(NULL)==0&&outsize==256&&closes==1&&!profile_scale&&!countbase);
    CHECK(!memcmp(output,buffer,256)&&get(b+72,8)==0);
    __nx_mcount(low+8);CHECK(get(b+72,8)==0); /* Off while output/after stop. */
    write_mode=2;writes=0;close_error=1;CHECK(monitor(NULL)==-1&&errno==ENOSPC&&closes==2);
    write_mode=3;close_error=0;CHECK(monitor(NULL)==-1&&errno==EIO&&closes==3);
    write_mode=4;CHECK(monitor(NULL)==-1&&errno==EACCES&&closes==3);
    write_mode=0;close_error=1;CHECK(monitor(NULL)==-1&&errno==EIO&&closes==4);close_error=0;
    CHECK(monitor(NULL)==0&&outsize==256); /* Retry rewrites stable contents. */
    CHECK(monitor((void *)low,(void *)(low+1024),buffer,(size_t)40,(size_t)0)==0);
    CHECK(profile_size==16&&profile_scale==1024); /* 8 / 512 * 65536. */
    __nx_mcount(low+8);CHECK(get(b+44,4)==0&&!countbase);
    CHECK(monitor((void *)low,(void *)(low+3),buffer,(size_t)40,(size_t)0)==0&&profile_scale==65534);
    CHECK(monitor((void *)low,(void *)(UINTPTR_MAX-1),buffer,(size_t)40,(size_t)0)==0&&profile_scale==0);
    CHECK(monitor((void *)low,(void *)high,buffer,(size_t)40,(size_t)1)==0&&profile_size==0&&profile_scale==0);
    __nx_mcount(low+8);CHECK(get(b+44,4)==1); /* Counts-only session. */
    CHECK(monitor((void *)low,(void *)high,buffer+1,(size_t)127,(size_t)1)==0);
    __nx_mcount(low+8);CHECK(get(b+66,8)==low+8&&get(b+74,8)==1); /* Only two-byte alignment. */
    profile_error=1;CHECK(monitor(NULL)==-1&&errno==ENOSYS);profile_error=0;
    CHECK(monitor(NULL)==0);
    allocation_error=1;CHECK(__nx_profile_start((void *)low,(void *)high)==-1&&errno==ENOMEM);allocation_error=0;
    atexit_error=1;CHECK(__nx_profile_start((void *)low,(void *)high)==-1&&errno==ENOMEM&&freed==1&&!profile_scale&&!countbase);atexit_error=0;
    CHECK(__nx_profile_start((void *)low,(void *)high)==0&&callback&&countbase);
    write_mode=4;callback();CHECK(flushed==1&&!profile_scale&&!countbase); /* Exit ignores output error. */
    return 0;
}
