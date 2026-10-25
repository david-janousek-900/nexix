#include <resolv.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
int errno;
static uint32_t random32(void);
static int entropy_fail;
int getentropy(void *out,size_t length)
{if(entropy_fail){errno=EIO;return -1;} unsigned char *p=out;for(size_t i=0;i<length;++i)p[i]=(unsigned char)random32();return 0;}
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
int fprintf(FILE *f,const char *s,...){(void)f;(void)s;return 0;}
int fputs(const char *s,FILE *f){(void)f;(void)s;return 0;}
int fputc(int c,FILE *f){(void)f;return c;}
static uint32_t seed=0x4e584447;
static uint32_t random32(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void)
{
    _Static_assert(sizeof(HEADER)==12,"DNS header layout");
    HEADER h={0};h.qr=1;h.rd=1;h.ra=1;h.rcode=3;
    CHECK(((unsigned char *)&h)[2]==0x81&&((unsigned char *)&h)[3]==0x83);
    unsigned char msg[1024]={0};char name[1025];
    const unsigned char vector[]={3,'w','w','w',7,'e','x','a','m','p','l','e',3,'c','o','m',0};
    CHECK(dn_comp("www.example.com.",msg,1024,NULL,NULL)==17&&!memcmp(msg,vector,17));
    CHECK(dn_expand(msg,msg+17,msg,name,sizeof(name))==17&&!strcmp(name,"www.example.com"));
    unsigned char *ptrs[16]={msg,NULL};
    CHECK(dn_comp("www.example.com",msg+12,100,ptrs,ptrs+16)==17);
    CHECK(dn_comp("mail.example.com",msg+29,100,ptrs,ptrs+16)==7);
    CHECK(msg[34]==0xc0&&msg[35]==16);
    CHECK(dn_expand(msg,msg+36,msg+29,name,sizeof(name))==7&&!strcmp(name,"mail.example.com"));
    CHECK(dn_skipname(msg+29,msg+36)==7);
    CHECK(dn_comp("a\\.b.\\000\\255",msg,1024,NULL,NULL)>0);
    CHECK(dn_expand(msg,msg+20,msg,name,sizeof(name))>0&&!strcmp(name,"a\\.b.\\000\\255"));
    CHECK(dn_comp("",msg,1,NULL,NULL)==1&&!msg[0]);
    CHECK(dn_comp(".",msg,1,NULL,NULL)==1);
    CHECK(dn_comp("a..b",msg,1024,NULL,NULL)<0&&dn_comp("a\\",msg,1024,NULL,NULL)<0);
    char edge[300];memset(edge,'a',sizeof(edge));edge[63]=0;CHECK(dn_comp(edge,msg,1024,NULL,NULL)==65);
    edge[63]='a';edge[64]=0;CHECK(dn_comp(edge,msg,1024,NULL,NULL)<0);
    unsigned at=0;for(int i=0;i<4;++i){unsigned n=i==3?61:63;memset(edge+at,'x',n);at+=n;edge[at++]='.';}edge[at]=0;
    CHECK(dn_comp(edge,msg,1024,NULL,NULL)==255);edge[at-1]='x';edge[at]='.';edge[at+1]=0;CHECK(dn_comp(edge,msg,1024,NULL,NULL)<0);
    unsigned char bad[]={0xc0,0};CHECK(dn_expand(bad,bad+2,bad,name,sizeof(name))<0);
    unsigned char forward[]={0xc0,2,0};CHECK(dn_expand(forward,forward+3,forward,name,sizeof(name))<0);
    unsigned char cycle[]={1,'a',0xc0,0};CHECK(dn_expand(cycle,cycle+4,cycle,name,sizeof(name))<0);
    unsigned char shortlabel[]={3,'a'};CHECK(dn_expand(shortlabel,shortlabel+2,shortlabel,name,sizeof(name))<0);
    CHECK(dn_skipname(bad,bad+1)<0);
    CHECK(dn_comp("example.com",msg,3,NULL,NULL)<0);
    int n=res_mkquery(QUERY,"example.com",C_IN,T_A,NULL,0,NULL,msg,sizeof(msg));
    CHECK(n==29&&_getshort(msg+4)==1&&msg[2]==1&&_getshort(msg+25)==T_A);
    CHECK(fp_nquery(msg,n,(FILE *)1)==0&&fp_nquery(msg,n-1,(FILE *)1)<0);
    /* RFC-format RDATA vectors: all printer reads must respect RDLENGTH. */
    static const unsigned char mx[]={0,10,0xc0,12};
    static const unsigned char txt[]={7,'f','i','x','t','u','r','e'};
    static const unsigned char hinfo[]={3,'c','p','u',2,'o','s'};
    static const unsigned char soa[]={0xc0,12,0xc0,12,0,0,0,1,0,0,0,60,0,0,0,60,0,0,0,60,0,0,0,60};
    static const unsigned char wks[]={192,0,2,1,6,128,1};
    struct vector {int type;const unsigned char *data;unsigned size;int valid;} vectors[]={
        {T_MX,mx,sizeof(mx),1},{T_TXT,txt,sizeof(txt),1},{T_HINFO,hinfo,sizeof(hinfo),1},
        {T_SOA,soa,sizeof(soa),1},{T_WKS,wks,sizeof(wks),1},
        {T_MX,mx,2,0},{T_TXT,txt,0,0},{T_HINFO,txt,sizeof(txt),0},{T_SOA,soa,4,0},{T_WKS,wks,4,0}};
    for(unsigned i=0;i<sizeof(vectors)/sizeof(*vectors);++i){
        struct vector *v=&vectors[i];int end=res_mkquery(QUERY,"example.com",C_IN,v->type,NULL,0,NULL,msg,sizeof(msg));
        msg[2]|=128;putshort(1,msg+6);msg[end++]=0xc0;msg[end++]=12;
        putshort((uint16_t)v->type,msg+end);putshort(C_IN,msg+end+2);putlong(60,msg+end+4);putshort((uint16_t)v->size,msg+end+8);end+=10;
        memcpy(msg+end,v->data,v->size);end+=(int)v->size;
        CHECK((fp_nquery(msg,end,(FILE *)1)==0)==v->valid);
        CHECK(fp_nquery(msg,end-1,(FILE *)1)<0);
    }
    unsigned char a[4]={1,2,3,4};CHECK(res_mkquery(IQUERY,NULL,C_IN,T_A,a,4,NULL,msg,sizeof(msg))==27);
    CHECK(res_mkquery(99,NULL,C_IN,T_A,NULL,0,NULL,msg,sizeof(msg))<0);
    putlong(0x12345678,msg);putshort(0xabcd,msg+4);CHECK(_getlong(msg)==0x12345678&&_getshort(msg+4)==0xabcd);
    for(unsigned i=0;i<40000;++i){
        unsigned length=random32()%513;
        for(unsigned j=0;j<length;++j)msg[j]=(unsigned char)random32();
        unsigned offset=length?random32()%length:0;
        (void)dn_expand(msg,msg+length,msg+offset,name,(int)(random32()%sizeof(name)));
        (void)dn_skipname(msg+offset,msg+length);
        (void)fp_nquery(msg,(int)length,(FILE *)1);
        char input[300];unsigned chars=random32()%sizeof(input);
        for(unsigned j=0;j<chars;++j)input[j]=(char)(32+random32()%95);
        input[chars]=0; /* chars is at most 299 */
        (void)dn_comp(input,msg,(int)(random32()%256),NULL,NULL);
    }
    entropy_fail=1;CHECK(res_mkquery(QUERY,"example.test",C_IN,T_A,NULL,0,NULL,msg,sizeof(msg))==-1&&errno==EIO);
    puts("PASS: DNS codec vectors, compression, LP64 header layout and 40000 seeded mutations");return 0;
}
