/*
 * ++Copyright++ 1985, 1989
 * -
 * Copyright (c) 1985, 1989
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

/* Native adapters for BIND 4.9.2 tools/nslookup. The original command lexer
 * remains the interactive grammar; resolver and socket adapters are native. */
#include "res.h"
#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int query_type=T_A,query_class=C_IN,status;
static struct sockaddr_in initial_server;
static volatile sig_atomic_t interrupted,reading_input;
static jmp_buf prompt_jump;
char rootServerName[]="ns.nic.ddn.mil";
static void interrupt(int n)
{
    (void)n;interrupted=1;signal(SIGINT,interrupt);
    /* Nexix signal contexts live in user memory and support handler longjmp.
     * Jump only out of lexer input, never through resolver/stdio allocation. */
    if(reading_input)longjmp(prompt_jump,1);
}
int nslookup_input(FILE *file)
{
    if(file!=stdin)return getc(file);
    reading_input=1;
    if(interrupted){reading_input=0;return EOF;}
    unsigned char c;ssize_t n=read(fileno(file),&c,1);
    reading_input=0;
    return n==1?c:EOF;
}
static int equal(const char *a,const char *b)
{while(*a&&*b){unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;if(x>='a'&&x<='z')x-=32;if(y>='a'&&y<='z')y-=32;if(x!=y)return 0;}return *a==*b;}
static unsigned words(const char *line,char buffer[2048],char **out,unsigned limit)
{
    size_t n=strlen(line);if(n>=2048)return 0;memcpy(buffer,line,n+1);unsigned count=0;char *p=buffer;
    while(*p){while(*p==' '||*p=='\t'||*p=='\n')++p;if(!*p)break;if(count==limit)return 0;out[count++]=p;while(*p&&*p!=' '&&*p!='\t'&&*p!='\n')++p;if(*p)*p++=0;}
    return count;
}
static int type_number(const char *name)
{
    struct {const char *name;int value;} types[]={
        {"A",T_A},{"NS",T_NS},{"CNAME",T_CNAME},{"SOA",T_SOA},{"PTR",T_PTR},{"MX",T_MX},{"TXT",T_TXT},{"HINFO",T_HINFO},{"WKS",T_WKS},{"ANY",T_ANY},{"AXFR",T_AXFR}};
    for(unsigned i=0;i<sizeof(types)/sizeof(*types);++i)if(equal(name,types[i].name))return types[i].value;
    char *end;long n=strtol(name,&end,10);return end!=name&&!*end&&n>0&&n<=65535?(int)n:-1;
}
static int class_number(const char *name)
{return equal(name,"IN")?C_IN:equal(name,"CHAOS")?C_CHAOS:equal(name,"HS")?C_HS:equal(name,"ANY")?C_ANY:-1;}
static void show_server(FILE *out)
{char address[16];inet_ntop(AF_INET,&_res.nsaddr.sin_addr,address,sizeof(address));fprintf(out,"Server: %s\nAddress: %s#%u\n\n",address,address,ntohs(_res.nsaddr.sin_port));}
static int select_server(const char *name,int original)
{
    struct in_addr address;
    if(!inet_aton(name,&address)){
        struct __res_state saved=_res;
        if(original){_res.nsaddr=initial_server;_res.nscount=1;_res_close();}
        struct hostent *host=gethostbyname(name);
        if(host)memcpy(&address,host->h_addr,4);
        _res_close();_res=saved;
        if(!host){herror(name);return -1;}
    }
    _res_close();_res.nsaddr.sin_addr=address;_res.nsaddr.sin_family=AF_INET;_res.nscount=1;return 0;
}
void SetDefaultServer(const char *line,int original)
{
    char buffer[2048],*w[4];unsigned n=words(line,buffer,w,4);if(!n){status=1;return;}
    const char *name=n>=2?w[1]:w[0];if(select_server(name,original)){status=1;return;}show_server(stdout);
}
void SetOption(const char *line)
{
    while(*line==' '||*line=='\t')++line;
    if(!strncmp(line,"set ",4))line+=4;
    if(*line=='-')++line;
    char buffer[2048],*w[2];unsigned n=words(line,buffer,w,2);if(n!=1){status=2;return;}
    char *value=strchr(w[0],'=');if(value)*value++=0;
    long bit=equal(w[0],"debug")?RES_DEBUG:equal(w[0],"vc")?RES_USEVC:equal(w[0],"search")?RES_DNSRCH:equal(w[0],"defname")?RES_DEFNAMES:equal(w[0],"recurse")?RES_RECURSE:0;
    if(bit&&!value){_res.options|=bit;return;}
    if(!strncmp(w[0],"no",2)&&!value){const char *s=w[0]+2;bit=equal(s,"debug")?RES_DEBUG:equal(s,"vc")?RES_USEVC:equal(s,"search")?RES_DNSRCH:equal(s,"defname")?RES_DEFNAMES:equal(s,"recurse")?RES_RECURSE:0;if(bit){_res.options&=~bit;_res_close();return;}}
    if(equal(w[0],"all")&&!value){printf("type=%s class=%s domain=%s retry=%d timeout=%d port=%u vc=%s debug=%s search=%s\n",p_type(query_type),p_class(query_class),_res.defdname,_res.retry,_res.retrans,ntohs(_res.nsaddr.sin_port),_res.options&RES_USEVC?"yes":"no",_res.options&RES_DEBUG?"yes":"no",_res.options&RES_DNSRCH?"yes":"no");return;}
    if(value){
        if(equal(w[0],"type")||equal(w[0],"querytype")){int t=type_number(value);if(t>0){query_type=t;return;}}
        else if(equal(w[0],"class")){int c=class_number(value);if(c>0){query_class=c;return;}}
        else if(equal(w[0],"domain")&&strlen(value)<sizeof(_res.defdname)){strcpy(_res.defdname,value);memset(_res.dnsrch,0,sizeof(_res.dnsrch));_res.dnsrch[0]=_res.defdname;return;}
        else {char *end;long number=strtol(value,&end,10);if(end!=value&&!*end&&number>0){if(equal(w[0],"retry")&&number<=10){_res.retry=(int)number;return;}if(equal(w[0],"timeout")&&number<=3600){_res.retrans=(int)number;return;}if(equal(w[0],"port")&&number<=65535){_res_close();_res.nsaddr.sin_port=htons((uint16_t)number);return;}}}
    }
    fprintf(stderr,"nslookup: invalid option: %s\n",line);status=2;
}
static int lookup(const char *name,FILE *out)
{
    show_server(out);
    if(query_class==C_IN&&query_type==T_A){
        struct in_addr a;struct hostent *host=inet_aton(name,&a)?gethostbyaddr(&a,4,AF_INET):gethostbyname(name);
        if(!host){if(!interrupted)herror(name);return 1;}
        fprintf(out,"Name: %s\n",host->h_name);
        for(char **p=host->h_addr_list;*p;++p){char text[16];inet_ntop(AF_INET,*p,text,sizeof(text));fprintf(out,"Address: %s\n",text);}
        for(char **p=host->h_aliases;*p;++p)fprintf(out,"Alias: %s\n",*p);
        return 0;
    }
    static unsigned char answer[65535];char reverse[80];struct in_addr a;
    if(query_type==T_PTR&&inet_aton(name,&a)){const unsigned char *p=(const unsigned char *)&a;snprintf(reverse,sizeof(reverse),"%u.%u.%u.%u.in-addr.arpa",p[3],p[2],p[1],p[0]);name=reverse;}
    int n=query_type==T_PTR?res_query(name,query_class,query_type,answer,sizeof(answer)):res_search(name,query_class,query_type,answer,sizeof(answer));if(n<0){if(!interrupted)herror(name);return 1;}
    return fp_nquery(answer,n,out)<0;
}
static FILE *output(char *line,int redirected)
{
    if(!redirected)return stdout;
    char *p=strchr(line,'>');if(!p)return NULL;*p++=0;int append=*p=='>';if(append)++p;while(*p==' '||*p=='\t')++p;
    char *end=p;while(*end&&*end!=' '&&*end!='\t'&&*end!='\n')++end;*end=0;return *p?fopen(p,append?"a":"w"):NULL;
}
static void lookup_line(const char *line,int redirected,int server)
{
    char text[2048];if(strlen(line)>=sizeof(text)){status=2;return;}strcpy(text,line);FILE *out=output(text,redirected);if(!out){perror("nslookup");status=1;return;}
    char buffer[2048],*w[3];unsigned n=words(text,buffer,w,3);struct __res_state saved=_res;
    if(n!=(server?2u:1u))status=2;
    else if(server&&select_server(w[1],0))status=1;
    else status=lookup(w[0],out);
    if(server){_res_close();_res=saved;}
    if(out!=stdout)fclose(out);
}
void LookupHost(const char *line,int redirected){lookup_line(line,redirected,0);}
void LookupHostWithServer(const char *line,int redirected){lookup_line(line,redirected,1);}
static int readall(int fd,unsigned char *p,unsigned n)
{while(n){ssize_t got=read(fd,p,n);if(got<=0)return -1;p+=got;n-=(unsigned)got;}return 0;}
static int soa_count(const unsigned char *msg,unsigned length)
{
    if(length<12)return -1;
    const unsigned char *p=msg+12,*end=msg+length;int count=0;
    for(unsigned i=0;i<_getshort(msg+4);++i){int n=dn_skipname(p,end);if(n<0||end-p-n<4)return -1;p+=n+4;}
    for(unsigned i=0;i<_getshort(msg+6);++i){int n=dn_skipname(p,end);if(n<0||end-p-n<10)return -1;p+=n;unsigned type=_getshort(p),bytes=_getshort(p+8);p+=10;if((size_t)(end-p)<bytes)return -1;if(type==T_SOA)++count;p+=bytes;}
    return count;
}
static int list_zone(const char *zone,FILE *out)
{
    unsigned char query[1026];int n=res_mkquery(QUERY,zone,query_class,T_AXFR,NULL,0,NULL,query+2,1024);if(n<0)return 1;putshort((uint16_t)n,query);
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return 1;
    struct timeval tv={_res.retrans,0};int failed=1;
    if(setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv))||setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv))||connect(fd,(struct sockaddr *)&_res.nsaddr,sizeof(_res.nsaddr)))goto done;
    if(send(fd,query,(size_t)n+2,MSG_NOSIGNAL)!=n+2)goto done;
    unsigned char *answer=malloc(65535);if(!answer)goto done;
    unsigned soas=0;
    for(unsigned messages=0;messages<4096&&!interrupted;++messages){
        unsigned char prefix[2];if(readall(fd,prefix,2))break;unsigned length=_getshort(prefix);
        if(length<12||readall(fd,answer,length)||memcmp(answer,query+2,2)||!(answer[2]&0x80)||(answer[3]&15))break;
        int count=soa_count(answer,length);if(count<0||fp_nquery(answer,(int)length,out))break;soas+=(unsigned)count;if(soas>=2){failed=0;break;}
    }
    free(answer);
