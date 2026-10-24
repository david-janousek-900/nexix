/*-
 * Copyright (c) 1985, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

/* Native blocking transport following Lite2 res_send and res_query. */
#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/times.h>
#include "netdb_private.h"
static int stream=-1;
static struct sockaddr_in stream_server;
static pid_t stream_owner;
void _res_close(void) { if(stream>=0)close(stream);stream=-1; }
static uint64_t ticks(void)
{ struct tms t;clock_t value=times(&t);return value<0?0:(uint64_t)value; }
static int timeout(int fd,uint64_t deadline,int option)
{
    uint64_t now=ticks();if(now>=deadline){errno=ETIMEDOUT;return -1;}
    uint64_t left=deadline-now;
    struct timeval tv={(time_t)(left/100),(long)((left%100)*10000)};
    return setsockopt(fd,SOL_SOCKET,option,&tv,sizeof(tv));
}
static int same_server(const struct sockaddr_in *a,const struct sockaddr_in *b)
{ return a->sin_family==AF_INET&&b->sin_family==AF_INET&&a->sin_port==b->sin_port&&a->sin_addr.s_addr==b->sin_addr.s_addr; }
static int question(const unsigned char *q,int qlen,const unsigned char *a,int alen)
{
    if(qlen<12||alen<12||!(a[2]&0x80)||(q[2]&0x78)!=(a[2]&0x78)||memcmp(q,a,2)||_getshort(q+4)!=1||_getshort(a+4)!=1)return 0;
    char qname[1025],aname[1025];
    int qn=dn_expand(q,q+qlen,q+12,qname,sizeof(qname));
    int an=dn_expand(a,a+alen,a+12,aname,sizeof(aname));
    if(qn<0||an<0||qlen-12-qn<4||alen-12-an<4)return 0;
    return _nx_dns_equal(qname,aname)&&!memcmp(q+12+qn,a+12+an,4);
}
static int exact(int fd,unsigned char *buf,size_t size,uint64_t deadline)
{
    size_t n=0;
    while(n<size) {
        if(timeout(fd,deadline,SO_RCVTIMEO))return -1;
        ssize_t got=recv(fd,buf+n,size-n,0);
        if(got<=0){if(!got)errno=ECONNRESET;return -1;}n+=(size_t)got;
    }
    return 0;
}
static int tcp(const struct sockaddr_in *server,const unsigned char *query,int length,unsigned char *answer,int capacity,uint64_t deadline)
{
    unsigned char *frame=malloc((size_t)length+2);if(!frame){errno=ENOMEM;return -1;}
    putshort((uint16_t)length,frame);memcpy(frame+2,query,(size_t)length);
    int result=-1;
    for(unsigned reset=0;reset<2;++reset) {
        if(stream>=0&&(!same_server(&stream_server,server)||stream_owner!=getpid()))_res_close();
        if(stream<0) {
            stream=socket(AF_INET,SOCK_STREAM,0);if(stream<0)break;
            stream_server=*server;stream_owner=getpid();
            if(timeout(stream,deadline,SO_SNDTIMEO)||connect(stream,(const struct sockaddr *)server,sizeof(*server)))goto failure;
        }
        size_t sent=0;
        while(sent<(size_t)length+2) {
            if(timeout(stream,deadline,SO_SNDTIMEO))goto failure;
            ssize_t n=send(stream,frame+sent,(size_t)length+2-sent,MSG_NOSIGNAL);
            if(n<=0){if(!n)errno=ECONNRESET;goto failure;}sent+=(size_t)n;
        }
        for(;;) {
            unsigned char prefix[2],discard[256];
            if(exact(stream,prefix,2,deadline))goto failure;
            unsigned total=_getshort(prefix),kept=total<(unsigned)capacity?total:(unsigned)capacity;
            if(exact(stream,answer,kept,deadline))goto failure;
            unsigned left=total-kept;
            while(left){unsigned n=left<sizeof(discard)?left:sizeof(discard);if(exact(stream,discard,n,deadline))goto failure;left-=n;}
            if(!question(query,length,answer,(int)kept))continue;
            if(total>kept)answer[2]|=2;
            result=(int)kept;goto done;
        }
failure: {
            int error=errno;_res_close();errno=error;
            if(error!=ECONNRESET||reset)break;
        }
    }
done:
    free(frame);
    if(!(_res.options&RES_STAYOPEN)||result<0)_res_close();
    return result;
}
int res_send(const unsigned char *query,int length,unsigned char *answer,int capacity)
{
    if(!query||!answer||length<12||length>65535||capacity<12){errno=EINVAL;return -1;}
    if(!(_res.options&RES_INIT)&&res_init())return -1;
    if(_res.nscount<1||_res.nscount>MAXNS||_res.retrans<1||_res.retrans>3600||_res.retry<1||_res.retry>10){errno=EINVAL;return -1;}
    unsigned bad=0;int last=ETIMEDOUT,refused=0,other=0;
    for(int attempt=0;attempt<_res.retry;++attempt)for(int ns=0;ns<_res.nscount;++ns) {
        if(bad&(1u<<ns))continue;
        unsigned seconds=(unsigned)_res.retrans<<attempt;
        if(attempt)seconds/=(unsigned)_res.nscount;
        if(!seconds)seconds=1;
        uint64_t deadline=ticks()+(uint64_t)seconds*100;
        const struct sockaddr_in *server=&_res.nsaddr_list[ns];
        int n=-1;
        if((_res.options&RES_USEVC)||length>PACKETSZ) n=tcp(server,query,length,answer,capacity,deadline);
        else {
            int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -1;
            int connected=_res.nscount==1;
            if(timeout(fd,deadline,SO_SNDTIMEO)|| (connected&&connect(fd,(const struct sockaddr *)server,sizeof(*server)))) {last=errno;close(fd);goto failed;}
            ssize_t sent=connected?send(fd,query,(size_t)length,0):sendto(fd,query,(size_t)length,0,(const struct sockaddr *)server,sizeof(*server));
            if(sent!=length){last=errno;close(fd);goto failed;}
            for(;;) {
                if(timeout(fd,deadline,SO_RCVTIMEO)){n=-1;break;}
                struct sockaddr_in from; socklen_t size=sizeof(from);
                n=(int)recvfrom(fd,answer,(size_t)capacity,0,(struct sockaddr *)&from,&size);
                if(n<0)break;
                if(size!=sizeof(from)||!same_server(&from,server)||!question(query,length,answer,n))continue;
                break;
            }
            last=errno;close(fd);
            if(n<0)goto failed;
            if((answer[2]&2)&&!(_res.options&RES_IGNTC))n=tcp(server,query,length,answer,capacity,deadline);
        }
        if(n<0){last=errno;goto failed;}
        if(_res.options&RES_DEBUG)(void)fp_nquery(answer,n,stderr);
        unsigned rcode=answer[3]&15;
        if(rcode==SERVFAIL||rcode==NOTIMP||rcode==REFUSED){bad|=1u<<ns;last=rcode==SERVFAIL?EAGAIN:EIO;other=1;continue;}
        errno=0;return n;
failed:
        if(last==EINTR){_res_close();errno=EINTR;return -1;}
        if(last==ECONNREFUSED)refused=1;else other=1;
    }
    _res_close();errno=refused&&!other?ECONNREFUSED:last==ECONNREFUSED?ETIMEDOUT:last;return -1;
}
int res_query(const char *name,int class,int type,unsigned char *answer,int capacity)
{
    if(!name||!answer||capacity<12){errno=EINVAL;h_errno=NO_RECOVERY;return -1;}
    if(!(_res.options&RES_INIT)&&res_init()){h_errno=NO_RECOVERY;return -1;}
    unsigned char query[1024];int n=res_mkquery(QUERY,name,class,type,NULL,0,NULL,query,sizeof(query));
    if(n<0){h_errno=NO_RECOVERY;return -1;}
    if(_res.options&RES_DEBUG)(void)fp_nquery(query,n,stderr);
    n=res_send(query,n,answer,capacity);
    if(n<0){h_errno=(errno==EIO||errno==EINVAL)?NO_RECOVERY:TRY_AGAIN;return -1;}
    unsigned rcode=answer[3]&15;
    if(rcode||!_getshort(answer+6)) {
        h_errno=rcode==NXDOMAIN?HOST_NOT_FOUND:rcode==SERVFAIL?TRY_AGAIN:rcode==NOERROR?NO_DATA:NO_RECOVERY;
        errno=0;return -1;
    }
    h_errno=0;return n;
}
char *hostalias(const char *name)
{
    const char *path=getenv("HOSTALIASES");if(!path||!name)return NULL;
    FILE *file=fopen(path,"r");if(!file)return NULL;
    static char alias[MAXDNAME];char line[1024];int state;char *result=NULL;
    while((state=_nx_dbline(file,line,sizeof(line)))) {
        if(state<0)continue;
        char *words[2];unsigned n=_nx_dbwords(line,words,2);
        if(n==2&&_nx_dns_equal(name,words[0])&&strlen(words[1])<sizeof(alias)) {strcpy(alias,words[1]);result=alias;break;}
    }
    fclose(file);return result;
}
int res_querydomain(const char *name,const char *domain,int class,int type,unsigned char *answer,int capacity)
{
    if(!name){errno=EINVAL;h_errno=NO_RECOVERY;return -1;}
    char full[2*MAXDNAME+2];size_t n=strlen(name);
    if(n>=MAXDNAME){errno=EMSGSIZE;h_errno=NO_RECOVERY;return -1;}
    memcpy(full,name,n+1);
    if(domain&&*domain) {
        size_t d=strlen(domain);if(d>=MAXDNAME||n+d+2>sizeof(full)){errno=EMSGSIZE;h_errno=NO_RECOVERY;return -1;}
        full[n++]='.';memcpy(full+n,domain,d+1);
    } else if(n&&full[n-1]=='.')full[n-1]=0;
    return res_query(full,class,type,answer,capacity);
}
int res_search(const char *name,int class,int type,unsigned char *answer,int capacity)
{
    if(!name||!*name){errno=EINVAL;h_errno=HOST_NOT_FOUND;return -1;}
    if(!(_res.options&RES_INIT)&&res_init())return -1;
    errno=0;h_errno=HOST_NOT_FOUND;unsigned dots=0;size_t length=strlen(name);
    for(const char *p=name;*p;++p)if(*p=='.')++dots;
    if(!dots){char *alias=hostalias(name);if(alias)return res_query(alias,class,type,answer,capacity);}
    int nodata=0;
    if((!dots&&(_res.options&RES_DEFNAMES))||(dots&&name[length-1]!='.'&&(_res.options&RES_DNSRCH))) {
        for(unsigned i=0;i<MAXDNSRCH&&_res.dnsrch[i];++i) {
            int n=res_querydomain(name,_res.dnsrch[i],class,type,answer,capacity);if(n>0)return n;
            if(errno==ECONNREFUSED)return -1;
            if(errno==EINTR)return -1;
            if(h_errno==NO_DATA)nodata=1;
            if((h_errno!=HOST_NOT_FOUND&&h_errno!=NO_DATA)||!(_res.options&RES_DNSRCH))break;
        }
    }
    if(dots){int n=res_querydomain(name,NULL,class,type,answer,capacity);if(n>0)return n;}
    if(nodata)h_errno=NO_DATA;
    return -1;
}
