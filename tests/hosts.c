#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "../libc/netdb_private.h"
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
int errno;
struct __res_state _res;
void _res_close(void) {}
static FILE stream;
static const char *data;
static int live, opens,fail_after=-1;
FILE *fopen(const char *path,const char *mode)
{
    if(strcmp(path,_PATH_HOSTS)||strcmp(mode,"r")||!data){errno=ENOENT;return NULL;}
    if(live){errno=EMFILE;return NULL;}
    stream=(FILE){.used=1,.buffer=(unsigned char *)data};++live;++opens;return &stream;
}
int fclose(FILE *f){f->used=0;--live;return 0;}
void rewind(FILE *f){f->position=0;f->error=0;}
int ferror(FILE *f){return f->error;}
int getc(FILE *f)
{
    if(fail_after>=0&&f->position>=(size_t)fail_after){errno=EIO;f->error=1;return EOF;}
    int c=f->buffer[f->position];if(c)++f->position;return c?c:EOF;
}
int main(void)
{
    data="# comment\ninvalid bad\n1.2.3.256 bad\n127.0.0.1 LocalHost ALIAS loop # tail\n255.255.255.255 broadcast\n0.0.0.0 any";
    struct hostent *h=gethostent();CHECK(h&&!strcmp(h->h_name,"LocalHost")&&h->h_length==4&&h->h_addrtype==AF_INET);
    CHECK(!strcmp(h->h_aliases[0],"ALIAS")&&!h->h_aliases[2]&&!h->h_addr_list[1]);
    CHECK(gethostent()->h_addr&&gethostent()->h_addr);CHECK(!gethostent());endhostent();CHECK(!live);
    h=_gethtbyname("aLiAs");CHECK(h&&!strcmp(h->h_name,"LocalHost")&&!live&&h_errno==0);
    CHECK(_gethtbyname("localhost")&&!_gethtbyname("bad")&&h_errno==HOST_NOT_FOUND&&!live);
    struct in_addr a={htonl(0x7f000001)};CHECK(_gethtbyaddr(&a,4,AF_INET)&&!live);
    CHECK(!_gethtbyaddr(&a,1,AF_INET)&&errno==EINVAL);
    CHECK(!_gethtbyaddr(&a,4,AF_UNSPEC)&&errno==EAFNOSUPPORT);CHECK(!_gethtbyname(NULL)&&errno==EINVAL);
    sethostent(1);int prior=opens;CHECK(_gethtbyname("alias")&&live==1&&opens==prior);
    CHECK(gethostent()&&!strcmp(gethostent()->h_name,"any"));sethostent(0);CHECK(gethostent());endhostent();CHECK(!live);
    char big[2300];memset(big,'x',2000);strcpy(big+2000,"\n127.1 good\n");data=big;
    CHECK(_gethtbyname("good")&&!_gethtbyname("x")&&!live);
    char many[700]="127.1 many";for(int i=0;i<35;++i)strcat(many," alias");strcat(many,"\n");data=many;
    h=gethostent();CHECK(h&&h->h_aliases[34]&&!h->h_aliases[35]);endhostent();
    data="127.1 partial\n";fail_after=3;CHECK(!_gethtbyname("partial")&&errno==EIO&&!live);fail_after=-1;
    data=NULL;CHECK(!gethostent()&&errno==ENOENT&&!live);endhostent();
    puts("PASS: hosts parsing, IPv4 validation, case-insensitive aliases, iteration, stayopen and failures");return 0;
}
