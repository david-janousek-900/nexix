#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do {if(!(c)){printf("resolver check failed: %d errno=%d h_errno=%d\n",__LINE__,errno,h_errno);return 1;}}while(0)
static int readall(int fd,unsigned char *p,unsigned n)
{while(n){ssize_t got=read(fd,p,n);if(got<=0)return -1;p+=got;n-=(unsigned)got;}return 0;}
static int reply(unsigned char *buf,int size,int mode)
{
    buf[2]=0x81;buf[3]=0x80;putshort(1,buf+6);
    int n=size;
    if(mode==9){
        putshort(3,buf+6);
        buf[n++]=0xc0;buf[n++]=12;putshort(T_CNAME,buf+n);putshort(C_IN,buf+n+2);putlong(60,buf+n+4);n+=8;
        int d=dn_comp("canonical.test",buf+n+2,200,NULL,NULL);putshort((uint16_t)d,buf+n);n+=2+d;
        for(unsigned i=0;i<2;++i){n+=dn_comp("canonical.test",buf+n,200,NULL,NULL);putshort(T_A,buf+n);putshort(C_IN,buf+n+2);putlong(60,buf+n+4);putshort(4,buf+n+8);n+=10;putlong(0xc000022a+i,buf+n);n+=4;}
        return n;
    }
    if(mode==10){
        buf[n++]=0xc0;buf[n++]=12;putshort(T_PTR,buf+n);putshort(C_IN,buf+n+2);putlong(60,buf+n+4);n+=8;
        int d=dn_comp("server.test",buf+n+2,200,NULL,NULL);putshort((uint16_t)d,buf+n);return n+2+d;
    }
    buf[n++]=0xc0;buf[n++]=12;putshort(T_A,buf+n);putshort(C_IN,buf+n+2);putlong(60,buf+n+4);putshort(4,buf+n+8);n+=10;
    buf[n++]=192;buf[n++]=0;buf[n++]=2;buf[n++]=42;
    if(mode==11)putshort(255,buf+size+10);
    return n;
}
/* 0 valid, 1 wrong ID, 2 wrong source, 3 wrong question, 4 truncation to
 * TCP, 5 oversized TCP plus a second query on the same connection,
 * 6 NXDOMAIN, 7 NODATA, 8 silent server. */
