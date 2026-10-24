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

/* Native bounded /etc/hosts adapter. DNS lookup is layered on in task 451. */
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include "netdb_private.h"
static FILE *hostfile;
static int stayopen;
static char line[1024], *aliases[36], *addresses[2];
static struct in_addr address;
static struct hostent host;
void _sethtent(int stay)
{
    if(hostfile) rewind(hostfile); else hostfile=fopen(_PATH_HOSTS,"r");
    stayopen |= stay;
}
void _endhtent(void)
{ if(hostfile && !stayopen) {fclose(hostfile);hostfile=NULL;} }
void sethostent(int stay) { _sethtent(stay); }
void endhostent(void) { stayopen=0; _endhtent(); }
struct hostent *_gethtent(void)
{
    if(!hostfile && !(hostfile=fopen(_PATH_HOSTS,"r"))) return NULL;
    int state;
    while((state=_nx_dbline(hostfile,line,sizeof(line)))) {
        if(state<0) continue;
        char *fields[37]; unsigned n=_nx_dbwords(line,fields,37);
        if(n<2 || n>37 || !inet_aton(fields[0],&address)) continue;
        for(unsigned i=2;i<n;++i) aliases[i-2]=fields[i];
        aliases[n-2]=NULL; addresses[0]=(char *)&address;addresses[1]=NULL;
        host=(struct hostent){fields[1],aliases,AF_INET,4,addresses};
        return &host;
    }
    return NULL;
}
struct hostent *gethostent(void) { return _gethtent(); }
static unsigned lower(unsigned c) { return c>='A'&&c<='Z'?c+32:c; }
int _nx_dns_equal(const char *a,const char *b)
{
    while(*a && *b) if(lower((unsigned char)*a++)!=lower((unsigned char)*b++)) return 0;
    return *a==*b;
}
struct hostent *_gethtbyname(const char *name)
{
    if(!name) {errno=EINVAL;h_errno=NO_RECOVERY;return NULL;}
    _sethtent(0); struct hostent *p;
    while((p=_gethtent())) {
        if(_nx_dns_equal(name,p->h_name)) break;
        char **a=p->h_aliases;
        while(*a && !_nx_dns_equal(name,*a)) ++a;
        if(*a) break;
    }
    _endhtent(); h_errno=p?0:HOST_NOT_FOUND; return p;
}
struct hostent *_gethtbyaddr(const void *addr,int length,int family)
{
    if(!addr || length!=4 || family!=AF_INET) {
        errno=family!=AF_INET?EAFNOSUPPORT:EINVAL;h_errno=NO_RECOVERY;return NULL;
    }
    _sethtent(0); struct hostent *p;
    while((p=_gethtent()) && memcmp(addr,p->h_addr,4)) {}
    _endhtent(); h_errno=p?0:HOST_NOT_FOUND;return p;
}
