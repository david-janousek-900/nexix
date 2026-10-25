#include <netdb.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include "../libc/netdb_private.h"
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
int errno;
static unsigned char response[4096];static int length,error,queries;
static FILE file;
FILE *fopen(const char *path,const char *mode){(void)mode;if(strcmp(path,_PATH_HOSTS))return NULL;file=(FILE){.buffer=(unsigned char *)"127.0.0.1 localhost alias\n"};return &file;}
int fclose(FILE *f){(void)f;return 0;}
void rewind(FILE *f){f->position=0;}
int ferror(FILE *f){(void)f;return 0;}
int getc(FILE *f){int c=f->buffer[f->position];if(c)++f->position;return c?c:EOF;}
void _res_close(void){}
int getentropy(void *p,size_t n){memset(p,42,n);return 0;}
int fprintf(FILE *f,const char *s,...){(void)f;(void)s;return 0;}
int fputs(const char *s,FILE *f){(void)s;(void)f;return 0;}
int fputc(int c,FILE *f){(void)f;return c;}
static int query(unsigned char *out,int capacity)
{++queries;if(error){errno=error;h_errno=TRY_AGAIN;return -1;}if(length>capacity){errno=EMSGSIZE;return -1;}memcpy(out,response,(size_t)length);return length;}
int res_search(const char *name,int class,int type,unsigned char *out,int capacity)
{(void)name;(void)class;(void)type;return query(out,capacity);}
int res_query(const char *name,int class,int type,unsigned char *out,int capacity)
{(void)name;(void)class;(void)type;return query(out,capacity);}
static void start(const char *name,int type)
{length=res_mkquery(QUERY,name,C_IN,type,NULL,0,NULL,response,sizeof(response));response[2]=0x81;response[3]=0x80;error=queries=0;}
static void record(const char *owner,int type,const char *target,unsigned address)
{
    length+=dn_comp(owner,response+length,(int)sizeof(response)-length,NULL,NULL);
    putshort((uint16_t)type,response+length);putshort(C_IN,response+length+2);putlong(60,response+length+4);length+=8;
    int size;
    if(type==T_A){size=4;putlong(address,response+length+2);}
    else size=dn_comp(target,response+length+2,(int)sizeof(response)-length-2,NULL,NULL);
    putshort((uint16_t)size,response+length);length+=2+size;
    putshort((uint16_t)(_getshort(response+6)+1),response+6);
}
int main(void)
{
    struct hostent *h=gethostbyname("127.1");CHECK(h&&queries==0&&!strcmp(h->h_name,"127.1")&&*(uint32_t *)h->h_addr==htonl(0x7f000001));
    CHECK(!gethostbyname("999.1")&&queries==0&&h_errno==HOST_NOT_FOUND);
    CHECK(!gethostbyname(NULL)&&errno==EINVAL);
    start("alias.test",T_A);record("real.test",T_A,NULL,0xc0000201);record("middle.test",T_CNAME,"real.test",0);record("alias.test",T_CNAME,"middle.test",0);
    h=gethostbyname("alias.test");CHECK(h&&!strcmp(h->h_name,"real.test")&&!strcmp(h->h_aliases[0],"alias.test")&&!strcmp(h->h_aliases[1],"middle.test")&&!h->h_aliases[2]);
    CHECK(((uintptr_t)h->h_addr%_Alignof(struct in_addr))==0&&h->h_length==4&&h->h_addrtype==AF_INET);
    CHECK(!gethostbyname("123.")==0&&queries==2);
    start("many.test",T_A);for(unsigned i=0;i<40;++i)record("many.test",T_A,NULL,0xc0000200+i);
    h=gethostbyname("many.test");CHECK(h&&h->h_addr_list[34]&&!h->h_addr_list[35]);
    start("1.2.0.192.in-addr.arpa",T_PTR);record("1.2.0.192.in-addr.arpa",T_PTR,"server.test",0);
    unsigned char addr[5]={99,192,0,2,1};h=gethostbyaddr(addr+1,4,AF_INET);CHECK(h&&!strcmp(h->h_name,"server.test")&&!memcmp(h->h_addr,addr+1,4));
    CHECK(!gethostbyaddr(addr,3,AF_INET)&&errno==EINVAL);CHECK(!gethostbyaddr(addr,4,AF_UNSPEC)&&errno==EAFNOSUPPORT);
    error=ECONNREFUSED;CHECK(gethostbyname("ALIAS")&&!strcmp(gethostbyname("localhost")->h_name,"localhost"));
    unsigned char loop[]={127,0,0,1};CHECK(gethostbyaddr(loop,4,AF_INET));
    error=ETIMEDOUT;CHECK(!gethostbyname("localhost")&&h_errno==TRY_AGAIN);error=EIO;CHECK(!gethostbyname("localhost"));
    start("cycle.test",T_A);record("cycle.test",T_CNAME,"other.test",0);record("other.test",T_CNAME,"cycle.test",0);
    CHECK(!gethostbyname("cycle.test")&&h_errno==NO_RECOVERY);
    start("clean.test",T_A);record("clean.test",T_A,NULL,0xc0000201);
    unsigned char valid[4096];int valid_length=length;memcpy(valid,response,(size_t)length);
    for(int n=0;n<valid_length;++n)CHECK(!_nx_host_answer(valid,n,0));
    uint32_t seed=0x4e584841;
    for(unsigned i=0;i<40000;++i){
        memcpy(response,valid,(size_t)valid_length);seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
        unsigned pos=seed%(unsigned)valid_length;response[pos]^=(unsigned char)(seed>>16);
        (void)_nx_host_answer(response,valid_length,0);
    }
    _res.options=RES_DEFAULT;sethostent(1);CHECK((_res.options&(RES_USEVC|RES_STAYOPEN))==(RES_USEVC|RES_STAYOPEN));endhostent();CHECK(!(_res.options&(RES_USEVC|RES_STAYOPEN)));
    puts("PASS: host answers, CNAME order/loops, PTR, address limits/alignment, numeric lookup, fallback and 40000 mutations");return 0;
}
