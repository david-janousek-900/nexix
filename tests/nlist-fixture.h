/* Independently encoded ELF64 bytes: seven sections and eight symbols. */
#include <stdint.h>
#include <string.h>
#define FIXTURE_SIZE 1536
static void putle(unsigned char *p,uint64_t v,unsigned n)
{ for(unsigned i=0;i<n;++i) p[i]=(unsigned char)(v>>(8*i)); }
static void fixture(unsigned char *b)
{
    memset(b,0,FIXTURE_SIZE);
    memcpy(b,"\177ELF\2\1\1",7);
    putle(b+16,2,2); putle(b+18,62,2); putle(b+20,1,4);
    putle(b+40,64,8); putle(b+52,64,2); putle(b+58,64,2); putle(b+60,7,2);
    /* section 1 .text, 2 .data, 3 .bss, 4 names, 5 symbols, 6 spare */
    for(unsigned i=1;i<=3;++i) {
        unsigned char *s=b+64+i*64;
        putle(s+4,i==3?8:1,4); putle(s+8,i==1?6:3,8);
    }
    static const char names[]="\0long_symbol_name\0data\0bss\0absolute\0common\0undefined\0weak\0";
    memcpy(b+512,names,sizeof(names));
    unsigned char *s=b+64+4*64;
    putle(s+4,3,4); putle(s+24,512,8); putle(s+32,sizeof(names),8);
    s=b+64+5*64;
    putle(s+4,2,4); putle(s+24,768,8); putle(s+32,8*24,8);
    putle(s+40,4,4); putle(s+56,24,8);
    const unsigned offsets[]={0,1,18,23,27,36,43,53};
    const unsigned sections[]={0,1,2,3,0xfff1,0xfff2,0,1};
    for(unsigned i=1;i<8;++i) {
        s=b+768+i*24; putle(s,offsets[i],4);
        s[4]=(unsigned char)((i==7?2:1)*16+(i==1?2:1));
        putle(s+6,sections[i],2);
        putle(s+8, i==6?0:UINT64_C(0x1234567800000000)+i,8);
        putle(s+16,i==5?64:8,8);
    }
}
