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

/* Bounded Lite2 resolver configuration on the native libc. */
#include <resolv.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "netdb_private.h"
static char search[MAXDNSRCH][MAXDNAME];
static int domain(const char *text,char *out)
{
    size_t n=strlen(text);
    if(!n||n>=MAXDNAME)return 0;
    memcpy(out,text,n+1);return 1;
}
static unsigned search_list(char **words,unsigned count)
{
    unsigned n=0;
    for(unsigned i=0;i<count&&n<MAXDNSRCH;++i)if(domain(words[i],search[n]))++n;
    return n;
}
int res_init(void)
{
    if(!_res.retrans)_res.retrans=RES_TIMEOUT;
    if(!_res.retry)_res.retry=4;
    _res.nscount=0;_res.pfcode=0;_res.defdname[0]=0;
    memset(_res.nsaddr_list,0,sizeof(_res.nsaddr_list));
    memset(_res.dnsrch,0,sizeof(_res.dnsrch));
    unsigned searches=0;int explicit_search=0;
    const char *env=getenv("LOCALDOMAIN");int haveenv=env!=NULL;
    if(env) {
        size_t n=0;while(env[n]&&env[n]!=' '&&env[n]!='\t'&&env[n]!='\n') {
            if(n+1>=sizeof(_res.defdname)){n=0;break;} _res.defdname[n]=env[n];++n;
        }
        _res.defdname[n]=0;
    }
    FILE *file=fopen(_PATH_RESCONF,"r");
    if(file) {
        char line[1024];int state;
        while((state=_nx_dbline(file,line,sizeof(line)))) {
            if(state<0)continue;
            char *comment=strchr(line,';');if(comment)*comment=0;
            char *words[64];unsigned n=_nx_dbwords(line,words,64);if(n<2||n>64)continue;
            if(!strcmp(words[0],"nameserver")&&n==2&&_res.nscount<MAXNS) {
                struct in_addr a;uint32_t v;
                if(!inet_aton(words[1],&a)||(v=ntohl(a.s_addr))==0||v>=0xe0000000)continue;
                _res.nsaddr_list[_res.nscount++]=(struct sockaddr_in){AF_INET,htons(NAMESERVER_PORT),a,{0}};
            } else if(!haveenv&&!strcmp(words[0],"domain")&&domain(words[1],_res.defdname)) {
                explicit_search=0;searches=0;
            } else if(!haveenv&&!strcmp(words[0],"search")) {
                searches=search_list(words+1,n-1);explicit_search=searches!=0;
                if(searches)domain(search[0],_res.defdname);
            }
        }
        fclose(file);
    }
    if(!_res.nscount) {
        _res.nsaddr_list[0]=(struct sockaddr_in){AF_INET,htons(NAMESERVER_PORT),{htonl(INADDR_LOOPBACK)},{0}};
        _res.nscount=1;
    }
    if(!_res.defdname[0]) {
        char hostname[256];
        if(!gethostname(hostname,sizeof(hostname))) {
            hostname[sizeof(hostname)-1]=0;char *dot=strchr(hostname,'.');
            if(dot&&dot[1])domain(dot+1,_res.defdname);
        }
    }
    if(explicit_search)for(unsigned i=0;i<searches;++i)_res.dnsrch[i]=search[i];
    else {
        unsigned n=0,dots=0;char *p=_res.defdname;
        for(char *q=p;*q;++q)if(*q=='.')++dots;
        _res.dnsrch[n++]=p;
        while(dots>=LOCALDOMAINPARTS&&n<MAXDFLSRCH) {
            p=strchr(p,'.')+1;_res.dnsrch[n++]=p;--dots;
        }
    }
    _res.options|=RES_INIT;return 0;
}
