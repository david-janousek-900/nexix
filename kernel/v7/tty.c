/* Native adaptation of V7 dev/tty.c's line discipline for the COM1 UART. */
#include "tty.h"
#include "tty_controller.h"
#include <nexix/modem.h>
#include "interrupts.h"
#include "packet.h"
#include "serial.h"
#include <nexix/kinfo.h>
static uint64_t tty_in,tty_out;
void tty_statistics(struct nx_kinfo_io *r){r->tty_in=tty_in;r->tty_out=tty_out;}

#include "uaccess.h"
#include "signals.h"
#include <nexix/errno.h>
#define RECORD_END 0x100
#define EMPTY_EOF 0x200
#define OUTPUT_CAPACITY 2048
#define OUTPUT_DELAY 0x100
struct tty {
    const struct tty_driver *driver;
    struct nx_sgttyb settings;
    struct nx_tchars chars;
    uint16_t input[TTY_INPUT_CAPACITY];
    uint8_t line[TTY_LINE_CAPACITY];
    unsigned head, count, length, opens, column;
    uint64_t dropped;
    v7_pid_t group;
    bool stopped,flow_blocked,flow_release,exclusive,hangup,pumping;
    uint8_t flow_resume;
    unsigned profile,out_head,out_count,waiting,modem_mode,modem_signals;
    bool carrier,hungup,closing;
    unsigned modem_outputs;
    uint16_t output[OUTPUT_CAPACITY];
    uint64_t delay_until;
    struct tty_parameters parameters;
    void *mpx_owner;
    char input_channel, output_channel, close_channel;
};
static struct tty terminals[TTY_LINES];
unsigned tty_console_line;
#define console terminals[0]
static bool valid_tty(struct tty *t){for(unsigned i=0;i<TTY_LINES;++i)if(t==&terminals[i])return t->driver!=NULL;return false;}
bool tty_handle_valid(struct tty *t){return valid_tty(t) && t->opens && !t->closing;}
static void receive(struct tty *,uint8_t);
static bool immediate(const struct tty *t) { return t->settings.flags & (NX_RAW | NX_CBREAK); }
static void start(struct tty *);
static void input_flow(struct tty *);
static void flush(struct tty *t)
{t->head=t->count=t->length=0;input_flow(t);}
static void flush_output(struct tty *t)
{t->out_head=t->out_count=0;t->delay_until=0;t->driver->flush(t->driver->context);proc_wakeup(&t->output_channel);}
static void resume(struct tty *t)
{ t->stopped = false; start(t);proc_wakeup(&t->output_channel);mpx_tty_resume(t); }
static void modem_outputs(struct tty *t,bool on)
{t->modem_outputs=on?NX_MDM_DTR|NX_MDM_RTS:0;t->driver->outputs(t->driver->context,on,on);}
static void carrier_update(struct tty *t,unsigned signals,bool notify)
{
    bool old=t->carrier;t->modem_signals=signals;
    t->carrier=!t->hungup && (t->modem_mode==NX_MDM_LOCAL || (signals&NX_MDM_DCD));
    if(old && !t->carrier && notify && t->opens>t->waiting){
        packet_abort(t);t->flow_blocked=t->flow_release=false;
        flush_output(t);flush(t);t->stopped=false;
        if(t->mpx_owner)mpx_tty_notify(t->mpx_owner,NX_SIGHUP);else signal_group(t->group,NX_SIGHUP);
        modem_outputs(t,false);
    }
    proc_wakeup(&t->input_channel);proc_wakeup(&t->output_channel);
}
void tty_modem_status(unsigned signals)
{if(console.modem_mode==NX_MDM_UART)carrier_update(&console,signals,true);}
int tty_modem_peer(unsigned signals)
{
    if(signals&~31U)return -NX_EINVAL;
    if(console.modem_mode!=NX_MDM_VIRTUAL)return -NX_EBUSY;
    carrier_update(&console,signals,true);return 0;
}
static void defaults(struct tty *t,unsigned profile)
{
    t->profile=profile;
    t->settings = (struct nx_sgttyb){NX_B115200, NX_B115200, 0177, 025, NX_ECHO | NX_CRMOD};
    t->chars = (struct nx_tchars){003, 034, 021, 023, 004, 0377};
    if(profile){
        t->settings=(struct nx_sgttyb){profile==NX_TTY_DC?NX_B150:profile==NX_TTY_KL?NX_B115200:NX_B300,
            profile==NX_TTY_DC?NX_B150:profile==NX_TTY_KL?NX_B115200:NX_B300,'#','@',NX_ODDP|NX_EVENP|NX_ECHO};
        t->chars.intr=0177;
        if(profile==NX_TTY_KL)t->settings.flags=NX_EVENP|NX_LCASE|NX_ECHO|NX_XTABS|NX_CRMOD|020000;
    }
    (void)tty_parameters(profile,&t->settings,&t->parameters);
    flush(t);resume(t);t->column=0;
    t->hungup=false;modem_outputs(t,true);
    carrier_update(t,t->modem_mode==NX_MDM_UART?t->driver->status(t->driver->context):t->modem_signals,false);
    t->driver->format(t->driver->context,t->parameters.rate,t->parameters.format);
}
static bool console_enqueue(void *v,const uint8_t *b,size_t n){(void)v;return serial_enqueue(b,n);}
static bool console_idle(void *v){(void)v;return serial_idle();}
static void console_flush(void *v){(void)v;serial_flush();}
static void console_format(void *v,unsigned r,uint8_t f){(void)v;serial_format(r,f);}
static void console_outputs(void *v,bool d,bool r){(void)v;serial_modem_outputs(d,r);}
static unsigned console_status(void *v){(void)v;return serial_modem_status();}
static const struct tty_driver console_driver={NULL,console_enqueue,console_idle,console_flush,console_format,console_outputs,console_status};
int tty_attach(unsigned line,const struct tty_driver *driver)
{
    if(line>=TTY_LINES || !driver || !driver->enqueue || !driver->idle || !driver->flush || !driver->format || !driver->outputs || !driver->status)return -NX_EINVAL;
    struct tty *t=&terminals[line];if(t->opens)return -NX_EBUSY;
    t->driver=driver;defaults(t,NX_TTY_NATIVE);return 0;
}
void tty_init(void)
{(void)tty_attach(0,&console_driver);serial_receive_enable();}
/* Only the foreground group, or root, may move a terminal to a new group. */
void tty_set_group(struct tty *t,v7_pid_t old,v7_pid_t group,bool privileged)
{if(valid_tty(t) && (privileged || !t->group || t->group==old))t->group=group;}
unsigned tty_open_count(void){unsigned n=0;for(unsigned i=0;i<TTY_LINES;++i)n+=terminals[i].opens;return n;}
int tty_open(v7_dev_t dev,struct user *u,struct tty **out)
{
    struct tty *t=&console;
    if((dev>>8)==1){if((dev&255)>=TTY_LINES)return -NX_ENXIO;t=&terminals[dev&255];}
    if(dev==NX_CONSOLE_DEV)t=&terminals[tty_console_line];
    if(dev==NX_CTTY_DEV){if(!valid_tty(u->u_ttyp))return -NX_ENXIO;t=u->u_ttyp;}
    if(!t->driver)return -NX_ENODEV;
    while(t->closing)if(proc_sleep_interruptible(&t->close_channel,28))return -NX_EINTR;
    unsigned profile=dev==NX_CTTY_DEV?t->profile:t->opens?t->profile:NX_TTY_NATIVE,line;
    if((dev>>8)!=1 && dev!=NX_CTTY_DEV){int error=tty_controller_device(dev,&profile,&line);if(error)return error;}
    if(t->opens && t->profile!=profile)return -NX_EBUSY;
    if(t->exclusive && u->u_uid)return -NX_EBUSY;
    if(!t->opens)defaults(t,profile);
    ++t->opens;++t->waiting;
    /* A carrier waiter must not finish opening while an earlier last close is still resetting the line. */
    while(!t->carrier || t->closing)if(proc_sleep_interruptible(t->closing?&t->close_channel:&t->input_channel,28)){
        --t->waiting;if(!--t->opens)modem_outputs(t,false);return -NX_EINTR;
    }
    --t->waiting;
    if(t->profile==NX_TTY_DH && (t->modem_signals&NX_MDM_SECONDARY)){
        t->settings.ispeed=t->settings.ospeed=NX_B1200;
        (void)tty_parameters(t->profile,&t->settings,&t->parameters);
        t->driver->format(t->driver->context,t->parameters.rate,t->parameters.format);
    }
    if(!u->u_ttyp){u->u_ttyp=t;u->u_ttyd=dev==NX_CTTY_DEV?NX_CONSOLE_DEV:dev;}
    if(!u->u_procp->p_pgrp){if(!t->group)t->group=u->u_procp->p_pid;u->u_procp->p_pgrp=t->group;}
    if(!t->group)t->group=u->u_procp->p_pgrp;
    *out=t;return 0;
}
static void start(struct tty *t)
{
    if(t->pumping || t->stopped || !t->carrier)return;
    t->pumping=true;
    if(t->delay_until){if(timer_ticks()<t->delay_until){t->pumping=false;return;}t->delay_until=0;}
    for(unsigned budget=0;budget<64 && t->out_count;++budget){
        uint16_t token=t->output[t->out_head];
        if(token&OUTPUT_DELAY){
            if(!t->driver->idle(t->driver->context))break;
            t->out_head=(t->out_head+1)%OUTPUT_CAPACITY;--t->out_count;
            t->delay_until=timer_ticks()+tty_delay(t->profile,token&127);break;
        }
        uint8_t byte=token;if(!t->driver->enqueue(t->driver->context,&byte,1))break;
        t->out_head=(t->out_head+1)%OUTPUT_CAPACITY;--t->out_count;++tty_out;
    }
    t->pumping=false;proc_wakeup(&t->output_channel);
}
void tty_timer(uint64_t now)
{(void)now;for(unsigned i=0;i<TTY_LINES;++i){struct tty *t=&terminals[i];if(!t->driver)continue;start(t);input_flow(t);proc_wakeup(&t->output_channel);if(t->mpx_owner && !t->stopped)mpx_tty_resume(t);}}
static int drain(struct tty *t)
{
    while(t->out_count || t->delay_until || !t->driver->idle(t->driver->context)){if(!t->carrier)return -NX_EIO;start(t);if(!t->out_count && !t->delay_until && t->driver->idle(t->driver->context))break;
        if(proc_sleep_interruptible(&t->output_channel,29))return -NX_EINTR;
    }
    return 0;
}
void tty_close(struct tty *t)
{
    if(!valid_tty(t) || !t->opens)panic("invalid tty close");
    /* Carrier waiters are not opens yet, so the last real close still resets the line. */
    if(t->opens-t->waiting==1){
        t->closing=true;
        packet_close(t);if(t->stopped)flush_output(t);if(drain(t))flush_output(t);
        flush(t);resume(t);if(drain(t))flush_output(t);t->group=0;t->exclusive=false;
        if(t->hangup)modem_outputs(t,false);
        t->hangup=false;
    }
    --t->opens;
    if(t->closing){t->closing=false;proc_wakeup(&t->close_channel);}
}
static void token(struct tty *t,uint16_t c)
{t->output[(t->out_head+t->out_count)%OUTPUT_CAPACITY]=c;++t->out_count;}
static void byte(struct tty *t,uint8_t c)
{if(t->parameters.software_parity && !(t->settings.flags&NX_RAW))c=(c&127)|(tty_character(c)&0200);token(t,c);}
static void logical(struct tty *t,uint8_t c)
{
    if(t->settings.flags&NX_RAW){token(t,c);return;}
    c&=127;if(c==004)return;
    if(c=='\t' && (t->settings.flags&NX_TBDELAY)==NX_XTABS){unsigned n=8-t->column%8;while(n--)logical(t,' ');return;}
    if(t->settings.flags&NX_LCASE){
        const char *pairs="({)}!|^~'`";
        for(unsigned i=0;i<10;i+=2)if(c==(uint8_t)pairs[i+1]){logical(t,'\\');c=pairs[i];break;}
        if(c>='a' && c<='z')c-='a'-'A';
    }
    if(c=='\n' && (t->settings.flags&(t->settings.flags&NX_TTY_SPLITCR?NX_TTY_ONLCR:NX_CRMOD)))logical(t,'\r');
    byte(t,c);unsigned delay=0,type=tty_character(c)&077,col=t->column;
    switch(type){
    case 0:t->column=(col+1)&255;break;
    case 2:if(col)--t->column;break;
    case 3:
        type=(t->settings.flags>>8)&3;
        if(type==1 && col){delay=(col>>4)+3;if(delay<6)delay=6;}else if(type==2)delay=6;
        t->column=0;break;
    case 4:
        if(((t->settings.flags>>10)&3)==1){delay=9-(col&7);if(delay<5)delay=0;}
        t->column=((col|7)+1)&255;break;
    case 5:if(t->settings.flags&NX_VTDELAY)delay=127;break;
    case 6:type=(t->settings.flags>>12)&3;if(type==1)delay=5;else if(type==2)delay=10;t->column=0;break;
    }
    if(delay)token(t,OUTPUT_DELAY|delay);
}
static bool output(struct tty *t,uint8_t c)
{
    start(t);if(OUTPUT_CAPACITY-t->out_count<16)return false;
    logical(t,c);start(t);return true;
}
static void input_flow(struct tty *t)
{
    if(t->flow_blocked && (t->flow_release || !(t->settings.flags&NX_TANDEM) || t->count+t->length<256/5)){
        if(t->flow_resume!=0377){if(t->out_count==OUTPUT_CAPACITY)return;byte(t,t->flow_resume);}
        t->flow_blocked=t->flow_release=false;start(t);
    }
}
static void input_high(struct tty *t)
{
    if((t->settings.flags&NX_TANDEM) && !t->flow_blocked && t->count+t->length>=128 && t->chars.stop!=0377 && OUTPUT_CAPACITY-t->out_count){
        byte(t,t->chars.stop);t->flow_resume=t->chars.start;t->flow_blocked=true;t->flow_release=false;start(t);
    }
}
static void echo(struct tty *t, uint8_t c)
{ if ((t->settings.flags & NX_ECHO) && !t->stopped) (void)output(t, c); }
static void enqueue(struct tty *t, uint16_t value)
{
    t->input[(t->head + t->count) % TTY_INPUT_CAPACITY] = value;
    ++t->count;
}
static void commit(struct tty *t)
{
    unsigned need = t->length ? t->length : 1;
    if (TTY_INPUT_CAPACITY - t->count < need) { t->dropped += need; t->length = 0; return; }
    if (!t->length) enqueue(t, RECORD_END | EMPTY_EOF);
    else {
        for (unsigned i = 0; i < t->length; ++i)
            enqueue(t, t->line[i] | (i + 1 == t->length ? RECORD_END : 0));
    }
    t->length = 0;
    proc_wakeup(&t->input_channel);
    if(t->mpx_owner)mpx_tty_notify(t->mpx_owner,0);
}
void tty_modem_line(unsigned line,unsigned signals)
{if(line<TTY_LINES && terminals[line].driver && terminals[line].modem_mode==NX_MDM_UART)carrier_update(&terminals[line],signals,true);}
void tty_receive_line(unsigned line,uint8_t c,unsigned errors)
{
    if(line>=TTY_LINES)return;
    struct tty *t=&terminals[line];if(!t->driver)return;uint8_t result;
    if(!tty_controller_input(t->profile,t->settings.flags,c,errors,&result)){++t->dropped;return;}
    if(errors&NX_RX_OVERRUN)++t->dropped;
    receive(t,result);
}
void tty_receive_status(uint8_t c,unsigned errors){tty_receive_line(0,c,errors);}
void tty_receive(uint8_t c){receive(&console,c);}
static void receive(struct tty *t,uint8_t c)
{
    ++tty_in;
    if (!t->opens || !t->carrier) return;
    if(packet_active(t)){packet_receive(t,c);return;}
    input_high(t);
    if (!(t->settings.flags & NX_RAW)) {
        if(!(t->settings.flags&NX_TTY_PASS8))c &= 0177;
        if (!(t->settings.flags&NX_TTY_NOSIG) && (c == t->chars.intr || c == t->chars.quit) && c != 0377) {
            flush_output(t);flush(t); resume(t);
            unsigned sig=c==t->chars.intr?NX_SIGINT:NX_SIGQUIT;
            if(t->mpx_owner)mpx_tty_notify(t->mpx_owner,sig);else signal_group(t->group,sig);
            proc_wakeup(&t->input_channel); return;
        }
        if (c == t->chars.start && c != 0377) { resume(t); return; }
        if (c == t->chars.stop && c != 0377) { t->stopped = true; return; }
        if (c == '\r' && (t->settings.flags & (t->settings.flags&NX_TTY_SPLITCR?NX_TTY_ICRNL:NX_CRMOD))) c = '\n';
    }
    if((t->settings.flags&NX_LCASE) && c>='A' && c<='Z')c+='a'-'A';
    if (immediate(t)) {
        if (t->count == TTY_INPUT_CAPACITY) { ++t->dropped; return; }
        enqueue(t, c); echo(t, c); proc_wakeup(&t->input_channel);
        if(t->mpx_owner)mpx_tty_notify(t->mpx_owner,0);
        return;
    }
    /* 0377 disables a control; PASS8 can also deliver it as ordinary data. */
    uint8_t echoed=c;bool quoted=t->length && t->line[t->length-1]=='\\';
    if(quoted){
        uint8_t mapped=tty_mapping(c);
        if((t->settings.erase!=0377 && c==t->settings.erase) ||
           (t->settings.kill!=0377 && c==t->settings.kill))mapped=c;
        if(mapped && (mapped==c || (t->settings.flags&NX_LCASE))){
            if(t->length<2 || t->line[t->length-2]!='\\')c=mapped;
            --t->length;
        }
    }
    if (!quoted && t->settings.erase != 0377 &&
        (c == t->settings.erase || (t->settings.erase == 0177 && c == '\b'))) {
        if (t->length) {
            --t->length;
            echo(t, '\b'); echo(t, ' '); echo(t, '\b');
        }
        return;
    }
    if (!quoted && t->settings.kill != 0377 && c == t->settings.kill) { t->length = 0; echo(t, '\n'); return; }
    if (!quoted && t->chars.eof != 0377 && c == t->chars.eof) { commit(t); return; }
    bool delimiter = (t->chars.eof != 0377 && c == t->chars.eof) || c == '\n' || (t->chars.brk != 0377 && c == t->chars.brk);
    /* Reserve a byte for a delimiter, so an overlong line remains readable. */
    if (t->length >= TTY_LINE_CAPACITY - (delimiter ? 0 : 1)) { ++t->dropped; return; }
    t->line[t->length++] = c; echo(t, echoed);
    if (delimiter) commit(t);
}
int64_t tty_io(struct tty *t, struct address_space *vm, uintptr_t address, size_t count, bool write)
{
    if(!write && !count)return 0;
    if(!write && t->mpx_owner)return -NX_ENXIO;
    if(!t->carrier)return write?-NX_EIO:0;
    if(packet_active(t))return packet_io(t,vm,address,count,write);
    if (!count) return 0;
    size_t done = 0;
    if (write) {
        uint8_t bytes[128];
        while (done < count) {
            size_t n = count - done; if (n > sizeof(bytes)) n = sizeof(bytes);
            if (copyin(vm, address + done, bytes, n)) panic("tty write mapping changed");
            for (size_t i = 0; i < n; ++i) {
                for(;;){
                    if(!t->carrier || packet_active(t))return done+i?(int64_t)(done+i):-NX_EIO;
                    if(!t->stopped && output(t,bytes[i]))break;
                    if(proc_sleep_interruptible(&t->output_channel,29))return done+i?(int64_t)(done+i):-NX_EINTR;
                }
            }
            done += n;
        }
        return done;
    }
    if(t->mpx_owner)return -NX_ENXIO;
    while (!t->count){
        if(!t->carrier)return 0;
        if(proc_sleep_interruptible(&t->input_channel,28))return -NX_EINTR;
        if(t->mpx_owner)return -NX_ENXIO;
        if(packet_active(t))return -NX_EIO;
    }
    while (done < count && t->count) {
        uint16_t value = t->input[t->head];
        t->head = (t->head + 1) % TTY_INPUT_CAPACITY; --t->count;
        if (!(value & EMPTY_EOF)) {
            uint8_t byte = value;
            if (copyout(vm, address + done, &byte, 1)) panic("tty read mapping changed");
            ++done;
        }
        if (!immediate(t) && (value & RECORD_END)) break;
    }
    input_flow(t);return done;
}
static void tcopy(void *destination,const void *source,size_t n)
{uint8_t *d=destination;const uint8_t *s=source;for(size_t i=0;i<n;++i)d[i]=s[i];}
int tty_control(struct tty *t,uint64_t request,void *argument)
{
    switch (request) {
    case NX_MDM_GET:{
        struct nx_modem_tty status={t->modem_mode,t->modem_signals,t->modem_outputs,0};
        tcopy(argument,&status,sizeof(status));return 0;
    }
    case NX_MDM_SET:{
        struct proc *p=proc_current();if(!p || p->p_user->u_uid)return -NX_EPERM;
        struct nx_modem_tty status;tcopy(&status,argument,sizeof(status));
        if(status.mode>NX_MDM_VIRTUAL || status.signals || status.outputs || status.reserved)return -NX_EINVAL;
        t->modem_mode=status.mode;modem_outputs(t,!t->hungup);
        carrier_update(t,status.mode==NX_MDM_UART?t->driver->status(t->driver->context):0,false);return 0;
    }
    case NX_TIOCGWINSZ: {
        struct nx_winsize size={t==&terminals[2]?25:24,80,0,0};
        tcopy(argument,&size,sizeof(size));return 0;
    }
    case NX_TIOCGETA: {
        struct nx_tty_state state={.settings=t->settings,.chars=t->chars};
        tcopy(argument,&state,sizeof(state));return 0;
    }
    case NX_TIOCSETA: {
        struct nx_tty_state state;tcopy(&state,argument,sizeof(state));
        if(state.reserved || state.action>2)return -NX_EINVAL;
        /* Validate all settings before any interruptible drain or mutation. */
        struct tty_parameters parameters;
        uint32_t allowed=NX_TANDEM|NX_CBREAK|NX_LCASE|NX_ECHO|NX_CRMOD|NX_RAW|NX_ODDP|NX_EVENP|
            NX_NLDELAY|NX_TBDELAY|NX_CRDELAY|NX_VTDELAY|NX_BSDELAY|
            NX_TTY_NOSIG|NX_TTY_PASS8|NX_TTY_SPLITCR|NX_TTY_ICRNL|NX_TTY_ONLCR;
        bool b0=!state.settings.ispeed && !state.settings.ospeed && (t->profile==NX_TTY_DH || t->profile==NX_TTY_DZ);
        if(state.settings.flags&~allowed || (!b0 && tty_parameters(t->profile,&state.settings,&parameters)))return -NX_EINVAL;
        if(packet_active(t))return -NX_EBUSY;
        if(state.action==1 && t->carrier){int error=drain(t);if(error)return error;}
        int error=tty_control(t,state.action==2?NX_TIOCSETP:NX_TIOCSETN,&state.settings);
        if(error)return error;
        return tty_control(t,NX_TIOCSETC,&state.chars);
    }
    case NX_TIOCGETP: tcopy(argument,&t->settings,sizeof(t->settings));return 0;
    case NX_TIOCSETP: case NX_TIOCSETN: {
        struct nx_sgttyb settings;
        tcopy(&settings,argument,sizeof(settings));
        uint32_t allowed=NX_TANDEM|NX_CBREAK|NX_LCASE|NX_ECHO|NX_CRMOD|NX_RAW|NX_ODDP|NX_EVENP|
            NX_NLDELAY|NX_TBDELAY|NX_CRDELAY|NX_VTDELAY|NX_BSDELAY|
            NX_TTY_NOSIG|NX_TTY_PASS8|NX_TTY_SPLITCR|NX_TTY_ICRNL|NX_TTY_ONLCR;
        struct tty_parameters parameters;
        bool b0=!settings.ispeed && !settings.ospeed && (t->profile==NX_TTY_DH || t->profile==NX_TTY_DZ);
        if(settings.flags&~allowed || (!b0 && tty_parameters(t->profile,&settings,&parameters)))return -NX_EINVAL;
        if(b0){
            if(packet_active(t))return -NX_EBUSY;
            t->flow_blocked=t->flow_release=false;flush_output(t);flush(t);
            t->settings=settings;t->hangup=t->hungup=true;
            modem_outputs(t,false);carrier_update(t,t->modem_signals,false);return 0;
        }
        if(packet_active(t))return -NX_EBUSY;
        if(request==NX_TIOCSETP && t->carrier){if(t->stopped)resume(t);int error=drain(t);if(error)return error;}
        else if(parameters.rate!=t->parameters.rate || parameters.format!=t->parameters.format){
            while(!t->driver->idle(t->driver->context))if(proc_sleep_interruptible(&t->output_channel,29))return -NX_EINTR;
        }
        if(request==NX_TIOCSETP || ((settings.flags^t->settings.flags)&(NX_RAW|NX_CBREAK|NX_LCASE)))flush(t);
        bool configure=parameters.rate!=t->parameters.rate || parameters.format!=t->parameters.format;
        t->settings=settings;t->parameters=parameters;
        if(t->hungup){t->hungup=false;modem_outputs(t,true);carrier_update(t,t->modem_signals,false);}
        input_flow(t);
        if(configure)t->driver->format(t->driver->context,parameters.rate,parameters.format);
        if(request==NX_TIOCSETP || (settings.flags&NX_RAW))resume(t);
        return 0;
    }
    case NX_TIOCGETC: tcopy(argument,&t->chars,sizeof(t->chars));return 0;
    case NX_TIOCSETC: {
        struct nx_tchars chars;
        tcopy(&chars,argument,sizeof(chars));
        if(t->flow_blocked)t->flow_release=true;
        t->chars = chars;input_flow(t);
        /* Changing flow-control bytes must not strand a stopped writer. */
        resume(t); return 0;
    }
    case NX_TIOCFLUSH:flush_output(t);flush(t);resume(t);return 0;
    case NX_TIOCHPCL:t->hangup=true;return 0;
    case NX_TIOCEXCL:t->exclusive=true;return 0;
    case NX_TIOCNXCL:t->exclusive=false;return 0;
    case NX_TIOCGETD: {
        int32_t discipline = packet_active(t)?1:0;
        tcopy(argument,&discipline,sizeof(discipline));return 0;
    }
    case NX_TIOCSETD: {
        int32_t discipline;
        tcopy(&discipline,argument,sizeof(discipline));
        if(discipline)return -NX_ENXIO;
        packet_close(t);flush(t);return 0;
    }
    default: return -NX_ENOTTY;
    }
}

