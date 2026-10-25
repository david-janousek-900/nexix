#include <netdb.h>
#include <resolv.h>
#include <sys/times.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do {if(!(c)){printf("resolver acceptance failed: %d errno=%d h_errno=%d\n",__LINE__,errno,h_errno);return 1;}}while(0)
static void configuration(unsigned port)
{
    _res_close();memset(&_res,0,sizeof(_res));_res.options=RES_INIT|RES_DEFAULT;_res.retrans=1;_res.retry=1;_res.nscount=1;
    _res.nsaddr=(struct sockaddr_in){AF_INET,htons((uint16_t)port),{htonl(0x0a000202)},{0}};_res.dnsrch[0]="";
}
int main(int argc,char **argv)
{
    CHECK(argc==3);unsigned first=(unsigned)atoi(argv[1]),second=(unsigned)atoi(argv[2]);configuration(first);
    struct hostent *host=gethostbyname("multi.test");CHECK(host&&host->h_addr_list[1]&&!host->h_addr_list[2]);
    struct in_addr address;memcpy(&address,host->h_addr,4);CHECK(ntohl(address.s_addr)==0xc000020a);
    host=gethostbyname("alias.test");CHECK(host&&!strcmp(host->h_name,"canonical.test")&&!strcmp(host->h_aliases[0],"alias.test")&&!strcmp(host->h_aliases[1],"middle.test"));
    host=gethostbyaddr(&address,4,AF_INET);CHECK(host&&!strcmp(host->h_name,"reverse.test")&&!memcmp(host->h_addr,&address,4));
    FILE *file=fopen("/etc/hosts","a");CHECK(file);CHECK(fputs("192.0.2.77 missing.test empty.test\n",file)>=0&&!fclose(file));
    CHECK(!gethostbyname("missing.test")&&h_errno==HOST_NOT_FOUND&&errno==0);
    CHECK(!gethostbyname("empty.test")&&h_errno==NO_DATA);
    CHECK(!gethostbyname("servfail.test")&&h_errno==TRY_AGAIN);
    CHECK(!gethostbyname("refused.test")&&h_errno==NO_RECOVERY);
    CHECK(gethostbyname("truncated.test"));
    CHECK(gethostbyname("badid.test"));CHECK(gethostbyname("badsource.test"));CHECK(gethostbyname("badquestion.test"));
    struct tms clock;clock_t before=times(&clock);
    _res.retry=2;CHECK(!gethostbyname("silent.test.")&&h_errno==TRY_AGAIN&&errno!=ECONNREFUSED);
    CHECK(times(&clock)-before>=300); /* one- and two-second waits at 100 Hz */
    configuration(first);_res.nscount=2;_res.nsaddr_list[1]=_res.nsaddr;_res.nsaddr_list[1].sin_port=htons((uint16_t)second);
    CHECK(gethostbyname("failover.test"));
    configuration(first);_res.dnsrch[0]="missing.search.test";_res.dnsrch[1]="search.test";
    host=gethostbyname("host");CHECK(host&&!strcmp(host->h_name,"host.search.test"));
    configuration(first);_res.nsaddr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);_res.nsaddr.sin_port=htons(53);
    host=gethostbyname("localhost");CHECK(host&&!strcmp(host->h_name,"localhost"));
    host=gethostbyaddr(&(struct in_addr){htonl(INADDR_LOOPBACK)},4,AF_INET);CHECK(host&&!strcmp(host->h_name,"localhost"));
    endhostent();puts("RESOLVER_ACCEPTANCE_OK");return 0;
}
