#include "keyboard.h"
#include "io.h"
#include <stddef.h>
#define POLLS 100000U
static void (*input)(uint8_t);
static bool active, extended, caps, caps_down;
static uint8_t shifts, controls, pause_bytes;
static const uint8_t plain[128] = {
 [1]=27,[2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',
 [12]='-',[13]='=',[14]=127,[15]='\t',[16]='q',[17]='w',[18]='e',[19]='r',[20]='t',
 [21]='y',[22]='u',[23]='i',[24]='o',[25]='p',[26]='[',[27]=']',[28]='\r',
 [30]='a',[31]='s',[32]='d',[33]='f',[34]='g',[35]='h',[36]='j',[37]='k',[38]='l',
 [39]=';',[40]='\'',[41]='`',[43]='\\',[44]='z',[45]='x',[46]='c',[47]='v',[48]='b',
 [49]='n',[50]='m',[51]=',',[52]='.',[53]='/',[55]='*',[57]=' ',[74]='-',[78]='+'
};
static const uint8_t shifted[128] = {
 [2]='!',[3]='@',[4]='#',[5]='$',[6]='%',[7]='^',[8]='&',[9]='*',[10]='(',[11]=')',
 [12]='_',[13]='+',[26]='{',[27]='}',[39]=':',[40]='"',[41]='~',[43]='|',
 [51]='<',[52]='>',[53]='?'
};
void keyboard_decode(uint8_t code)
{
    if(pause_bytes){--pause_bytes;return;}
    if(code==0xe1){pause_bytes=5;extended=false;return;}
    if(code==0xe0){extended=true;return;}
    bool ext=extended;extended=false;
    bool release=(code&128)!=0;unsigned key=code&127;
    if(key==29){uint8_t bit=ext?2:1;if(release)controls&=~bit;else controls|=bit;return;}
    if(ext){
        if(!release && input){
            const char *sequence=NULL;
            switch(key){
            case 28:input('\r');break;case 53:input('/');break;
            case 72:sequence="\033[A";break;case 80:sequence="\033[B";break;
            case 77:sequence="\033[C";break;case 75:sequence="\033[D";break;
            case 71:sequence="\033[H";break;case 79:sequence="\033[F";break;
            case 73:sequence="\033[5~";break;case 81:sequence="\033[6~";break;
            case 82:sequence="\033[2~";break;case 83:sequence="\033[3~";break;
            }
            if(sequence)while(*sequence)input(*sequence++);
        }
        return;
    }
    if(key==42 || key==54){uint8_t bit=key==42?1:2;if(release)shifts&=~bit;else shifts|=bit;return;}
    if(key==58){if(!release && !caps_down)caps=!caps;caps_down=!release;return;}
    if(release)return;
    uint8_t c=plain[key];if(!c)return;
    if(c>='a' && c<='z'){if((shifts!=0)^caps)c-=32;}
    else if(shifts && shifted[key])c=shifted[key];
    if(controls){
        if(c>='a' && c<='z')c-=32;
        if(c>='@' && c<='_')c&=31;
        else if(c==' ' || c=='2')c=0;
        else if(c=='?')c=127;
    }
    if(input)input(c);
}
static bool wait_write(void)
{for(unsigned i=0;i<POLLS;++i)if(!(inb(0x64)&2))return true;return false;}
static bool command(uint8_t c)
{if(!wait_write())return false;outb(0x64,c);return true;}
static bool read_byte(uint8_t *value)
{
    for(unsigned i=0;i<POLLS;++i){uint8_t s=inb(0x64);if(s&1){uint8_t c=inb(0x60);if(!(s&0xe0)){*value=c;return true;}}}
    return false;
}
static bool data(uint8_t c)
{if(!wait_write())return false;outb(0x60,c);return true;}
static bool keyboard_command(uint8_t c)
{
    for(unsigned retry=0;retry<3;++retry){uint8_t reply;if(!data(c)||!read_byte(&reply))return false;if(reply==0xfa)return true;if(reply!=0xfe)return false;}
    return false;
}
bool keyboard_init(void (*receive)(uint8_t))
{
    active=false;input=receive;extended=caps=caps_down=false;shifts=controls=pause_bytes=0;
    outb(0x21,inb(0x21)|2);
    if(!command(0xad)||!command(0xa7))return false;
    for(unsigned i=0;i<256 && (inb(0x64)&1);++i)(void)inb(0x60);
    uint8_t config;
    if(!command(0x20)||!read_byte(&config))return false;
    config=(config|0x40)&~0x13U; /* Translation on, IRQs off, first port enabled. */
    if(!command(0x60)||!data(config)||!command(0xae))return false;
    if(!keyboard_command(0xf5)||!keyboard_command(0xf0)||!keyboard_command(2)||!keyboard_command(0xf4))return false;
    if(!command(0x60)||!data(config|1))return false;
    active=true;outb(0x21,inb(0x21)&~2U);return true;
}
void keyboard_interrupt(void)
{
    for(unsigned i=0;i<256;++i){uint8_t s=inb(0x64);if(!(s&1))break;uint8_t c=inb(0x60);
        if(active && !(s&0xe0) && c!=0xfa && c!=0xfe)keyboard_decode(c);
    }
}
