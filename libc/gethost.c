/*-
 * Copyright (c) 1985, 1988, 1993
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

/* Bounded native gethostbyname/gethostbyaddr after Lite2 gethostnamadr.c. */
#include <netdb.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "netdb_private.h"
#define HOST_LIMIT 35
static struct hostent result;
static char canonical[1025],alias_text[HOST_LIMIT][1025];
static char *aliases[HOST_LIMIT+1],*address_ptrs[HOST_LIMIT+1];
static struct in_addr addresses[HOST_LIMIT];
struct record { char owner[1025]; unsigned type,class,length;const unsigned char *data,*next; };
static int rr(const unsigned char *msg,const unsigned char *end,const unsigned char *p,struct record *r)
{
    int n=dn_expand(msg,end,p,r->owner,sizeof(r->owner));if(n<0)return -1;p+=n;
    if(end-p<10)return -1;
    r->type=_getshort(p);r->class=_getshort(p+2);r->length=_getshort(p+8);p+=10;
    if((size_t)(end-p)<r->length)return -1;
    r->data=p;r->next=p+r->length;return 0;
}
struct hostent *_nx_host_answer(const unsigned char *msg,int length,int reverse)
{
    if(!msg||length<12||_getshort(msg+4)!=1||!(msg[2]&0x80)||(msg[2]&0x7a)||(msg[3]&15))goto malformed;
    const unsigned char *end=msg+length,*p=msg+12;
    int n=dn_expand(msg,end,p,canonical,sizeof(canonical));if(n<0)goto malformed;p+=n;
    if(end-p<4||_getshort(p+2)!=C_IN||_getshort(p)!=(reverse?T_PTR:T_A))goto malformed;
    p+=4;const unsigned char *answers=p;unsigned count=_getshort(msg+6),alias_count=0,address_count=0;
    /* Validate every RR in all sections, including names in supported RDATA,
     * before exposing partial results from otherwise malformed packets. */
    unsigned total=count+_getshort(msg+8)+_getshort(msg+10);
    for(unsigned i=0;i<total;++i){struct record r;if(rr(msg,end,p,&r))goto malformed;
        if(r.class==C_IN&&(r.type==T_CNAME||r.type==T_PTR||r.type==T_NS)){
            char name[1025];n=dn_expand(msg,end,r.data,name,sizeof(name));if(n<0||(unsigned)n!=r.length)goto malformed;
        }
        if(r.class==C_IN&&r.type==T_A&&r.length!=4)goto malformed;
        p=r.next;
    }
    /* Rescan for CNAMEs to handle wire order independently. Bound the chain
     * and reject loops rather than returning attacker-selected unrelated A RRs. */
    for(;;){
        int found=0;p=answers;
        for(unsigned i=0;i<count;++i){struct record r;if(rr(msg,end,p,&r))goto malformed;p=r.next;
            if(r.class!=C_IN||r.type!=T_CNAME||!_nx_dns_equal(r.owner,canonical))continue;
            char target[1025];n=dn_expand(msg,end,r.data,target,sizeof(target));if(n<0||(unsigned)n!=r.length)goto malformed;
            if(_nx_dns_equal(target,canonical)||alias_count==HOST_LIMIT)goto malformed;
            for(unsigned j=0;j<alias_count;++j)if(_nx_dns_equal(target,aliases[j]))goto malformed;
            strcpy(alias_text[alias_count],canonical);aliases[alias_count]=alias_text[alias_count];++alias_count;
            strcpy(canonical,target);found=1;break;
        }
        if(!found)break;
    }
    p=answers;
    for(unsigned i=0;i<count;++i){struct record r;if(rr(msg,end,p,&r))goto malformed;p=r.next;
        if(r.class!=C_IN||!_nx_dns_equal(r.owner,canonical))continue;
        if(reverse&&r.type==T_PTR){
            char target[1025];n=dn_expand(msg,end,r.data,target,sizeof(target));if(n<0||(unsigned)n!=r.length)goto malformed;
            strcpy(canonical,target);address_count=1;break;
        }
        if(!reverse&&r.type==T_A&&address_count<HOST_LIMIT){
            memcpy(&addresses[address_count],r.data,4);address_ptrs[address_count]=(char *)&addresses[address_count];++address_count;
        }
    }
    if(!address_count){h_errno=NO_DATA;errno=0;return NULL;}
    aliases[alias_count]=NULL;address_ptrs[address_count]=NULL;
    if(reverse){address_ptrs[0]=(char *)&addresses[0];address_ptrs[1]=NULL;}
    result=(struct hostent){canonical,aliases,AF_INET,4,address_ptrs};h_errno=0;errno=0;return &result;
malformed:
    h_errno=NO_RECOVERY;errno=EINVAL;return NULL;
}
struct hostent *gethostbyname(const char *name)
{
    if(!name||!*name){h_errno=HOST_NOT_FOUND;errno=EINVAL;return NULL;}
    size_t size=strlen(name);int numeric=name[0]>='0'&&name[0]<='9';
    for(const char *p=name;*p;++p)if((*p<'0'||*p>'9')&&*p!='.')numeric=0;
    if(numeric&&name[size-1]!='.'){
        if(!inet_aton(name,&addresses[0])){h_errno=HOST_NOT_FOUND;errno=0;return NULL;}
        if(size>=sizeof(canonical)){h_errno=NO_RECOVERY;errno=EMSGSIZE;return NULL;}
        strcpy(canonical,name);aliases[0]=NULL;address_ptrs[0]=(char *)&addresses[0];address_ptrs[1]=NULL;
        result=(struct hostent){canonical,aliases,AF_INET,4,address_ptrs};h_errno=0;return &result;
    }
    unsigned char answer[4096];int n=res_search(name,C_IN,T_A,answer,sizeof(answer));
    if(n<0)return errno==ECONNREFUSED?_gethtbyname(name):NULL;
    return _nx_host_answer(answer,n,0);
}
struct hostent *gethostbyaddr(const void *addr,int length,int family)
{
    if(!addr||length!=4||family!=AF_INET){errno=family!=AF_INET?EAFNOSUPPORT:EINVAL;h_errno=NO_RECOVERY;return NULL;}
    struct in_addr saved;memcpy(&saved,addr,4);const unsigned char *bytes=(const unsigned char *)&saved;
    char name[80];snprintf(name,sizeof(name),"%u.%u.%u.%u.in-addr.arpa",bytes[3],bytes[2],bytes[1],bytes[0]);
    unsigned char answer[4096];int n=res_query(name,C_IN,T_PTR,answer,sizeof(answer));
    if(n<0)return errno==ECONNREFUSED?_gethtbyaddr(&saved,4,AF_INET):NULL;
    struct hostent *p=_nx_host_answer(answer,n,1);if(p)addresses[0]=saved;return p;
}
