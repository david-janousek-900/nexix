/*
 * Copyright (c) 1983, 1993
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
 */

/* Native bounded adapters for Lite2 get{serv,proto,net}ent and lookups.
 * Separate static storage for each database; results live until its next call. */
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "netdb_private.h"
int h_errno;
char *hstrerror(int error)
{
    static char *messages[] = {"Resolver Error 0 (no error)", "Unknown host",
        "Host name lookup failure", "Unknown server error", "No address associated with name"};
    return error >= 0 && error <= NO_DATA ? messages[error] : "Unknown resolver error";
}
void herror(const char *prefix)
{
    if (prefix && *prefix) { (void)write(2,prefix,strlen(prefix)); (void)write(2,": ",2); }
    const char *message=hstrerror(h_errno);
    (void)write(2,message,strlen(message)); (void)write(2,"\n",1);
}
/* Consume the whole physical line, including on overflow. Never interpret
 * a suffix of an overlong or embedded-NUL line as a new record. */
int _nx_dbline(FILE *file, char *line, size_t capacity)
{
    int c, bad=0; size_t n=0;
    while ((c=getc(file))!=EOF && c!='\n') {
        if (!c) bad=1;
        if (n+1<capacity) line[n++]=(char)c; else bad=1;
    }
    if (ferror(file) || (c==EOF && !n && !bad)) return 0;
    line[n]=0;
    if (bad) { errno=ERANGE; return -1; }
    char *comment=strchr(line,'#'); if(comment) *comment=0;
    return 1;
}
unsigned _nx_dbwords(char *line, char **words, unsigned capacity)
{
    unsigned n=0;
    while (*line) {
        while (*line==' ' || *line=='\t' || *line=='\r') ++line;
        if (!*line) break;
        if (n==capacity) return capacity+1;
        words[n++]=line;
        while (*line && *line!=' ' && *line!='\t' && *line!='\r') ++line;
        if (*line) *line++=0;
    }
    return n;
}
static int number(const char *s, unsigned maximum, int *out)
{
    unsigned value=0;
    if (!*s) return 0;
    for (;*s;++s) {
        if (*s<'0'||*s>'9'||value>(maximum-(unsigned)(*s-'0'))/10) return 0;
        value=value*10+(unsigned)(*s-'0');
    }
    *out=(int)value; return 1;
}
struct database { FILE *file; int stay; char line[1024]; char *aliases[36]; };
static struct database services, protocols, networks;
static struct servent service;
static struct protoent protocol;
static struct netent network;
static void reset(struct database *db,const char *path,int stay)
{
    if(db->file) rewind(db->file); else db->file=fopen(path,"r");
    db->stay |= stay;
}
static void finish(struct database *db)
{ if(db->file) fclose(db->file); db->file=NULL; db->stay=0; }
void setservent(int stay) { reset(&services,_PATH_SERVICES,stay); }
void endservent(void) { finish(&services); }
void setprotoent(int stay) { reset(&protocols,_PATH_PROTOCOLS,stay); }
void endprotoent(void) { finish(&protocols); }
void setnetent(int stay) { reset(&networks,_PATH_NETWORKS,stay); }
void endnetent(void) { finish(&networks); }
static unsigned record(struct database *db,const char *path,char **fields)
{
    if(!db->file && !(db->file=fopen(path,"r"))) return 0;
    int state;
    while((state=_nx_dbline(db->file,db->line,sizeof(db->line)))) {
        if(state<0) continue;
        unsigned n=_nx_dbwords(db->line,fields,37);
        if(n<2 || n>37) continue;
        for(unsigned i=2;i<n;++i) db->aliases[i-2]=fields[i];
        db->aliases[n-2]=NULL;
        return n;
    }
    return 0;
}
struct servent *getservent(void)
{
    char *f[37];
    while(record(&services,_PATH_SERVICES,f)) {
        char *slash=strchr(f[1],'/'); int port;
        if(!slash) slash=strchr(f[1],',');
        if(!slash) continue;
        *slash++=0;
        if(!*slash || !number(f[1],65535,&port)) continue;
        service=(struct servent){f[0],services.aliases,htons((uint16_t)port),slash};
        return &service;
    }
    return NULL;
}
struct protoent *getprotoent(void)
{
    char *f[37]; int value;
    while(record(&protocols,_PATH_PROTOCOLS,f)) {
        if(!number(f[1],255,&value)) continue;
        protocol=(struct protoent){f[0],protocols.aliases,value}; return &protocol;
    }
    return NULL;
}
struct netent *getnetent(void)
{
    char *f[37];
    while(record(&networks,_PATH_NETWORKS,f)) {
        uint32_t value=inet_network(f[1]); if(value==INADDR_NONE) continue;
        network=(struct netent){f[0],networks.aliases,AF_INET,value}; return &network;
    }
    return NULL;
}
static int named(const char *name,const char *canonical,char **aliases)
{
    if(!strcmp(name,canonical)) return 1;
    for(;*aliases;++aliases) if(!strcmp(name,*aliases)) return 1;
    return 0;
}
struct servent *getservbyname(const char *name,const char *proto)
{
    if(!name) {errno=EINVAL;return NULL;}
    setservent(0); struct servent *p;
    while((p=getservent())) if((!proto||!strcmp(proto,p->s_proto))&&named(name,p->s_name,p->s_aliases)) break;
    if(!services.stay) finish(&services);
    return p;
}
struct servent *getservbyport(int port,const char *proto)
{
    setservent(0); struct servent *p;
    while((p=getservent())) if(p->s_port==port&&(!proto||!strcmp(proto,p->s_proto))) break;
    if(!services.stay) finish(&services);
    return p;
}
struct protoent *getprotobyname(const char *name)
{
    if(!name) {errno=EINVAL;return NULL;}
    setprotoent(0); struct protoent *p;
    while((p=getprotoent())) if(named(name,p->p_name,p->p_aliases)) break;
    if(!protocols.stay) finish(&protocols);
    return p;
}
struct protoent *getprotobynumber(int value)
{
    setprotoent(0); struct protoent *p;
    while((p=getprotoent())&&p->p_proto!=value) {}
    if(!protocols.stay) finish(&protocols);
    return p;
}
struct netent *getnetbyname(const char *name)
{
    if(!name) {errno=EINVAL;return NULL;}
    setnetent(0); struct netent *p;
    while((p=getnetent())) if(named(name,p->n_name,p->n_aliases)) break;
    if(!networks.stay) finish(&networks);
    return p;
}
struct netent *getnetbyaddr(uint32_t value,int family)
{
    if(family!=AF_INET) {errno=EAFNOSUPPORT;return NULL;}
    setnetent(0); struct netent *p;
    while((p=getnetent())&&p->n_net!=value) {}
    if(!networks.stay) finish(&networks);
    return p;
}
