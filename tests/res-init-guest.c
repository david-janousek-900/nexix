#include <resolv.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){printf("res_init check failed: %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    unlink("/etc/resolv.conf");
    CHECK(sethostname("box.a.b.example.test",20)==0);
    CHECK(res_init()==0&&_res.nscount==1&&ntohl(_res.nsaddr.sin_addr.s_addr)==0x7f000001);
    CHECK(!strcmp(_res.defdname,"a.b.example.test")&&!strcmp(_res.dnsrch[2],"example.test"));
    FILE *f=fopen("/etc/resolv.conf","w");CHECK(f);
    CHECK(fputs("nameserver invalid\nnameserver 127.0.0.2\nnameserver 127.0.0.3\nnameserver 127.0.0.4\nnameserver 127.0.0.5\nsearch a.test b.test c.test d.test e.test f.test g.test\n",f)>=0);CHECK(!fclose(f));
    CHECK(!res_init()&&_res.nscount==3&&ntohl(_res.nsaddr.sin_addr.s_addr)==0x7f000002);
    CHECK(!strcmp(_res.defdname,"a.test")&&!strcmp(_res.dnsrch[5],"f.test")&&!_res.dnsrch[6]);
    extern char **environ;char *env[]={"LOCALDOMAIN=env.a.test ignored.test",NULL};char **old=environ;environ=env;
    CHECK(!res_init()&&!strcmp(_res.defdname,"env.a.test")&&!strcmp(_res.dnsrch[1],"a.test"));environ=old;
    puts("RES_INIT_GUEST_OK");return 0;
}
