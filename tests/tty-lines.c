#define main controller_regressions
#include "tty-controller.c"
#undef main
struct endpoint {uint8_t wire[4096];unsigned count,rate,format;};
static bool transmit_line(void *v,const uint8_t *p,size_t n){struct endpoint *e=v;assert(e->count+n<=sizeof(e->wire));memcpy(e->wire+e->count,p,n);e->count+=n;return true;}
static bool line_idle(void *v){(void)v;return true;}
static void line_flush(void *v){(void)v;}
static void line_format(void *v,unsigned rate,uint8_t bits){struct endpoint *e=v;e->rate=rate;e->format=bits;}
static void line_outputs(void *v,bool d,bool r){(void)v;(void)d;(void)r;}
static unsigned line_status(void *v){(void)v;return 0;}
int main(void)
{
    assert(!controller_regressions());tty_init();
    struct endpoint e={0};struct tty_driver driver={&e,transmit_line,line_idle,line_flush,line_format,line_outputs,line_status};
    assert(!tty_attach(1,&driver));struct proc a={.p_pid=10,.p_pgrp=10},b={.p_pid=20,.p_pgrp=20};
    struct user ua={.u_procp=&a},ub={.u_procp=&b};struct tty *x,*y,*alias;
    assert(!tty_open(0x100,&ua,&x) && !tty_open(0x101,&ub,&y) && x!=y && tty_open_count()==2);
    assert(tty_attach(1,&driver)==-NX_EBUSY);
    struct nx_sgttyb sx={16,16,127,21,NX_RAW},sy={13,13,'#','@',NX_CRMOD};
    assert(!ctl(x,NX_TIOCSETP,&sx) && !ctl(y,NX_TIOCSETP,&sy) && e.rate==9600);
    tty_receive_line(0,0x80,0);tty_receive_line(1,'a',0);tty_receive_line(1,'b',0);tty_receive_line(1,'#',0);tty_receive_line(1,'\r',0);
    const uint8_t high=0x80;expect(x,&high,1);expect(y,"a\n",2);
    assert(!tty_open(NX_CTTY_DEV,&ub,&alias) && alias==y);tty_close(alias);
    size=0;assert(tty_io(x,&vm,(uintptr_t)"one",3,true)==3);assert(tty_io(y,&vm,(uintptr_t)"two\n",4,true)==4);
    assert(size==3 && !memcmp(wire,"one",3) && e.count==5 && !memcmp(e.wire,"two\r\n",5));
    assert(!ctl(x,NX_TIOCEXCL,NULL));ub.u_uid=1;assert(!tty_open(0x101,&ub,&alias));tty_close(alias);ub.u_uid=0;
    tty_receive_line(1,'z',0);tty_receive_line(1,'\n',0);tty_close(x);expect(y,"z\n",2);tty_close(y);
    assert(!tty_open_count());
    struct endpoint local={0};struct tty_driver local_driver={&local,transmit_line,line_idle,line_flush,line_format,line_outputs,line_status};
    assert(!tty_attach(2,&local_driver));
    struct proc c={.p_pid=30};struct user uc={.u_procp=&c};struct tty *z;
    assert(!tty_open(0x102,&uc,&z) && z!=x && z!=y && uc.u_ttyd==0x102);
    assert(!tty_open(NX_CTTY_DEV,&uc,&alias) && alias==z);tty_close(alias);
    assert(!ctl(z,NX_TIOCSETP,&sy));
    tty_receive_line(2,'v',0);tty_receive_line(2,'g',0);tty_receive_line(2,'#',0);tty_receive_line(2,'\r',0);
    expect(z,"v\n",2);
    assert(tty_io(z,&vm,(uintptr_t)"local\n",6,true)==6);
    assert(local.count==7 && !memcmp(local.wire,"local\r\n",7));
    assert(!ctl(z,NX_TIOCSETP,&sx));
    tty_receive_line(2,0x80,0);tty_receive_line(2,0,0);
    const uint8_t raw_local[]={0x80,0};expect(z,raw_local,2);
    tty_close(z);assert(!tty_open_count());
    assert(tty_open(0x103,&uc,&alias)==-NX_ENXIO);
    puts("PASS: independent TTY queues, settings, transports, ownership, ctty alias, exclusivity and close lifetime");
}
