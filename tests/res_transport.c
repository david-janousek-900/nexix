#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/times.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
int errno;
static uint64_t now;
static int mode,received,sends,connects,resets;
static unsigned char query[1024],tcpframe[2048];
static int qsize;static size_t tcp_size,tcp_pos;
static struct sockaddr_in target;
enum {VALID,WRONG_ID,WRONG_SOURCE,WRONG_QUESTION,SILENT,REFUSAL,SERVFAIL_FIRST,TRUNCATED,TCP_RESET,OVERSIZED,NX,NODATA,SEARCH,INTERRUPTED,MIXED};
int res_init(void){_res.options|=RES_INIT;return 0;}
int getentropy(void *out,size_t length){static unsigned id;unsigned char *p=out;for(size_t i=0;i<length;++i)p[i]=(unsigned char)++id;return 0;}
clock_t times(struct tms *t){memset(t,0,sizeof(*t));return (clock_t)now;}
int socket(int family,int type,int protocol){(void)family;(void)protocol;return type==SOCK_STREAM?4:3;}
int close(int fd){(void)fd;return 0;}
int setsockopt(int fd,int level,int option,const void *value,socklen_t length)
{(void)fd;(void)level;(void)option;(void)value;(void)length;return 0;}
int connect(int fd,const struct sockaddr *address,socklen_t length)
{(void)fd;(void)length;memcpy(&target,address,sizeof(target));++connects;return 0;}
static int response(unsigned char *out)
{
    memcpy(out,query,(size_t)qsize);out[2]=0x81;out[3]=0x80;putshort(1,out+6);
    int n=qsize;out[n++]=0xc0;out[n++]=12;putshort(T_A,out+n);putshort(C_IN,out+n+2);putlong(60,out+n+4);putshort(4,out+n+8);n+=10;
    out[n++]=192;out[n++]=0;out[n++]=2;out[n++]=1;
    if(mode==NX){out[3]=0x83;putshort(0,out+6);}
    if(mode==NODATA)putshort(0,out+6);
    if(mode==SERVFAIL_FIRST&&ntohs(target.sin_port)==53)out[3]=0x82;
    if(mode==SEARCH){char name[256];dn_expand(query,query+qsize,query+12,name,sizeof(name));if(!strncmp(name,"host.first",10)){out[3]=0x83;putshort(0,out+6);}}
    return n;
}
ssize_t send(int fd,const void *buffer,size_t length,int flags)
{
    (void)flags;++sends;
    if(mode==REFUSAL||(mode==MIXED&&ntohs(target.sin_port)==54)){errno=ECONNREFUSED;return -1;}
    if(mode==TCP_RESET&&fd==4&&!resets++){errno=ECONNRESET;return -1;}
    const unsigned char *p=buffer;
    if(fd==4){qsize=(int)_getshort(p);memcpy(query,p+2,(size_t)qsize);int n=response(tcpframe+2);if(mode==OVERSIZED){memset(tcpframe+2+n,0,300-(size_t)n);n=300;}putshort((uint16_t)n,tcpframe);tcp_size=(size_t)n+2;tcp_pos=0;}
    else {qsize=(int)length;memcpy(query,buffer,length);}
    return (ssize_t)length;
}
ssize_t sendto(int fd,const void *buffer,size_t length,int flags,const struct sockaddr *address,socklen_t size)
{memcpy(&target,address,sizeof(target));(void)size;return send(fd,buffer,length,flags);}
ssize_t recv(int fd,void *buffer,size_t length,int flags)
{
    (void)fd;(void)flags;
    if(tcp_pos>=tcp_size){errno=EAGAIN;return -1;}
    size_t n=tcp_size-tcp_pos;if(n>length)n=length;if(n>3)n=3;
    memcpy(buffer,tcpframe+tcp_pos,n);tcp_pos+=n;return (ssize_t)n;
}
ssize_t recvfrom(int fd,void *buffer,size_t length,int flags,struct sockaddr *address,socklen_t *size)
{
    (void)fd;(void)flags;++received;++now;
    if(mode==SILENT||mode==MIXED){now+=100;errno=EAGAIN;return -1;}
    if(mode==INTERRUPTED){errno=EINTR;return -1;}
    unsigned char answer[2048];int n=response(answer);
    struct sockaddr_in from=target;
    if(received==1&&mode==WRONG_ID)answer[0]^=1;
    if(received==1&&mode==WRONG_SOURCE)from.sin_port=htons(99);
    if(received==1&&mode==WRONG_QUESTION)answer[13]^=1;
    if(mode==TRUNCATED)answer[2]|=2;
    memcpy(address,&from,sizeof(from));*size=sizeof(from);if((size_t)n>length)n=(int)length;memcpy(buffer,answer,(size_t)n);return n;
}
static FILE file;static const char *aliases;
FILE *fopen(const char *path,const char *how){(void)how;if(strcmp(path,"/aliases")||!aliases)return NULL;file=(FILE){.buffer=(unsigned char *)aliases};return &file;}
int fclose(FILE *f){(void)f;return 0;}
void rewind(FILE *f){f->position=0;}
int ferror(FILE *f){(void)f;return 0;}
int getc(FILE *f){int c=f->buffer[f->position];if(c)++f->position;return c?c:EOF;}
char *getenv(const char *name){return !strcmp(name,"HOSTALIASES")&&aliases?"/aliases":NULL;}
int fprintf(FILE *f,const char *s,...){(void)f;(void)s;return 0;}
int fputs(const char *s,FILE *f){(void)s;(void)f;return 0;}
int fputc(int c,FILE *f){(void)f;return c;}
static void setup(int scenario)
{
    _res_close();memset(&_res,0,sizeof(_res));_res.options=RES_INIT|RES_DEFAULT;_res.retry=1;_res.retrans=1;_res.nscount=1;
    _res.nsaddr=(struct sockaddr_in){AF_INET,htons(53),{htonl(0x7f000001)},{0}};
    mode=scenario;received=sends=connects=resets=0;now=0;aliases=NULL;
}
int main(void)
{
    unsigned char answer[1024];
    for(int m=VALID;m<=WRONG_QUESTION;++m){setup(m);CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))>0&&h_errno==0);CHECK(received==(m?2:1));}
    setup(SILENT);CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))==-1&&h_errno==TRY_AGAIN&&errno==EAGAIN);
    setup(REFUSAL);CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))==-1&&errno==ECONNREFUSED);
    setup(SERVFAIL_FIRST);_res.nscount=2;_res.nsaddr_list[1]=_res.nsaddr;_res.nsaddr_list[1].sin_port=htons(54);
    CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))>0&&sends==2&&connects==0);
    setup(MIXED);_res.nscount=2;_res.nsaddr_list[1]=_res.nsaddr;_res.nsaddr_list[1].sin_port=htons(54);
    CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))==-1&&errno!=ECONNREFUSED);
    setup(TRUNCATED);CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))>0&&sends==2);
    setup(TCP_RESET);_res.options|=RES_USEVC|RES_STAYOPEN;CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))>0&&resets==2&&connects==2);
    int before=connects;CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))>0&&connects==before);
    setup(OVERSIZED);_res.options|=RES_USEVC|RES_STAYOPEN;
    CHECK(res_query("example.test",C_IN,T_A,answer,64)==64&&(answer[2]&2)&&tcp_pos==tcp_size);
    CHECK(res_query("example.test",C_IN,T_A,answer,64)==64&&connects==1);
    setup(NX);CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))==-1&&h_errno==HOST_NOT_FOUND&&errno==0);
    setup(NODATA);CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))==-1&&h_errno==NO_DATA);
    setup(INTERRUPTED);CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))==-1&&errno==EINTR&&sends==1);
    setup(SEARCH);_res.dnsrch[0]="first.test";_res.dnsrch[1]="second.test";
    CHECK(res_search("host",C_IN,T_A,answer,sizeof(answer))>0&&sends==2);
    setup(VALID);aliases="alias canonical.test\n";CHECK(res_search("Alias",C_IN,T_A,answer,sizeof(answer))>0&&sends==1);
    CHECK(!strcmp(hostalias("alias"),"canonical.test"));
    setup(VALID);CHECK(res_querydomain("example.test.",NULL,C_IN,T_A,answer,sizeof(answer))>0);
    _res_close();puts("PASS: scripted resolver transport, reply validation, failover, TCP/reset/drain, errors and search");return 0;
}