done:
    close(fd);if(failed&&!interrupted)fputs("nslookup: zone transfer failed\n",stderr);return failed;
}
void ListHosts(const char *line,int redirected)
{
    char text[2048];if(strlen(line)>=sizeof(text)){status=2;return;}strcpy(text,line);FILE *out=output(text,redirected);if(!out){status=1;return;}
    char buffer[2048],*w[8];unsigned n=words(text,buffer,w,8);if(n<2)status=2;else status=list_zone(w[n-1],out);if(out!=stdout)fclose(out);
}
void ListHostsByType(const char *line,int redirected){ListHosts(line,redirected);}
void Finger(const char *line,int redirected){(void)line;(void)redirected;fputs("nslookup: finger is outside the IPv4 resolver client scope\n",stderr);status=2;}
void ViewList(const char *line){(void)line;fputs("nslookup: use the pager to view saved listings\n",stderr);status=2;}
void PrintHelp(void)
{
    FILE *file=fopen("/usr/lib/nslookup.help","r");if(!file){perror("nslookup help");return;}int c;while((c=getc(file))!=EOF)putchar(c);fclose(file);
}
int yywrap(void){return 1;}
int main(int argc,char **argv)
{
    if(res_init())return 1;
    signal(SIGINT,interrupt);int i=1;
    for(;i<argc&&argv[i][0]=='-'&&argv[i][1];++i){SetOption(argv[i]+1);if(status)return status;}
    if(argc-i>2){fputs("usage: nslookup [-option[=value]] [name|- [server]]\n",stderr);return 2;}
    if(argc-i==2&&select_server(argv[i+1],0))return 1;
    initial_server=_res.nsaddr;
    if(i<argc&&strcmp(argv[i],"-"))return lookup(argv[i],stdout);
    show_server(stdout);
    for(;;){
        interrupted=0;
        if(setjmp(prompt_jump)){
            reading_input=0;nslookup_lexer_reset();clearerr(stdin);_res_close();fputc('\n',stdout);continue;
        }
        fputs("> ",stdout);fflush(stdout);int result=yylex();
        if(interrupted){nslookup_lexer_reset();clearerr(stdin);_res_close();fputc('\n',stdout);continue;}
        if(!result)break;
    }
    endhostent();return status;
}
