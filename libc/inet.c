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

/* Byte order and numeric IPv4 conversions (task 424), after 4.4BSD
 * inet_addr.c, inet_ntoa.c and inet_pton.c/inet_ntop.c's IPv4 paths. */
#include <arpa/inet.h>
#include <errno.h>
uint32_t htonl(uint32_t value) { return __builtin_bswap32(value); }
uint16_t htons(uint16_t value) { return __builtin_bswap16(value); }
uint32_t ntohl(uint32_t value) { return __builtin_bswap32(value); }
uint16_t ntohs(uint16_t value) { return __builtin_bswap16(value); }

/* BSD inet_aton: one to four parts, each decimal, octal (leading 0) or hex
 * (0x); the last part fills the remaining bytes. Trailing whitespace ends
 * the address. */
int inet_aton(const char *s, struct in_addr *out)
{
    uint32_t parts[4];
    unsigned count = 0;
    for (;;) {
        unsigned base = 10;
        uint64_t value = 0;
        int digits = 0;
        if (*s < '0' || *s > '9') return 0;
        if (*s == '0') {
            ++s; base = 8; digits = 1;
            if (*s == 'x' || *s == 'X') { ++s; base = 16; digits = 0; }
        }
        for (;; ++s) {
            unsigned d;
            if (*s >= '0' && *s <= '9') d = (unsigned)(*s - '0');
            else if (base == 16 && *s >= 'a' && *s <= 'f') d = (unsigned)(*s - 'a' + 10);
            else if (base == 16 && *s >= 'A' && *s <= 'F') d = (unsigned)(*s - 'A' + 10);
            else break;
            if (d >= base) return 0;
            value = value * base + d;
            if (value > UINT32_MAX) return 0;
            ++digits;
        }
        if (!digits) return 0;
        if (count == 4) return 0;
        parts[count++] = (uint32_t)value;
        if (*s != '.') break;
        ++s;
    }
    if (*s && *s != ' ' && *s != '\t' && *s != '\n') return 0;
    uint32_t address = parts[count - 1];
    static const uint32_t limit[] = {UINT32_MAX, 0xffffff, 0xffff, 0xff};
    if (address > limit[count - 1]) return 0;
    for (unsigned i = 0; i + 1 < count; ++i) {
        if (parts[i] > 255) return 0;
        address |= parts[i] << (24 - 8 * i);
    }
    if (out) out->s_addr = htonl(address);
    return 1;
}
in_addr_t inet_addr(const char *s)
{
    struct in_addr a;
    return inet_aton(s, &a) ? a.s_addr : INADDR_NONE;
}

/* Strict dotted decimal: four parts of 0-255 without leading zeros. */
static int pton4(const char *s, unsigned char out[4])
{
    unsigned char bytes[4];
    unsigned count = 0;
    while (count < 4) {
        unsigned value = 0, digits = 0;
        while (*s >= '0' && *s <= '9') {
            if (digits && !value) return 0;   /* leading zero */
            value = value * 10 + (unsigned)(*s++ - '0');
            if (value > 255) return 0;
            ++digits;
        }
        if (!digits) return 0;
        bytes[count++] = (unsigned char)value;
        if (count < 4 && *s++ != '.') return 0;
    }
    if (*s) return 0;
    for (unsigned i = 0; i < 4; ++i) out[i] = bytes[i];
    return 1;
}
int inet_pton(int family, const char *source, void *destination)
{
    if (family != AF_INET) { errno = EAFNOSUPPORT; return -1; }
    return pton4(source, destination);
}
const char *inet_ntop(int family, const void *source, char *destination, socklen_t size)
{
    if (family != AF_INET) { errno = EAFNOSUPPORT; return 0; }
    const unsigned char *b = source;
    char text[INET_ADDRSTRLEN];
    unsigned n = 0;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned v = b[i];
        if (v >= 100) text[n++] = (char)('0' + v / 100);
        if (v >= 10) text[n++] = (char)('0' + v / 10 % 10);
        text[n++] = (char)('0' + v % 10);
        text[n++] = i < 3 ? '.' : '\0';
    }
    if (size < n) { errno = ENOSPC; return 0; }
    for (unsigned i = 0; i < n; ++i) destination[i] = text[i];
    return destination;
}
char *inet_ntoa(struct in_addr address)
{
    static char text[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.s_addr, text, sizeof(text));
    return text;
}

/* Classful network helpers following Lite2 inet_{network,makeaddr,lnaof,netof}.
 * Network numbers are host-order 32-bit values on LP64. */
uint32_t inet_network(const char *text)
{
    uint32_t value=0; unsigned count=0;
    for (;;) {
        unsigned base=10, digits=0, part=0;
        if (*text=='0') { base=8; ++text; digits=1;
            if(*text=='x'||*text=='X') {base=16;++text;digits=0;} }
        for(;;++text) {
            unsigned d;
            if(*text>='0'&&*text<='9') d=(unsigned)(*text-'0');
            else if(*text>='a'&&*text<='f') d=(unsigned)(*text-'a'+10);
            else if(*text>='A'&&*text<='F') d=(unsigned)(*text-'A'+10);
            else break;
            if(d>=base || part>(255-d)/base) return INADDR_NONE;
            part=part*base+d; ++digits;
        }
        if(!digits || ++count>4) return INADDR_NONE;
        value=(value<<8)|part;
        if(*text!='.') break;
        ++text;
    }
    if(*text && *text!=' '&&*text!='\t'&&*text!='\n') return INADDR_NONE;
    return value;
}
struct in_addr inet_makeaddr(uint32_t net,uint32_t host)
{
    uint32_t address;
    if(net<128) address=(net<<24)|(host&0xffffff);
    else if(net<65536) address=(net<<16)|(host&0xffff);
    else if(net<16777216) address=(net<<8)|(host&0xff);
    else address=net|host;
    return (struct in_addr){htonl(address)};
}
uint32_t inet_lnaof(struct in_addr a)
{
    uint32_t v=ntohl(a.s_addr);
    return v<0x80000000 ? v&0xffffff : v<0xc0000000 ? v&0xffff : v&0xff;
}
uint32_t inet_netof(struct in_addr a)
{
    uint32_t v=ntohl(a.s_addr);
    return v<0x80000000 ? v>>24 : v<0xc0000000 ? v>>16 : v>>8;
}
