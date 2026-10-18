/* V7 dev/{dc,dh,dz,kl,partab}.c controller contracts on a native UART. */
#include "tty_controller.h"
#include <nexix/errno.h>
static const uint8_t partab[] = {
	0001,0201,0201,0001,0201,0001,0001,0201,
	0202,0004,0003,0201,0005,0206,0201,0001,
	0201,0001,0001,0201,0001,0201,0201,0001,
	0001,0201,0201,0001,0201,0001,0001,0201,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0201
};
uint8_t tty_character(uint8_t c){return partab[c&127];}
uint8_t tty_mapping(uint8_t c)
{
    if(c>='a' && c<='z')return c-'a'+'A';
    switch(c){case '!':return '|';case '\'':return '`';case '(':return '{';case ')':return '}';case '^':return '~';default:return 0;}
}
int tty_controller_device(uint16_t dev,unsigned *profile,unsigned *line)
{
    unsigned major=dev>>8;*line=dev&255;
    if(major<23 || major>26)return -NX_ENODEV;
    *profile=major-22;
    unsigned count=*profile==NX_TTY_DC?4:*profile==NX_TTY_KL?1:16;
    if(*line>=count)return -NX_ENXIO;
    return *line?-NX_ENODEV:0; /* Only native COM1 is bound before tasks 89-93. */
}
int tty_parameters(unsigned profile,const struct nx_sgttyb *s,struct tty_parameters *out)
{
    static const unsigned rates[]={0,50,75,110,134,150,200,300,600,1200,1800,2400,4800,9600,19200,38400,115200};
    if(profile>NX_TTY_KL || s->ispeed>16 || s->ospeed>16)return -NX_EINVAL;
    if(profile!=NX_TTY_KL && (!s->ispeed || s->ispeed!=s->ospeed))return -NX_EINVAL;
    if(profile==NX_TTY_DC && s->ispeed!=4 && s->ispeed!=5 && s->ispeed!=7 && s->ispeed!=9)return -NX_EINVAL;
    if(profile==NX_TTY_DH && s->ispeed>15)return -NX_EINVAL;
    if(profile==NX_TTY_DZ && (s->ispeed==6 || s->ispeed>=14))return -NX_EINVAL;
    unsigned parity=s->flags&(NX_ODDP|NX_EVENP);
    *out=(struct tty_parameters){.rate=profile==NX_TTY_KL?115200:rates[s->ospeed],.format=3};
    if(profile==NX_TTY_KL || profile==NX_TTY_DC){out->software_parity=!(s->flags&NX_RAW);return 0;}
    if(profile==NX_TTY_NATIVE){
        if(!(s->flags&NX_RAW) && (parity==NX_ODDP || parity==NX_EVENP))out->format=parity==NX_ODDP?0x0a:0x1a;
        return 0;
    }
    if(profile==NX_TTY_DH && s->ispeed==4)out->format=1|8; /* Six data bits, parity (134.5 baud). */
    else if(!(s->flags&NX_RAW))out->format=2|8;
    if((out->format&8) && (s->flags&NX_EVENP))out->format|=16;
    if((profile==NX_TTY_DH?s->ospeed:s->ispeed)==3)out->format|=4;
    return 0;
}
bool tty_controller_input(unsigned profile,uint32_t flags,uint8_t c,unsigned errors,uint8_t *out)
{
    unsigned parity=flags&(NX_ODDP|NX_EVENP);
    if(profile==NX_TTY_NATIVE && (errors&(NX_RX_PARITY|NX_RX_FRAME|NX_RX_BREAK)))return false;
    if(profile==NX_TTY_DC){
        if(errors&(NX_RX_FRAME|NX_RX_BREAK) || !parity)return false;
        bool even=(c&0200)==(tty_character(c)&0200);
        if(!(flags&(even?NX_EVENP:NX_ODDP)))return false;
        if((errors&NX_RX_PARITY) && parity!=(NX_ODDP|NX_EVENP))return false;
    }
    if(profile==NX_TTY_DH || profile==NX_TTY_DZ){
        if((errors&NX_RX_PARITY) && (parity==NX_ODDP || parity==NX_EVENP))return false;
        if(errors&(NX_RX_FRAME|NX_RX_BREAK))c=flags&NX_RAW?0:0177;
    }
    *out=c;return true;
}
unsigned tty_delay(unsigned profile,unsigned ticks)
{
    if(!ticks)return 0;
    ticks+=profile==NX_TTY_DC || profile==NX_TTY_KL?4:profile==NX_TTY_DH || profile==NX_TTY_DZ?6:0;
    return (ticks*100+59)/60;
}