int tty_control_size(uint64_t request,bool *get)
{
    *get=false;
    switch(request){
    case NX_TIOCGWINSZ:*get=true;return sizeof(struct nx_winsize);
    case NX_TIOCGETA:*get=true;return sizeof(struct nx_tty_state);
    case NX_TIOCSETA:return sizeof(struct nx_tty_state);
    case NX_MDM_GET:*get=true;return sizeof(struct nx_modem_tty);
    case NX_MDM_SET:return sizeof(struct nx_modem_tty);
    case NX_TIOCGETP:*get=true;return sizeof(struct nx_sgttyb);
    case NX_TIOCSETP:case NX_TIOCSETN:return sizeof(struct nx_sgttyb);
    case NX_TIOCGETC:*get=true;return sizeof(struct nx_tchars);
    case NX_TIOCSETC:return sizeof(struct nx_tchars);
    case NX_TIOCGETD:*get=true;return sizeof(int32_t);
    case NX_TIOCSETD:return sizeof(int32_t);
    case NX_TIOCFLUSH:case NX_TIOCHPCL:case NX_TIOCEXCL:case NX_TIOCNXCL:return 0;
    default:return -NX_ENOTTY;
    }
}
int tty_ioctl(struct tty *t,struct address_space *vm,uint64_t request,uintptr_t address)
{
    if(request==NX_DIOCGETP){
        struct nx_piocb p;int result=packet_status(t,&p);if(result<0)return result;
        int error=copyout(vm,address,&p,sizeof(p));return error?error:result;
    }
    if(request==NX_TIOCSETD){
        int32_t discipline;int error=copyin(vm,address,&discipline,sizeof(discipline));if(error)return error;
        if(discipline==1){
            struct nx_piocb p;
            if(!user_access_ok(vm,address,sizeof(p),true))return -NX_EFAULT;
            error=copyin(vm,address,&p,sizeof(p));if(error)return error;
            if(!(t->settings.flags&NX_RAW))return -NX_EINVAL;
            if(t->out_count || t->delay_until)return -NX_EBUSY;
            flush(t);error=packet_open(t,&p);
            if(!error)error=copyout(vm,address,&p,sizeof(p));
            return error;
        }
    }
    bool get;int size=tty_control_size(request,&get);if(size<0)return size;
    uint64_t data[4]={0};int error=0;
    if(!get && size)error=copyin(vm,address,&data,size);
    if(!error)error=tty_control(t,request,&data);
    if(!error && get)error=copyout(vm,address,&data,size);
    return error;
}
int tty_mpx_bind(struct tty *t,void *owner)
{
    if(!valid_tty(t) || !t->opens)return -NX_ENXIO;
    if(t->mpx_owner && t->mpx_owner!=owner)return -NX_EBUSY;
    t->mpx_owner=owner;packet_changed(t);proc_wakeup(&t->input_channel);return 0;
}
void tty_mpx_unbind(struct tty *t,void *owner)
{
    if(t->mpx_owner==owner){t->mpx_owner=NULL;proc_wakeup(&t->input_channel);}
}
bool tty_mpx_ready(struct tty *t){return packet_active(t)?packet_ready(t):t->count!=0;}
size_t tty_mpx_read(struct tty *t,uint8_t *bytes,size_t count,bool *eof)
{
    if(packet_active(t))return packet_take(t,bytes,count,eof);
    size_t done=0;*eof=false;
    while(done<count && t->count){
        uint16_t value=t->input[t->head];t->head=(t->head+1)%TTY_INPUT_CAPACITY;--t->count;
        if(value&EMPTY_EOF)*eof=true;else bytes[done++]=value;
        if(!immediate(t) && (value&RECORD_END))break;
    }
    input_flow(t);return done;
}
int64_t tty_mpx_write(struct tty *t,const uint8_t *bytes,size_t count)
{
    if(!t->carrier)return -NX_EIO;
    if(packet_active(t))return packet_put(t,bytes,count);
    if(t->stopped)return 0;
    size_t done=0;while(done<count && output(t,bytes[done]))++done;
    return done;
}

bool tty_packet_emit(struct tty *t,const uint8_t *bytes,size_t n)
{
    if(!t->driver->enqueue(t->driver->context,bytes,n))return false;
    tty_out+=n;return true;
}
bool tty_packet_reader(struct tty *t){return !t->mpx_owner;}
void tty_packet_notice(struct tty *t)
{
    proc_wakeup(&t->input_channel);proc_wakeup(&t->output_channel);
    if(t->mpx_owner)mpx_tty_notify(t->mpx_owner,0);
    mpx_tty_resume(t);
}
