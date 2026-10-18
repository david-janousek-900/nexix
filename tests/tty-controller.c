#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tty.h"
#include "tty_controller.h"
#include "uaccess.h"
#include <nexix/errno.h>
static uint8_t wire[32768],buffer[2048];static unsigned size,baud,format,sleeps;static uint64_t ticks;
static bool blocked,dtr,idle=true;
static struct address_space vm;
uint64_t timer_ticks(void){return ticks;}
void serial_putc(uint8_t b){assert(size<sizeof(wire));wire[size++]=b;}
bool serial_enqueue(const uint8_t *p,size_t n){if(blocked)return false;while(n--)serial_putc(*p++);return true;}
void serial_configure(unsigned rate,unsigned parity){baud=rate;format=parity;}
void serial_format(unsigned rate,uint8_t control){baud=rate;format=control;}
void serial_receive_enable(void){}
void serial_flush(void){}
bool serial_idle(void){return idle;}
void serial_dtr(bool on){dtr=on;}
unsigned proc_wakeup(void *p){assert(p);return 0;}
int proc_sleep_interruptible(void *p,unsigned priority)
{assert(p && priority==29);++sleeps;++ticks;tty_timer(ticks);return sleeps>1000?-NX_EINTR:0;}
void signal_group(v7_pid_t g,unsigned n){(void)g;(void)n;abort();}
void mpx_tty_notify(void *p,unsigned n){(void)p;(void)n;}
void mpx_tty_resume(struct tty *p){(void)p;}
_Noreturn void panic(const char *s){puts(s);abort();}
int copyin(const struct address_space *v,uintptr_t a,void *p,size_t n){(void)v;if(a<4096)return -NX_EFAULT;memcpy(p,(void *)a,n);return 0;}
int copyout(const struct address_space *v,uintptr_t a,const void *p,size_t n){(void)v;if(a<4096)return -NX_EFAULT;memcpy((void *)a,p,n);return 0;}
static int ctl(struct tty *t,unsigned r,void *p){return tty_ioctl(t,&vm,r,(uintptr_t)p);}
static void feed(const char *p){while(*p)tty_receive((uint8_t)*p++);}
static void expect(struct tty *t,const void *p,size_t n)
{assert(tty_io(t,&vm,(uintptr_t)buffer,sizeof(buffer),false)==(int64_t)n && !memcmp(buffer,p,n));}
int main(void)
{
    struct proc process={.p_pgrp=17};struct user u={.u_procp=&process};struct tty *t,*other;
    tty_init();assert(dtr && baud==115200 && format==3);
    assert(!tty_open(NX_CONSOLE_DEV,&u,&t));struct nx_sgttyb s={16,16,127,21,NX_LCASE};
    assert(!ctl(t,NX_TIOCSETP,&s));feed("ABC\\(\\)\\!\\^\\'\\a\n");expect(t,"abc{}|~`A\n",10);
    feed("\\a\\z\n");expect(t,"AZ\n",3);
    assert(tty_io(t,&vm,(uintptr_t)"a{}|~`\n",7,true)==7);
    assert(size==12 && !memcmp(wire,"A\\(\\)\\!\\^\\'\n",12));
    s.flags=NX_RAW|NX_TANDEM;assert(!ctl(t,NX_TIOCSETP,&s));size=0;
    for(unsigned i=0;i<129;++i)tty_receive('x');
    assert(size==1 && wire[0]==023);
    for(unsigned i=0;i<20;++i)tty_receive('x');
    assert(size==1);
    assert(tty_io(t,&vm,(uintptr_t)buffer,sizeof(buffer),false)==149 && size==2 && wire[1]==021);
    s.flags=NX_RAW|NX_LCASE;assert(!ctl(t,NX_TIOCSETP,&s));feed("AZ");expect(t,"az",2);
    s.flags=001000;assert(!ctl(t,NX_TIOCSETP,&s));size=0;idle=false;
    assert(tty_io(t,&vm,(uintptr_t)"\nZ",2,true)==2 && size==1 && wire[0]=='\n');
    ticks+=100;tty_timer(ticks);assert(size==1); /* Delay starts at wire idle, not enqueue time. */
    idle=true;tty_timer(ticks);ticks+=9;tty_timer(ticks);assert(size==1);++ticks;tty_timer(ticks);assert(size==2 && wire[1]=='Z');
    s.flags=040000;assert(!ctl(t,NX_TIOCSETP,&s));size=0;
    assert(tty_io(t,&vm,(uintptr_t)"\fX",2,true)==2 && size==1);
    assert(!ctl(t,NX_TIOCFLUSH,NULL));ticks+=1000;tty_timer(ticks);assert(size==1);
    s.flags=001000;assert(!ctl(t,NX_TIOCSETP,&s));size=0;
    assert(tty_io(t,&vm,(uintptr_t)"\nR",2,true)==2 && size==1);
    assert(!ctl(t,NX_TIOCSETP,&s) && size==2 && sleeps>0); /* SETP drains a real timer delay. */
    s.flags=NX_RAW;assert(!ctl(t,NX_TIOCSETP,&s));size=0;blocked=true;
    for(unsigned i=0;i<3000;++i)assert(tty_mpx_write(t,(const uint8_t *)"x",1)==(i<2033?1:0));
    blocked=false;for(unsigned i=0;i<64;++i){++ticks;tty_timer(ticks);}assert(size==2033);
    assert(!ctl(t,NX_TIOCEXCL,NULL));u.u_uid=1;assert(tty_open(NX_CONSOLE_DEV,&u,&other)==-NX_EBUSY);u.u_uid=0;
    assert(!tty_open(NX_CONSOLE_DEV,&u,&other));tty_close(other);assert(!ctl(t,NX_TIOCNXCL,NULL));
    assert(!ctl(t,NX_TIOCHPCL,NULL));tty_close(t);assert(!dtr);
    for(unsigned profile=1;profile<=4;++profile){
        assert(!tty_open(NX_TTY_CONTROLLER_DEV(profile,0),&u,&t));assert(dtr);
        assert(tty_open(NX_TTY_CONTROLLER_DEV(profile==4?1:profile+1,0),&u,&other)==-NX_EBUSY);
        assert(!ctl(t,NX_TIOCGETP,&s));assert(s.erase=='#' && s.kill=='@');
        s.flags=NX_RAW|(profile==NX_TTY_DC?(NX_ODDP|NX_EVENP):0);s.ispeed=s.ospeed=profile==NX_TTY_DC?NX_B1200:NX_B9600;
        assert(!ctl(t,NX_TIOCSETP,&s));tty_receive_status('Q',NX_RX_PARITY);expect(t,"Q",1);
        if(profile==NX_TTY_DH || profile==NX_TTY_DZ){tty_receive_status('Q',NX_RX_BREAK|NX_RX_FRAME);expect(t,"",1);assert(!buffer[0]);}
        s.flags=NX_ODDP|NX_EVENP;assert(!ctl(t,NX_TIOCSETP,&s));size=0;
        assert(tty_io(t,&vm,(uintptr_t)"a",1,true)==1 && size==1);
        assert(wire[0]==(profile==NX_TTY_DC || profile==NX_TTY_KL?0xe1:'a'));
        assert(format==(profile==NX_TTY_DC || profile==NX_TTY_KL?3:0x1a));
        tty_receive_status('Q',NX_RX_PARITY);feed("\n");expect(t,"Q\n",2);
        if(profile==NX_TTY_DC){
            s.flags=NX_CBREAK|NX_EVENP;assert(!ctl(t,NX_TIOCSETP,&s));
            tty_receive_status(0x61,0);tty_receive_status(0xe2,0);expect(t,"b",1);
            s.flags=NX_CBREAK|NX_ODDP;assert(!ctl(t,NX_TIOCSETP,&s));
            tty_receive_status(0xe1,0);tty_receive_status(0x61,0);expect(t,"a",1);
            s.ispeed=s.ospeed=NX_B9600;assert(ctl(t,NX_TIOCSETN,&s)==-NX_EINVAL);}
        tty_close(t);
    }
    struct tty_parameters parameters;unsigned profile,line;
    s=(struct nx_sgttyb){NX_B134,NX_B134,0,0,NX_EVENP};assert(!tty_parameters(NX_TTY_DH,&s,&parameters) && parameters.format==0x19);
    s.ispeed=s.ospeed=NX_B110;assert(!tty_parameters(NX_TTY_DZ,&s,&parameters) && parameters.format==0x1e);
    assert(tty_controller_device(NX_TTY_CONTROLLER_DEV(NX_TTY_DC,4),&profile,&line)==-NX_ENXIO);
    assert(tty_controller_device(NX_TTY_CONTROLLER_DEV(NX_TTY_DH,15),&profile,&line)==-NX_ENODEV);
    assert(tty_delay(NX_TTY_KL,10)==24 && tty_delay(NX_TTY_DH,6)==20);
    puts("PASS: controller plans/parity, source escapes, TANDEM, delayed/blocked output, timer drain/flush, exclusive opens, hangup and profile ownership");
    return 0;
}