static int server(int ready,unsigned port,int mode)
{
    struct sockaddr_in local={AF_INET,htons((uint16_t)port),{htonl(INADDR_LOOPBACK)},{0}},peer;
    int udp=socket(AF_INET,SOCK_DGRAM,0),tcp=-1;
    CHECK(udp>=0&&bind(udp,(struct sockaddr *)&local,sizeof(local))==0);
    if(mode==4||mode==5){tcp=socket(AF_INET,SOCK_STREAM,0);int one=1;CHECK(tcp>=0&&!setsockopt(tcp,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one))&&!bind(tcp,(struct sockaddr *)&local,sizeof(local))&&!listen(tcp,1));}
    CHECK(write(ready,"R",1)==1);close(ready);
    unsigned char message[2048];socklen_t length=sizeof(peer);
    int n=(int)recvfrom(udp,message,1024,0,(struct sockaddr *)&peer,&length);CHECK(n>=12);
    if(mode==8){sleep(2);close(udp);return 0;}
    int size=reply(message,n,mode);
    if(mode==1){message[0]^=1;CHECK(sendto(udp,message,size,0,(struct sockaddr *)&peer,length)==size);message[0]^=1;}
    if(mode==2){int wrong=socket(AF_INET,SOCK_DGRAM,0);CHECK(wrong>=0);CHECK(sendto(wrong,message,size,0,(struct sockaddr *)&peer,length)==size);close(wrong);}
    if(mode==3){message[13]^=1;CHECK(sendto(udp,message,size,0,(struct sockaddr *)&peer,length)==size);message[13]^=1;}
    if(mode==4||mode==5)message[2]|=2;
    if(mode==6){message[3]=0x83;putshort(0,message+6);}
    if(mode==7)putshort(0,message+6);
    CHECK(sendto(udp,message,size,0,(struct sockaddr *)&peer,length)==size);close(udp);
    if(tcp>=0){
        int fd=accept(tcp,NULL,NULL);CHECK(fd>=0);close(tcp);
        for(unsigned i=0;i<(mode==5?2u:1u);++i){
            unsigned char prefix[2];CHECK(!readall(fd,prefix,2));unsigned bytes=_getshort(prefix);CHECK(bytes<=1024&&!readall(fd,message+2,bytes));
            size=reply(message+2,(int)bytes,mode);
            if(mode==5){memset(message+2+size,0,1024-(unsigned)size);size=1024;}
            putshort((uint16_t)size,message);CHECK(write(fd,message,(size_t)size+2)==size+2);
        }
        close(fd);
    }
    return 0;
}
int main(void)
{
    for(int mode=0;mode<=11;++mode){
        int ready[2];CHECK(!pipe(ready));unsigned port=5300+(unsigned)mode;
        pid_t child=fork();CHECK(child>=0);
        if(!child){close(ready[0]);_exit(server(ready[1],port,mode));}
        close(ready[1]);char token;CHECK(read(ready[0],&token,1)==1);close(ready[0]);
        memset(&_res,0,sizeof(_res));_res.options=RES_INIT|RES_DEFAULT;_res.retry=1;_res.retrans=1;_res.nscount=1;
        _res.nsaddr=(struct sockaddr_in){AF_INET,htons((uint16_t)port),{htonl(INADDR_LOOPBACK)},{0}};
        if(mode==5)_res.options|=RES_USEVC|RES_STAYOPEN;
        /* mode 5 still sends an initial UDP request to synchronize the server. */
        if(mode==5)_res.options&=~RES_USEVC;
        unsigned char answer[512];int n;
        if(mode>=9){
            struct in_addr addr={htonl(0xc000022a)};
            struct hostent *host=mode==10?gethostbyaddr(&addr,4,AF_INET):gethostbyname("example.test");
            if(mode==9)CHECK(host&&!strcmp(host->h_name,"canonical.test")&&!strcmp(host->h_aliases[0],"example.test")&&host->h_addr_list[1]&&!host->h_addr_list[2]);
            if(mode==10)CHECK(host&&!strcmp(host->h_name,"server.test")&&!memcmp(host->h_addr,&addr,4));
            if(mode==11)CHECK(!host&&h_errno==NO_RECOVERY);
            n=0;
        } else n=res_query("example.test",C_IN,T_A,answer,sizeof(answer));
        if(mode==6)CHECK(n==-1&&h_errno==HOST_NOT_FOUND&&errno==0);
        else if(mode==7)CHECK(n==-1&&h_errno==NO_DATA);
        else if(mode==8)CHECK(n==-1&&h_errno==TRY_AGAIN&&errno!=ECONNREFUSED);
        else if(mode<9)CHECK(n>0&&_getshort(answer+6)==1);
        if(mode==5){CHECK(n==sizeof(answer)&&(answer[2]&2));_res.options|=RES_USEVC;CHECK(res_query("second.test",C_IN,T_A,answer,sizeof(answer))==(int)sizeof(answer));}
        _res_close();int status;CHECK(wait(&status)==child&&status==0);
    }
    _res.options=RES_INIT|RES_DEFAULT;_res.nscount=1;_res.retry=1;_res.retrans=1;
    _res.nsaddr=(struct sockaddr_in){AF_INET,htons(5399),{htonl(INADDR_LOOPBACK)},{0}};
    unsigned char answer[512];CHECK(res_query("example.test",C_IN,T_A,answer,sizeof(answer))==-1&&errno==ECONNREFUSED);
    _res.dnsrch[0]="";
    CHECK(gethostbyname("localhost")&&!strcmp(gethostbyname("localhost")->h_name,"localhost"));
    CHECK(gethostbyname("127.1")&&gethostbyname("127.1")->h_length==4);
    puts("RESOLVER_GUEST_OK");return 0;
}
