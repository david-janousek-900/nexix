#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do { if(!(c)) return __LINE__; } while(0)
int errno;
static FILE streams[4];
static const char *service_data,*protocol_data,*network_data;
static int live, opens, fail_after=-1;
FILE *fopen(const char *path,const char *mode)
{
    const char *data=!strcmp(path,_PATH_SERVICES)?service_data:!strcmp(path,_PATH_PROTOCOLS)?protocol_data:!strcmp(path,_PATH_NETWORKS)?network_data:NULL;
    if(!data||strcmp(mode,"r")) {errno=ENOENT;return NULL;}
    for(unsigned i=0;i<4;++i) if(!streams[i].used) {
        streams[i]=(FILE){.used=1,.buffer=(unsigned char *)data}; ++live; ++opens;return &streams[i];
    }
    errno=EMFILE;return NULL;
}
int fclose(FILE *file){file->used=0;--live;return 0;}
void rewind(FILE *file){file->position=0;file->error=0;}
int ferror(FILE *file){return file->error;}
int getc(FILE *file)
{
    if(fail_after>=0&&file->position>=(size_t)fail_after){file->error=1;errno=EIO;return EOF;}
    int c=file->buffer[file->position];if(!c)return EOF;
    ++file->position;return c;
}
int main(void)
{
    service_data="# comment\nbad\nnegative -1/tcp\nlarge 65536/tcp\nnotnum 8x/tcp\nmissing 80/\n domain 53/udp nameserver # tail\nhttp 80/tcp www\nfinal 65535/tcp";
    protocol_data="bad -1\nlarge 256\ntcp 6 TCP\nudp 17 UDP";
    network_data="bad invalid\nloopback 127 loop\nlocal 192.0.2 fixture";
    struct servent *s=getservent();CHECK(s&&s->s_port==htons(53)&&!strcmp(s->s_aliases[0],"nameserver"));
    CHECK(getservent()->s_port==htons(80));CHECK(getservent()->s_port==htons(65535));CHECK(!getservent());
    setservent(1);CHECK(getservent()->s_port==htons(53));int prior=opens;
    s=getservbyname("www","tcp");CHECK(s&&s->s_port==htons(80)&&opens==prior&&live==1);
    CHECK(!getservbyname("www","udp")&&live==1);endservent();CHECK(!live);
    CHECK(getservbyport(htons(53),NULL)&&!live);CHECK(!getservbyname("absent",NULL)&&!live);
    CHECK(getprotobyname("TCP")->p_proto==6&&!live);CHECK(getprotobynumber(17)&&!live);
    setprotoent(1);CHECK(getprotoent()->p_proto==6);CHECK(getprotobynumber(17)&&live==1);endprotoent();
    CHECK(getnetbyname("fixture")->n_net==0xc00002&&!live);
    CHECK(getnetbyaddr(127,AF_INET)&&!live);CHECK(!getnetbyaddr(127,AF_UNSPEC)&&errno==EAFNOSUPPORT);
    CHECK(!getservbyname(NULL,NULL)&&errno==EINVAL);CHECK(!getprotobyname(NULL));CHECK(!getnetbyname(NULL));
    CHECK(inet_network("127")==127&&inet_network("0xc0.0.02")==0xc00002);
    CHECK(inet_network("256.0")==INADDR_NONE&&inet_network("1..2")==INADDR_NONE&&inet_network("08")==INADDR_NONE);
    CHECK(inet_network("1.2.3.4.5")==INADDR_NONE&&inet_network("")==INADDR_NONE);
    struct in_addr a=inet_makeaddr(127,1);CHECK(a.s_addr==htonl(0x7f000001)&&inet_netof(a)==127&&inet_lnaof(a)==1);
    a=inet_makeaddr(0xac10,0x1234);CHECK(ntohl(a.s_addr)==0xac101234&&inet_netof(a)==0xac10&&inet_lnaof(a)==0x1234);
    a=inet_makeaddr(0xc00002,99);CHECK(ntohl(a.s_addr)==0xc0000263&&inet_netof(a)==0xc00002&&inet_lnaof(a)==99);
    a=inet_makeaddr(0xe0000000,5);CHECK(ntohl(a.s_addr)==0xe0000005);
    char longline[2200];memset(longline,'x',2000);strcpy(longline+2000,"\ngood 42/tcp alias\n");
    service_data=longline;CHECK(getservbyname("good",NULL)->s_port==htons(42)&&!live);
    char aliases[700]="many 9/tcp";for(unsigned i=0;i<35;++i)strcat(aliases," alias");strcat(aliases,"\n");
    service_data=aliases;s=getservent();CHECK(s&&s->s_aliases[34]&&!s->s_aliases[35]);endservent();
    strcat(aliases,"ok 10/tcp\n");char *end=strchr(aliases,'\n');memmove(end+6,end,strlen(end)+1);memcpy(end," extra",6);
    CHECK(getservbyname("ok",NULL)&&!live);CHECK(!getservbyname("many",NULL)&&!live);
    service_data="bad 7/tcp\n";fail_after=4;CHECK(!getservbyname("bad",NULL)&&errno==EIO&&!live);fail_after=-1;
    service_data=NULL;CHECK(!getservent()&&errno==ENOENT&&!live);
    for(int i=0;i<5;++i) { CHECK(hstrerror(i)&&*hstrerror(i)); }
    CHECK(!strcmp(hstrerror(-1),"Unknown resolver error"));
    puts("PASS: bounded network databases, aliases, malformed/long records, stayopen, I/O errors and classful conversions");
    return 0;
}
