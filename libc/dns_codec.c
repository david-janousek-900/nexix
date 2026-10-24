/*-
 * Copyright (c) 1985, 1993
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

/* Bounds-checked native adaptation of Lite2 res_comp/res_mkquery. */
#include <resolv.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
struct __res_state _res={.retrans=RES_TIMEOUT,.retry=4,.options=RES_DEFAULT};
uint16_t _getshort(const unsigned char *p) { return (uint16_t)((unsigned)p[0]<<8|p[1]); }
uint32_t _getlong(const unsigned char *p) { return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]; }
void putshort(uint16_t v,unsigned char *p) { p[0]=(unsigned char)(v>>8);p[1]=(unsigned char)v; }
void putlong(uint32_t v,unsigned char *p) { p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v; }
/* Expand into wire labels. Every pointer strictly decreases its location;
 * the step bound also rejects cycles crossing ordinary labels. */
static int wire(const unsigned char *msg,const unsigned char *end,const unsigned char *src,unsigned char out[255],unsigned *size)
{
    if(!msg||!end||!src||src<msg||src>=end) return -1;
    const unsigned char *p=src; unsigned n=0,steps=0;int consumed=-1;
    while(p<end && ++steps<=255) {
        const unsigned char *label=p;unsigned len=*p++;
        if((len&0xc0)==0xc0) {
            if(p==end) return -1;
            unsigned offset=((len&0x3f)<<8)|*p++;
            if((size_t)offset>=(size_t)(label-msg)) return -1;
            if(consumed<0) consumed=(int)(p-src);
            p=msg+offset;continue;
        }
        if(len&0xc0 || len>63 || (size_t)(end-p)<len || n+1+len>255) return -1;
        out[n++]=(unsigned char)len;
        if(!len) { *size=n;return consumed<0?(int)(p-src):consumed; }
        memcpy(out+n,p,len);n+=len;p+=len;
    }
    return -1;
}
int dn_expand(const unsigned char *msg,const unsigned char *end,const unsigned char *src,char *dst,int capacity)
{
    unsigned char labels[255];unsigned n;
    if(!dst||capacity<=0) return -1;
    int used=wire(msg,end,src,labels,&n);if(used<0) return -1;
    unsigned pos=0,w=0;
    while(pos<n && labels[pos]) {
        unsigned len=labels[pos++];
        if(w) {if(w+1>=(unsigned)capacity)return -1;dst[w++]='.';}
        for(unsigned i=0;i<len;++i) {
            unsigned c=labels[pos++];
            if(c=='.'||c=='\\') {if(w+1>=(unsigned)capacity)return -1;dst[w++]='\\';}
            if(c<=32||c>=127) {
                if(w+4>=(unsigned)capacity)return -1;
                dst[w++]='\\';dst[w++]=(char)('0'+c/100);dst[w++]=(char)('0'+c/10%10);dst[w++]=(char)('0'+c%10);
            } else {if(w+1>=(unsigned)capacity)return -1;dst[w++]=(char)c;}
        }
    }
    dst[w]=0;return used;
}
int dn_skipname(const unsigned char *src,const unsigned char *end)
{
    const unsigned char *p=src;unsigned total=0;
    if(!src||!end||src>=end)return -1;
    while(p<end) {
        unsigned n=*p++;
        if((n&0xc0)==0xc0) return p<end?(int)(p+1-src):-1;
        if(n&0xc0 || (size_t)(end-p)<n || total+1+n>255)return -1;
        total+=1+n;if(!n)return (int)(p-src);p+=n;
    }
    return -1;
}
static int encode(const char *s,unsigned char out[255])
{
    if(!s)return -1;
    if(!*s || (*s=='.'&&!s[1])) {out[0]=0;return 1;}
    unsigned n=1,label=0;
    for(;;) {
        if(!*s || *s=='.') {
            unsigned len=n-label-1;if(!len||len>63)return -1;
            out[label]=(unsigned char)len;
            if(!*s || !s[1]) {if(n>=255)return -1;out[n++]=0;return (int)n;}
            if(n>=254)return -1;
            label=n++;++s;continue;
        }
        unsigned c=(unsigned char)*s++;
        if(c=='\\') {
            if(!*s)return -1;
            c=(unsigned char)*s++;
            if(c>='0'&&c<='9') {
                if(s[0]<'0'||s[0]>'9'||!s[1]||s[1]<'0'||s[1]>'9')return -1;
                c=(c-'0')*100+(unsigned)(s[0]-'0')*10+(unsigned)(s[1]-'0');s+=2;
                if(c>255)return -1;
            }
        }
        if(n>=254||n-label>63)return -1;
        out[n++]=(unsigned char)c;
    }
}
int dn_comp(const char *src,unsigned char *dst,int capacity,unsigned char **pointers,unsigned char **last)
{
    unsigned char labels[255];int size=encode(src,labels);
    if(size<0||!dst||capacity<0)return -1;
    unsigned n=0,w=0;unsigned char **save=NULL,**slot=NULL,*base=NULL;
    if(pointers&&last&&pointers<last&&*pointers) {
        base=*pointers;slot=pointers+1;
        while(slot<last&&*slot)++slot;
        if(slot==last)return -1;
        save=slot;
    }
    while(n<(unsigned)size) {
        if(base && labels[n]) {
            for(unsigned char **p=pointers+1;p<save;++p) {
                unsigned char expanded[255];unsigned length;
                if(*p<base||*p>=dst||*p-base>=0x4000)continue;
                if(wire(base,dst,*p,expanded,&length)>=0 && length==(unsigned)size-n && !memcmp(expanded,labels+n,length)) {
                    if(w+2>(unsigned)capacity)goto fail;
                    unsigned off=(unsigned)(*p-base);dst[w++]=(unsigned char)(0xc0|(off>>8));dst[w++]=(unsigned char)off;
                    return (int)w;
                }
            }
            if(slot+1<last && dst+w>=base && dst+w-base<0x4000) {*slot++=dst+w;*slot=NULL;}
        }
        unsigned chunk=1+labels[n];
        if(w+chunk>(unsigned)capacity)goto fail;
        memcpy(dst+w,labels+n,chunk);w+=chunk;n+=chunk;
    }
    return (int)w;
fail:
    if(save)*save=NULL;
    return -1;
}
int res_mkquery(int op,const char *name,int class,int type,const unsigned char *data,int datalen,const unsigned char *newrr,unsigned char *buf,int capacity)
{
    (void)newrr;
    if((op!=QUERY&&op!=IQUERY)||!buf||capacity<HFIXEDSZ||class<0||class>65535||type<0||type>65535) return -1;
    memset(buf,0,HFIXEDSZ); putshort(++_res.id,buf);
    buf[2]=(unsigned char)((op<<3)|((_res.options&RES_RECURSE)?1:0));
    int n=HFIXEDSZ;
    if(op==QUERY) {
        int length=dn_comp(name,buf+n,capacity-n,NULL,NULL);
        if(length<0||capacity-n-length<4)return -1;
        n+=length;putshort((uint16_t)type,buf+n);putshort((uint16_t)class,buf+n+2);n+=4;putshort(1,buf+4);
    } else if(op==IQUERY) {
        if(datalen<0||datalen>65535||capacity-n<11||capacity-n-11<datalen||(!data&&datalen))return -1;
        buf[n++]=0;putshort((uint16_t)type,buf+n);putshort((uint16_t)class,buf+n+2);putlong(0,buf+n+4);putshort((uint16_t)datalen,buf+n+8);n+=10;
        if(datalen)memcpy(buf+n,data,(size_t)datalen);
        n+=datalen;putshort(1,buf+6);
    } else return -1;
    return n;
}
const char *p_type(int type)
{
    switch(type) {case T_A:return "A";case T_NS:return "NS";case T_CNAME:return "CNAME";case T_SOA:return "SOA";case T_PTR:return "PTR";case T_MX:return "MX";case T_TXT:return "TXT";case T_AXFR:return "AXFR";case T_ANY:return "ANY";default:return "UNKNOWN";}
}
const char *p_class(int class)
{ return class==C_IN?"IN":class==C_ANY?"ANY":class==C_CHAOS?"CHAOS":class==C_HS?"HS":"UNKNOWN"; }
/* Always use the length-bearing entry point on received messages. */
int fp_nquery(const unsigned char *msg,int length,FILE *file)
{
    if(!msg||length<12||!file)return -1;
    const unsigned char *p=msg+12,*end=msg+length;
    fprintf(file,";; id: %u opcode: %u rcode: %u flags: 0x%02x%02x\n",_getshort(msg),(msg[2]>>3)&15,msg[3]&15,msg[2],msg[3]);
    for(unsigned section=0;section<4;++section) {
        unsigned count=_getshort(msg+4+2*section);
        for(unsigned i=0;i<count;++i) {
            char name[1025];int n=dn_expand(msg,end,p,name,sizeof(name));if(n<0)return -1;p+=n;
            if(end-p<(section?10:4))return -1;
            unsigned type=_getshort(p),class=_getshort(p+2);p+=4;
            fprintf(file,"%s\t%s\t%s",*name?name:".",p_class((int)class),p_type((int)type));
            if(section) {
                uint32_t ttl=_getlong(p);unsigned bytes=_getshort(p+4);p+=6;if((size_t)(end-p)<bytes)return -1;
                fprintf(file,"\t%u\t",ttl);
                if(type==T_A&&bytes==4)fprintf(file,"%u.%u.%u.%u",p[0],p[1],p[2],p[3]);
                else if(type==T_CNAME||type==T_NS||type==T_PTR) {n=dn_expand(msg,end,p,name,sizeof(name));if(n<0||(unsigned)n!=bytes)return -1;fputs(name,file);}
                else fprintf(file,"[%u bytes]",bytes);
                p+=bytes;
            }
            fputc('\n',file);
        }
    }
    return 0;
}
void fp_query(const unsigned char *msg,FILE *file)
{
    /* Historic fp_query has no length parameter. Its only safe native use is
     * a local QUERY (one question and no RRs), never an untrusted response. */
    if(!msg||!file)return;
    if(_getshort(msg+4)!=1||_getshort(msg+6)||_getshort(msg+8)||_getshort(msg+10))return;
    int n=dn_skipname(msg+12,msg+12+255);
    if(n>=0)(void)fp_nquery(msg,12+n+4,file);
}
