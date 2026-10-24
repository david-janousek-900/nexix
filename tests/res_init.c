#include <resolv.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
int errno;
struct __res_state _res;
static const char *config,*localdomain,*hostname="host.a.b.example.test";
static FILE stream;
static int live;
FILE *fopen(const char *path,const char *mode)
{if(strcmp(path,"/test/resolv.conf")||strcmp(mode,"r")||!config){errno=ENOENT;return NULL;}stream=(FILE){.used=1,.buffer=(unsigned char *)config};++live;return &stream;}
int fclose(FILE *f){f->used=0;--live;return 0;}
void rewind(FILE *f){f->position=0;}
int ferror(FILE *f){return f->error;}
int getc(FILE *f){int c=f->buffer[f->position];if(c)++f->position;return c?c:EOF;}
char *getenv(const char *name){return !strcmp(name,"LOCALDOMAIN")?(char *)localdomain:NULL;}
int gethostname(char *out,size_t n){if(strlen(hostname)+1>n)return -1;strcpy(out,hostname);return 0;}
int main(void)
{
    CHECK(res_init()==0&&!live&&(_res.options&RES_INIT)&&_res.retrans==5&&_res.retry==4);
    CHECK(_res.nscount==1&&ntohl(_res.nsaddr.sin_addr.s_addr)==0x7f000001&&ntohs(_res.nsaddr.sin_port)==53);
    CHECK(!strcmp(_res.defdname,"a.b.example.test")&&!strcmp(_res.dnsrch[1],"b.example.test")&&!strcmp(_res.dnsrch[2],"example.test")&&!_res.dnsrch[3]);
    config="nameserver invalid\nnameserver 1.2.3.4 # comment\nnameserver 127.0.0.2 ; comment\nnameserver 10.1.2.3\nnameserver 10.1.2.4\nsearch a.test b.test c.test d.test e.test f.test ignored.test\n";
    _res.retrans=1;_res.retry=2;CHECK(res_init()==0&&!live&&_res.retrans==1&&_res.retry==2);
    CHECK(_res.nscount==3&&ntohl(_res.nsaddr.sin_addr.s_addr)==0x01020304);
    CHECK(!strcmp(_res.defdname,"a.test")&&!strcmp(_res.dnsrch[5],"f.test")&&!_res.dnsrch[6]);
    localdomain="env.a.test ignored.test";CHECK(!res_init()&&!strcmp(_res.defdname,"env.a.test")&&!strcmp(_res.dnsrch[1],"a.test")&&!_res.dnsrch[2]);
    localdomain=NULL;config="search ignored.test\ndomain chosen.test\n";CHECK(!res_init()&&!strcmp(_res.defdname,"chosen.test")&&!_res.dnsrch[1]);
    char big[1800];memset(big,'x',1600);strcpy(big+1600,"\ndomain good.test\n");config=big;
    CHECK(!res_init()&&!strcmp(_res.defdname,"good.test")&&!live);
    config="nameserver 0.0.0.0\nnameserver 224.1.2.3\nnameserver 255.255.255.255\n";hostname="single";
    CHECK(!res_init()&&!_res.defdname[0]&&_res.dnsrch[0]&&!_res.dnsrch[0][0]&&ntohl(_res.nsaddr.sin_addr.s_addr)==0x7f000001);
    puts("PASS: resolver defaults, config path override, servers, search, LOCALDOMAIN and hostname domain");return 0;
}
