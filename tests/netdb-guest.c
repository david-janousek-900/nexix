#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if(!(c)){printf("netdb check failed: %d\n",__LINE__);return 1;} } while(0)
int main(void)
{
    struct servent *s=getservbyname("domain","udp");CHECK(s&&s->s_port==htons(53));
    s=getservbyport(htons(23),"tcp");CHECK(s&&!strcmp(s->s_name,"telnet"));
    CHECK(!getservbyname("no-such-service",NULL));
    struct protoent *p=getprotobyname("tcp");CHECK(p&&p->p_proto==6);
    CHECK(getprotobynumber(17)&&getprotobynumber(17)->p_proto==17);
    CHECK(getnetbyname("your-net")&&getnetbyname("your-net")->n_net==127);
    setservent(1);CHECK(getservent());CHECK(getservbyname("domain","udp"));endservent();
    setprotoent(1);CHECK(getprotoent());endprotoent();
    setnetent(1);CHECK(getnetent());endnetent();
    sethostent(1);
    struct hostent *h=gethostent();CHECK(h&&!strcmp(h->h_name,"localhost")&&h->h_addrtype==AF_INET&&h->h_length==4);
    CHECK(!strcmp(inet_ntoa(*(struct in_addr *)h->h_addr),"127.0.0.1")&&!h->h_aliases[0]);
    CHECK(!gethostent());sethostent(0);CHECK(gethostent());endhostent();
    puts("NETDB_GUEST_OK");return 0;
}
