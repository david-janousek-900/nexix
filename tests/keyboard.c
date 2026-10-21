#include <assert.h>
#include <stdio.h>
#include "keyboard.h"
static uint8_t queue[512],output[512],mask=255;
static unsigned head,count,used,writes;
static bool config_write,absent,no_ack,resend;
static void push(uint8_t c){queue[(head+count++)%512]=c;}
uint8_t inb(uint16_t port)
{
 if(port==0x21)return mask;
 if(port==0x64)return absent?255:count?1:0;
 if(port==0x60){assert(count);--count;return queue[head++%512];}
 return 0;
}
void outb(uint16_t port,uint8_t c)
{
 ++writes;
 if(port==0x21){mask=c;return;}
 if(port==0x64){if(c==0x20)push(0x30);if(c==0x60)config_write=true;return;}
 if(port==0x60){if(config_write){config_write=false;return;}if(!no_ack){push(resend?0xfe:0xfa);resend=false;}}
}
static void receive(uint8_t c){output[used++]=c;}
static void key(uint8_t c){keyboard_decode(c);}
int main(void)
{
 resend=true;assert(keyboard_init(receive));assert(!(mask&2));
 key(30);key(158);key(42);key(30);key(54);key(170);key(48);key(182);
 key(58);key(58);key(186);key(46);key(42);key(32);key(170);
 key(29);key(46);key(32);key(21);key(157);key(14);key(28);
 unsigned n=used;key(0xe0);key(42);key(0xe1);key(29);key(69);key(0xe1);key(157);key(197);assert(used==n);
 key(0xe0);key(29);key(46);key(0xe0);key(157);
 const uint8_t navigation[]={72,80,77,75,71,79,73,81,82,83};
 for(unsigned i=0;i<sizeof(navigation);++i){key(0xe0);key(navigation[i]);key(0xe0);key(navigation[i]|128);}
 const uint8_t expected[]={'a','A','B','C','d',3,4,25,127,'\r',3,27,'[','A',27,'[','B',27,'[','C',27,'[','D',27,'[','H',27,'[','F',27,'[','5','~',27,'[','6','~',27,'[','2','~',27,'[','3','~'};
 assert(used==sizeof(expected));for(unsigned i=0;i<used;++i)assert(output[i]==expected[i]);
 push(30);keyboard_interrupt();assert(output[used-1]=='A');
 no_ack=true;assert(!keyboard_init(receive));assert(mask&2);
 absent=true;unsigned before=writes;assert(!keyboard_init(receive));assert(writes-before<5);
 puts("PASS: PS/2 modifiers, controls, prefixes, IRQ input, retries and timeouts");
}
