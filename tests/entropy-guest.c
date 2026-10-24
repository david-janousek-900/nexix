#include <sys/random.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <resolv.h>
#define CHECK(c) do {if(!(c)){printf("entropy check failed: %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    (void)argv;
    unsigned char b[256],other[256];
    if(argc>1){memset(b,0xa5,sizeof(b));CHECK(getentropy(b,sizeof(b))==-1&&errno==EIO);for(unsigned i=0;i<sizeof(b);++i)CHECK(b[i]==0xa5);puts("ENTROPY_UNAVAILABLE_OK");return 0;}
    CHECK(getentropy(NULL,0)==0);
    CHECK(getentropy(b,257)==-1&&errno==EIO);
    CHECK(getentropy((void *)1,8)==-1&&errno==EFAULT);
    CHECK(getentropy((void *)(uintptr_t)0x100000,8)==-1&&errno==EFAULT);
    CHECK(getentropy(b,sizeof(b))==0&&getentropy(other,sizeof(other))==0&&memcmp(b,other,sizeof(b)));
    unsigned bins[16]={0};
    for(unsigned i=0;i<100;++i){CHECK(getentropy(b,sizeof(b))==0);for(unsigned j=0;j<256;++j)++bins[b[j]&15];}
    for(unsigned i=0;i<16;++i)CHECK(bins[i]>1200&&bins[i]<2000);
    unsigned char query[512];unsigned previous=0,changes=0;
    for(unsigned i=0;i<512;++i){CHECK(res_mkquery(QUERY,"example.test",C_IN,T_A,NULL,0,NULL,query,sizeof(query))>0);unsigned id=_getshort(query);if(i&&id!=((previous+1)&65535))++changes;previous=id;}
    CHECK(changes>490);
    int p[2];CHECK(pipe(p)==0);pid_t child=fork();CHECK(child>=0);
    if(!child){close(p[0]);if(getentropy(b,sizeof(b))||write(p[1],b,sizeof(b))!=(int)sizeof(b))_exit(1);_exit(0);}
    close(p[1]);CHECK(getentropy(b,sizeof(b))==0);unsigned n=0;while(n<sizeof(other)){ssize_t got=read(p[0],other+n,sizeof(other)-n);CHECK(got>0);n+=(unsigned)got;}
    CHECK(memcmp(b,other,sizeof(b)));int status;CHECK(wait(&status)==child&&status==0);close(p[0]);
    puts("ENTROPY_GUEST_OK");return 0;
}
