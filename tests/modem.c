#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modem.h"
#include "tty.h"
#include "uaccess.h"
#include <nexix/errno.h>
#include <nexix/signal.h>
static struct user user;
static struct proc process={.p_pid=17,.p_pgrp=17,.p_user=&user};
static struct address_space vm;
static uint64_t ticks;
static void *ctl;
static unsigned script,steps,digits,hangups,outputs,uart_signals;
static v7_pid_t interrupt_group;
static uint8_t number[16],wire[8192];static size_t used;
static uintptr_t readonly;
static struct tty *closing;
struct proc *proc_current(void){return &process;}
uint64_t timer_ticks(void){return ticks;}
int copyin(const struct address_space *v,uintptr_t a,void *p,size_t n)
{(void)v;if(a<4096)return -NX_EFAULT;memcpy(p,(void *)a,n);return 0;}
int copyout(const struct address_space *v,uintptr_t a,const void *p,size_t n)
{(void)v;if(a<4096 || a==readonly)return -NX_EFAULT;memcpy((void *)a,p,n);return 0;}
bool user_access_ok(const struct address_space *v,uintptr_t a,size_t n,bool write)
{(void)v;(void)n;return a>=4096 && (!write || a!=readonly);}
unsigned proc_wakeup(void *p){assert(p);return 0;}
_Noreturn void panic(const char *s){puts(s);abort();}
void signal_group(v7_pid_t g,unsigned n){if(n==NX_SIGINT){interrupt_group=g;return;}assert(g==17 && n==NX_SIGHUP);++hangups;}
void mpx_tty_notify(void *p,unsigned n){(void)p;(void)n;}
void mpx_tty_resume(struct tty *t){(void)t;}
void serial_putc(uint8_t c){assert(used<sizeof(wire));wire[used++]=c;}
bool serial_enqueue(const uint8_t *p,size_t n){while(n--)serial_putc(*p++);return true;}
void serial_configure(unsigned rate,unsigned parity){(void)rate;(void)parity;}
void serial_receive_enable(void){}
void serial_dtr(bool on){outputs=on?NX_MDM_DTR:0;}
void serial_modem_outputs(bool dtr,bool rts){outputs=(dtr?NX_MDM_DTR:0)|(rts?NX_MDM_RTS:0);}
unsigned serial_modem_status(void){return uart_signals;}
static int command(struct nx_modem_peer *p){return modem_ioctl(ctl,&vm,NX_MDM_PEER,(uintptr_t)p);}
static void signals(unsigned dev,unsigned bits)
{struct nx_modem_peer p={.device=dev,.operation=NX_PEER_SIGNALS,.signals=bits};assert(!command(&p));}
static void tty_link(unsigned bits)
{struct nx_modem_peer p={.device=NX_CONSOLE_DEV,.operation=NX_PEER_TTY,.signals=bits};assert(!command(&p));}
int proc_sleep_interruptible(void *p,unsigned priority)
{
    assert(p && (priority==28 || priority==29));ticks+=10;++steps;modem_timer(ticks);tty_timer(ticks);
    assert(steps<1000);
    if(!script)return -NX_EINTR;
    if(script==1){
        struct nx_modem_peer data={.device=NX_DN_DEV(0),.operation=NX_PEER_TAKE,.count=1};
        assert(!command(&data));if(data.count){number[digits++]=data.data[0];signals(NX_DN_DEV(0),digits==2?NX_DN_CONNECTED:NX_DN_READY);}
    }else if(script==2)signals(NX_DN_DEV(0),NX_DN_ABANDON);
    else if(script==3)signals(NX_DU_DEV,NX_MDM_DSR|NX_MDM_CTS);
    else if(script==4)signals(NX_DU_DEV,0);
    else if(script==5){struct nx_modem_peer data={.device=NX_DU_DEV,.operation=NX_PEER_END};assert(!command(&data));}
    else if(script==6)tty_link(NX_MDM_DCD|NX_MDM_SECONDARY);
    else if(script==8){tty_close(closing);script=0;}
    else if(script==9){tty_close(closing);tty_link(NX_MDM_DCD);}
    return 0;
}
int main(void)
{
    user.u_procp=&process;assert(!modem_open(NX_MODEMCTL_DEV,&user,&ctl));
    struct nx_modem_peer data={.device=NX_DN_DEV(0),.operation=NX_PEER_STATUS};
    user.u_uid=1;void *handle,*other;
    assert(modem_open(NX_MODEMCTL_DEV,&user,&other)==-NX_EPERM);
    assert(command(&data)==-NX_EPERM);user.u_uid=0;
    assert(modem_open(NX_DN_DEV(4),&user,&handle)==-NX_ENXIO);
    signals(NX_DN_DEV(0),NX_DN_POWER);assert(modem_open(NX_DN_DEV(0),&user,&handle)==-NX_ENXIO);
    signals(NX_DN_DEV(0),NX_DN_READY);assert(!modem_open(NX_DN_DEV(0),&user,&handle));
    assert(modem_open(NX_DN_DEV(0),&user,&other)==-NX_ENXIO);
    script=1;assert(modem_io(handle,&vm,(uintptr_t)"1-2",3,true)==3 && digits==2 && !memcmp(number,"12",2) && ticks>=200);
    assert(modem_io(handle,&vm,0,1,false)==-NX_ENXIO);modem_close(handle);
    for(unsigned i=0;i<4;++i){signals(NX_DN_DEV(i),0);assert(!modem_open(NX_DN_DEV(i),&user,&handle));modem_close(handle);}
    signals(NX_DN_DEV(0),NX_DN_READY);assert(!modem_open(NX_DN_DEV(0),&user,&handle));
    script=2;assert(modem_io(handle,&vm,(uintptr_t)"9",1,true)==-NX_EIO);modem_close(handle);
    signals(NX_DN_DEV(0),0);assert(!modem_open(NX_DN_DEV(0),&user,&handle));script=0;
    assert(modem_io(handle,&vm,(uintptr_t)"9",1,true)==-NX_EINTR);modem_close(handle);
    assert(!modem_open(NX_DU_DEV,&user,&handle));assert(!modem_open(NX_DU_DEV,&user,&other));
    process.p_pid=18;assert(modem_open(NX_DU_DEV,&user,&other)==-NX_ENXIO);process.p_pid=17;
    assert(modem_io(handle,&vm,(uintptr_t)number,1,false)==-NX_EIO);
    signals(NX_DU_DEV,NX_MDM_DSR|NX_MDM_CTS|NX_MDM_DCD);
    data=(struct nx_modem_peer){.device=NX_DU_DEV,.operation=NX_PEER_RECEIVE,.count=512};
    for(unsigned i=0;i<512;++i)data.data[i]=i;
    assert(!command(&data));
    data.count=1;assert(command(&data)==-NX_ENOSPC);
    data.operation=NX_PEER_END;data.count=0;assert(!command(&data));
    uint8_t bytes[600];assert(modem_io(handle,&vm,(uintptr_t)bytes,600,false)==512);
    for(unsigned i=0;i<512;++i)assert(bytes[i]==(uint8_t)i);
    assert(modem_io(handle,&vm,(uintptr_t)bytes,1,false)==1 && !bytes[0]);
    assert(modem_io(handle,&vm,(uintptr_t)bytes,600,true)==512);
    data.operation=NX_PEER_TAKE;data.count=100;readonly=(uintptr_t)&data;
    assert(command(&data)==-NX_EFAULT);readonly=0;
    assert(!command(&data) && data.count==100 && data.data[99]==99 &&
        (data.state&(NX_MDM_DTR|NX_MDM_RTS))==(NX_MDM_DTR|NX_MDM_RTS));
    data.count=512;assert(!command(&data) && data.count==412 && data.data[0]==100 && (data.state&NX_MODEM_READ) && (data.state&NX_MDM_DTR) && !(data.state&NX_MDM_RTS));
    signals(NX_DU_DEV,NX_MDM_DSR);script=3;
    assert(modem_io(handle,&vm,(uintptr_t)"abc",3,true)==3);
    data.count=512;assert(!command(&data) && data.count==3 && !memcmp(data.data,"abc",3));
    signals(NX_DU_DEV,NX_MDM_DSR);script=4;
    assert(modem_io(handle,&vm,(uintptr_t)"abc",3,true)==-NX_EIO);
    signals(NX_DU_DEV,NX_MDM_DSR);script=0;
    assert(modem_io(handle,&vm,(uintptr_t)"abc",3,true)==-NX_EINTR);
    script=7;uint64_t before=ticks;
    assert(modem_io(handle,&vm,(uintptr_t)"abc",3,true)==-NX_EIO && ticks-before>=1000);
    signals(NX_DU_DEV,NX_MDM_DSR|NX_MDM_CTS);assert(modem_io(handle,&vm,(uintptr_t)"abc",3,true)==3);
    ticks+=1000;modem_timer(ticks);data.operation=NX_PEER_STATUS;data.count=0;assert(!command(&data) && (data.state&NX_MODEM_READ));
    data.operation=NX_PEER_RECEIVE;data.count=1;data.data[0]=0x80;assert(!command(&data));script=5;
    assert(modem_io(handle,&vm,(uintptr_t)bytes,1,false)==1 && bytes[0]==0x80);
    modem_close(handle);modem_close(handle);
    assert(!modem_open(NX_DU_DEV,&user,&handle));script=7;before=ticks;
    assert(modem_io(handle,&vm,(uintptr_t)bytes,1,false)==0 && ticks-before>=1000);modem_close(handle);
    struct tty *tty;tty_init();assert(!tty_open(NX_CONSOLE_DEV,&user,&tty));
    struct nx_sgttyb s={16,16,127,21,NX_RAW|NX_TANDEM};assert(!tty_control(tty,NX_TIOCSETP,&s));
    struct nx_modem_tty mode={.mode=NX_MDM_VIRTUAL};user.u_uid=1;
    assert(tty_control(tty,NX_MDM_SET,&mode)==-NX_EPERM);user.u_uid=0;assert(!tty_control(tty,NX_MDM_SET,&mode));
    tty_link(NX_MDM_DCD);for(unsigned i=0;i<129;++i)tty_receive('x');
    unsigned sent=used;assert(sent && wire[sent-1]==023);tty_link(0);assert(hangups==1 && !outputs);
    assert(tty_io(tty,&vm,(uintptr_t)bytes,1,false)==0 && tty_io(tty,&vm,(uintptr_t)"x",1,true)==-NX_EIO);
    tty_link(0);assert(hangups==1);tty_link(NX_MDM_DCD);tty_timer(ticks);
    assert(used==sent);tty_link(0);assert(hangups==2);tty_close(tty);script=0;
    assert(tty_open(NX_TTY_CONTROLLER_DEV(NX_TTY_DH,0),&user,&tty)==-NX_EINTR);
    script=6;assert(!tty_open(NX_TTY_CONTROLLER_DEV(NX_TTY_DH,0),&user,&tty) && hangups==2);
    assert(!tty_control(tty,NX_TIOCGETP,&s) && s.ispeed==NX_B1200);
    s.ispeed=s.ospeed=0;assert(!tty_control(tty,NX_TIOCSETP,&s) && !outputs);
    s.ispeed=s.ospeed=NX_B300;assert(!tty_control(tty,NX_TIOCSETP,&s) && outputs);
    mode.mode=NX_MDM_UART;uart_signals=NX_MDM_DCD;assert(!tty_control(tty,NX_MDM_SET,&mode));
    tty_modem_status(0);assert(hangups==3);mode.mode=NX_MDM_LOCAL;assert(!tty_control(tty,NX_MDM_SET,&mode));
    tty_modem_status(0);assert(hangups==3);
    /* The last real close resets exclusive use and the group even while a carrier waiter holds an open. */
    mode.mode=NX_MDM_VIRTUAL;assert(!tty_control(tty,NX_MDM_SET,&mode));tty_link(0);
    assert(!tty_control(tty,NX_TIOCEXCL,NULL));struct tty *waiter;closing=tty;script=8;
    assert(tty_open(NX_TTY_CONTROLLER_DEV(NX_TTY_DH,0),&user,&waiter)==-NX_EINTR);
    user.u_uid=1;assert(tty_open(NX_TTY_CONTROLLER_DEV(NX_TTY_DH,0),&user,&waiter)==-NX_EINTR);user.u_uid=0;
    tty_link(NX_MDM_DCD);assert(!tty_open(NX_TTY_CONTROLLER_DEV(NX_TTY_DH,0),&user,&tty));tty_link(0);
    closing=tty;script=9;process.p_pid=19;process.p_pgrp=0;
    assert(!tty_open(NX_TTY_CONTROLLER_DEV(NX_TTY_DH,0),&user,&tty) && process.p_pgrp==19);
    s.flags=NX_CRMOD;assert(!tty_control(tty,NX_TIOCSETP,&s));
    tty_receive(0177);assert(interrupt_group==19);
    tty_close(tty);process.p_pid=process.p_pgrp=17;script=0;modem_close(ctl);
    puts("PASS: DN dialing/pause/completion/errors, DU ownership/records/flow/timeouts, peer atomicity, carrier waits/hangup, last close with carrier waiters, local/UART modes and B0");
}
