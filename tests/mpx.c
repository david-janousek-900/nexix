/* Exercise the production private fio/mpx implementation; section GC removes
 * unrelated filesystem/device paths. QEMU supplies the real syscall/VM tests. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/v7/files.c"
static struct proc process;
static unsigned wakes,sleeps;
static int (*on_sleep)(void *);
static uintptr_t readonly_start,readonly_end;
static uint8_t output[256];static unsigned output_count;
static bool uart_idle=true;
bool serial_idle(void){return uart_idle;}
/* Device/filesystem teardown is mocked; the production file slot and TTY
 * close paths below must remain intact across an injected scheduler turn. */
struct v7fs *namespace_volume(v7_dev_t dev){(void)dev;return NULL;}
int v7fs_inode_get(struct v7fs *fs,v7_ino_t ino,struct v7fs_inode *ip)
{(void)fs;(void)ino;(void)ip;abort();}
int v7fs_release_inode(struct v7fs *fs,v7_ino_t ino){(void)fs;(void)ino;abort();}
void storage_close(struct storage_handle *h){(void)h;abort();}
int tape_close(void *p){(void)p;abort();}
void socket_file_close(void *p){(void)p;abort();}
int64_t socket_file_io(void *p,uintptr_t a,uint64_t n,bool w){(void)p;(void)a;(void)n;(void)w;abort();}
int output_close(void *p){(void)p;abort();}
void modem_close(void *p){(void)p;abort();}
bool output_matches(v7_dev_t dev){return dev>=NX_CAT_DEV && dev<=NX_VS_DEV;}
bool modem_matches(uint16_t dev){return (dev>>8)>=27 && (dev>>8)<=29;}
void serial_putc(uint8_t c){assert(output_count<sizeof(output));output[output_count++]=c;}
void serial_configure(unsigned rate,unsigned mode){(void)rate;(void)mode;}
void serial_receive_enable(void){}
void signal_group(v7_pid_t group,unsigned sig){(void)group;(void)sig;}
struct proc *proc_current(void){return &process;}
_Noreturn void panic(const char *message){fprintf(stderr,"panic: %s\n",message);abort();}
unsigned proc_wakeup(void *channel){assert(channel);++wakes;return 1;}
int proc_sleep_interruptible(void *channel,unsigned priority)
{assert(priority==40 || priority==29);++sleeps;return on_sleep?on_sleep(channel):-NX_EINTR;}
bool user_access_ok(const struct address_space *vm,uintptr_t address,size_t length,bool write)
{
    (void)vm;if(!length)return true;
    return address>=4096 && address<=UINTPTR_MAX-length &&
        !(write && address<readonly_end && address+length>readonly_start);
}
int copyin(const struct address_space *vm,uintptr_t source,void *destination,size_t length)
{if(!user_access_ok(vm,source,length,false))return -NX_EFAULT;memcpy(destination,(void *)source,length);return 0;}
int copyout(const struct address_space *vm,uintptr_t destination,const void *source,size_t length)
{if(!user_access_ok(vm,destination,length,true))return -NX_EFAULT;memcpy((void *)destination,source,length);return 0;}
static void reset_mx(void)
{
    memset(mx_groups,0,sizeof(mx_groups));memset(mx_channels,0,sizeof(mx_channels));
    wakes=sleeps=0;on_sleep=NULL;readonly_start=readonly_end=0;
}
static int orphan_waiter(void *channel)
{
    struct mx_channel *c=channel;assert(c->users==1 && c->y.size==200);
    c->closing=true;c->parent->entry[c->index]=(struct mx_entry){0};c->parent=NULL;
    mx_reap(c);assert(c->active && c->users==1);return 0;
}
static int drain_waiter(void *channel)
{
    struct mx_channel *c=channel;assert(c->y.size==200);
    mx_drop(c,&c->y,181);return 0;
}
static struct file *closing_file,*replacement;
static int allocate_during_close(void *channel)
{
    (void)channel;assert(!closing_file->f_count);
    replacement=file_alloc();assert(replacement && replacement!=closing_file);
    replacement->f_open_flags=NX_O_APPEND;uart_idle=true;return 0;
}
static struct mx_channel *bridged;
static int disconnect_during_drain(void *channel)
{
    (void)channel;struct tty *t=bridged->output_tty;mx_bridge_clear(bridged,1);
    assert(tty_handle_valid(t));uart_idle=true;return 0;
}
static int fill_during_ioctl(void *channel)
{
    (void)channel;
    for(unsigned i=0;i<NX_MX_EVENTS-1;++i)assert(mx_event(bridged,NX_M_EOT,0,NULL));
    assert(!mx_event(bridged,NX_M_EOT,0,NULL));
    uart_idle=true;return 0;
}
static int close_during_detach(void *channel)
{
    (void)channel;
    assert(bridged->users && bridged->endpoint);
    mpx_close(bridged->endpoint);
    assert(bridged->active && !bridged->parent);
    uart_idle=true;return 0;
}
static int accept_open(void *channel)
{
    struct mx_channel *c=channel;
    assert(c->event_count==1);
    struct nx_mx_control event;memcpy(&event,c->events[c->event_head].bytes,sizeof(event));
    assert(event.code==NX_M_OPEN || event.code==NX_M_WATCH);
    c->pending=false;return 0;
}
int main(void)
{
    reset_mx();struct mx_group *g=&mx_groups[0];struct mx_channel *c=mx_allocate(g);assert(c);
    char payload[401];memset(payload,'x',sizeof(payload));
    g->nonblocking=true;
    struct nx_mx_write h={.index=0xfff0,.count=250,.data=payload};
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==15);
    assert(h.count==50 && h.ccount==-1 && h.data==payload+200 && c->y.size==200);
    mx_drop(c,&c->y,180);assert(c->y.size==20 && !c->unblock_event);
    mx_drop(c,&c->y,1);assert(c->y.size==19 && c->unblock_event && !c->y.blocked_notice);
    uint8_t buffer[256];assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
    assert(((struct nx_mx_control *)(buffer+6))->code==NX_M_UBLK && !c->unblock_event);
    mx_drop(c,&c->y,19);
    h.ccount=0;assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==16 && c->y.size==50);
    mx_drop(c,&c->y,50);
    /* Every ring position wraps without overwriting unread control records. */
    for(unsigned repeat=0;repeat<3;++repeat){
        for(unsigned i=0;i<NX_MX_EVENTS;++i)assert(mx_event(c,NX_M_EOT,i,NULL));
        assert(!mx_event(c,NX_M_EOT,999,NULL) && c->event_count==NX_MX_EVENTS);
        c->close_event=true;c->unblock_event=true;
        for(unsigned i=0;i<NX_MX_EVENTS;++i){
            assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
            struct nx_mx_control event;memcpy(&event,buffer+6,sizeof(event));assert(event.code==NX_M_EOT && event.value==i);
        }
        assert(mx_group_read(g,(uintptr_t)buffer,12)==10 && ((struct nx_mx_control *)(buffer+6))->code==NX_M_CLOSE);
        assert(mx_group_read(g,(uintptr_t)buffer,12)==10 && ((struct nx_mx_control *)(buffer+6))->code==NX_M_UBLK);
    }
    char name[100];memset(name,'n',sizeof(name));name[99]=0;
    assert(mx_event(c,NX_M_OPEN,123,name));assert(mx_group_read(g,(uintptr_t)buffer,12)==0 && c->event_count==1);
    assert(mx_group_read(g,(uintptr_t)buffer,sizeof(buffer))==60 && !c->event_count);
    assert(((struct nx_mx_read *)buffer)->ccount==54 && buffer[59]==0);
    /* Preflight rejects every descriptor/payload before any queue mutation. */
    struct nx_mx_write records[2]={{.index=0xfff0,.count=3,.data=payload},{.index=0xfff0,.count=1,.data=(void *)1}};
    assert(mx_group_write(g,(uintptr_t)records,sizeof(records))==-NX_EFAULT && c->y.size==0);
    readonly_start=(uintptr_t)&h;readonly_end=readonly_start+sizeof(h);
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==-NX_EFAULT && c->y.size==0);
    readonly_start=readonly_end=0;
    assert(mx_group_write(g,(uintptr_t)&h,15)==-NX_EINVAL);
    /* Arrays larger than the channel table are legal; no native batch cap. */
    struct nx_mx_write many[80];
    for(unsigned i=0;i<80;++i)many[i]=(struct nx_mx_write){.index=0xfff0,.count=1,.data=payload};
    assert(mx_group_write(g,(uintptr_t)many,sizeof(many))==sizeof(many) && c->y.size==80);
    mx_drop(c,&c->y,80);
    g->nonblocking=false;
    h=(struct nx_mx_write){.index=0xfff0,.count=401,.data=payload};
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==15 && sleeps==1 && h.count==201);
    assert(c->y.size==200 && !c->users);mx_drop(c,&c->y,200);
    h=(struct nx_mx_write){.index=0xfff0,.count=250,.data=payload};on_sleep=drain_waiter;
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==16 && c->y.size==69 && !c->users);
    mx_drop(c,&c->y,69);
    /* An in-flight group writer survives detach and cannot hit a reused slot. */
    on_sleep=orphan_waiter;h=(struct nx_mx_write){.index=0xfff0,.count=401,.data=payload};
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==15 && h.count==201 && !c->active);
    assert(mx_allocate(g)==c && c->y.size==0 && c->event_count==0);
    reset_mx();tty_init();struct user user={.u_procp=&process};struct tty *t;
    process.p_pid=17;assert(tty_open(NX_CONSOLE_DEV,&user,&t)==0);
    /* Non-TTY private handles, including output devices, cannot be bridges. */
    struct inode output_inode={.i_mode=V7_IFCHR};output_inode.i_un.i_rdev=NX_CAT_DEV;
    struct file fake={.f_inode=&output_inode,.f_private=&output_inode,.f_flag=V7_FREAD|V7_FWRITE};
    user.u_ofile[0]=&fake;assert(!mx_tty_file(&user,0));
    struct file *terminal_file=file_alloc();assert(terminal_file);terminal_file->f_private=t;terminal_file->f_flag=V7_FREAD;
    user.u_ofile[0]=terminal_file;assert(mx_tty_file(&user,0)==terminal_file);
    g=&mx_groups[0];c=mx_allocate(g);assert(c);
    struct file connected={.f_private=c,.f_flag=V7_FMPY};user.u_ofile[1]=&connected;
    struct nx_mx_args connection={.arg={0,1,1}};
    assert(mx_connect(&user,&connection,false)==-NX_EBADF && !c->output_tty);
    assert(mx_join_tty(&user,g,0)==-NX_EBADF);
    terminal_file->f_flag=V7_FWRITE;connection.arg[2]=-1;
    assert(mx_connect(&user,&connection,false)==-NX_EBADF && !c->input_tty);
    assert(mx_join_tty(&user,g,0)==-NX_EBADF);
    /* Successful one-way connections preserve access restrictions. */
    connection.arg[2]=1;assert(!mx_connect(&user,&connection,false) && c->output_tty==t);
    assert(!mx_connect(&user,&connection,true) && !c->output_tty);
    terminal_file->f_flag=V7_FREAD;connection.arg[2]=-1;
    assert(!mx_connect(&user,&connection,false) && c->input_tty==t);
    assert(!mx_connect(&user,&connection,true) && !c->input_tty);
    memset(user.u_ofile,0,sizeof(user.u_ofile));reset_mx();
    /* Endpoint setup contributes direction flags, never grants I/O access. */
    struct file canonical={.f_count=1,.f_inode=&output_inode};g=&mx_groups[0];g->file=&canonical;
    struct file client={.f_count=1,.f_inode=&output_inode};on_sleep=accept_open;
    assert(!mpx_open(&client,"client",&user) && !(client.f_flag&(V7_FREAD|V7_FWRITE)));
    canonical.f_count=0;struct file late={.f_count=1,.f_inode=&output_inode};
    assert(mpx_open(&late,"closing",&user)==-NX_ENXIO && !late.f_private);
    reset_mx();
    g=&mx_groups[0];c=mx_allocate(g);assert(c);
    c->input_tty=c->output_tty=t;assert(tty_mpx_bind(t,c)==0);
    struct nx_sgttyb settings;assert(tty_control(t,NX_TIOCGETP,&settings)==0);
    settings.flags=NX_CRMOD;assert(tty_control(t,NX_TIOCSETP,&settings)==0);
    tty_receive('a');tty_receive('\n');
    assert(mx_group_read(g,(uintptr_t)buffer,sizeof(buffer))==8 && !memcmp(buffer+6,"a\n",2));
    tty_receive(4);assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
    assert(((struct nx_mx_control *)(buffer+6))->code==NX_M_EOT);
    for(unsigned i=0;i<NX_MX_EVENTS;++i)assert(mx_event(c,NX_M_EOT,0,NULL));
    tty_receive(3);assert(c->signals&(1U<<NX_SIGINT));
    for(unsigned i=0;i<NX_MX_EVENTS;++i)assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
    assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
    assert(((struct nx_mx_control *)(buffer+6))->code==NX_M_SIG && !c->signals);
    g->nonblocking=true;tty_receive(19);
    h=(struct nx_mx_write){.index=0xfff0,.count=3,.data="OUT"};
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==15 && c->tty_blocked && output_count==0);
    tty_receive(17);assert(!c->tty_blocked && c->unblock_event);
    assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
    h.ccount=0;assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==16 && output_count==3 && !memcmp(output,"OUT",3));
    struct nx_mx_ioctl request={.code=NX_M_IOCTL,.command=NX_TIOCGETP};
    h=(struct nx_mx_write){.index=0xfff0,.ccount=sizeof(request),.data=&request};
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==16);
    assert(mx_group_read(g,(uintptr_t)buffer,sizeof(buffer))==18 && ((struct nx_mx_ioctl *)(buffer+6))->command==NX_TIOCGETP);
    request.command=NX_MDM_GET;
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==-NX_ENOTTY);
    assert(mx_ioctl_control(c,&request)==-NX_ENOTTY);
    struct file modem_request={.f_private=c,.f_flag=V7_FMPY};
    assert(mx_endpoint_ioctl(&modem_request,&process.p_vm,NX_MDM_GET,(uintptr_t)buffer)==-NX_ENOTTY);
    request.command=NX_TIOCGETP;
    tty_mpx_unbind(t,c);c->input_tty=c->output_tty=NULL;
    /* Closing TTY sleeps while its original file slot remains reserved. */
    closing_file=file_alloc();assert(closing_file);closing_file->f_private=t;closing_file->f_flag=V7_FREAD;
    uart_idle=false;on_sleep=allocate_during_close;
    assert(!file_put(closing_file) && replacement->f_count==1 && replacement->f_open_flags==NX_O_APPEND);
    assert(file_alloc()==closing_file);on_sleep=NULL;
    /* A reply remains owned by its sleeping caller until it consumes data. */
    c->ioctl_busy=c->ioctl_pending=true;c->ioctl_command=NX_TIOCGETP;
    assert(mx_ioctl_control(c,&request)==0 && !c->ioctl_pending && c->ioctl_busy);
    c->ioctl_pending=c->ioctl_abandoned=true;
    assert(mx_ioctl_control(c,&request)==-NX_EINTR && !c->ioctl_pending && !c->ioctl_busy);
    c->closing=true;struct file endpoint={.f_private=c,.f_flag=V7_FMPY};
    assert(mx_endpoint_ioctl(&endpoint,&process.p_vm,NX_TIOCGETP,(uintptr_t)&settings)==-NX_ENXIO);
    /* A sleeping endpoint ioctl keeps a disconnected bridge's terminal open. */
    reset_mx();assert(tty_open(NX_CONSOLE_DEV,&user,&t)==0);
    terminal_file=file_alloc();assert(terminal_file);terminal_file->f_private=t;terminal_file->f_flag=V7_FREAD|V7_FWRITE;
    g=&mx_groups[0];c=mx_allocate(g);assert(c);bridged=c;
    struct file connected_rw={.f_private=c,.f_flag=V7_FMPY};
    user.u_ofile[0]=terminal_file;user.u_ofile[1]=&connected_rw;connection=(struct nx_mx_args){.arg={0,1,1}};
    assert(!mx_connect(&user,&connection,false) && c->output_tty==t);
    assert(!file_put(terminal_file) && terminal_file->f_count==1);memset(user.u_ofile,0,sizeof(user.u_ofile));
    struct file reader={.f_private=c,.f_flag=V7_FMPY|V7_FREAD};
    assert(mx_endpoint_ioctl(&reader,&process.p_vm,NX_TIOCSETP,(uintptr_t)&settings)==-NX_EBADF);
    const unsigned protected_controls[]={NX_TIOCEXCL,NX_TIOCHPCL,NX_TIOCFLUSH,NX_TIOCSETD};
    for(unsigned i=0;i<sizeof(protected_controls)/sizeof(*protected_controls);++i)
        assert(mx_endpoint_ioctl(&reader,&process.p_vm,protected_controls[i],(uintptr_t)&settings)==-NX_EBADF);
    struct file writer={.f_private=c,.f_flag=V7_FMPY|V7_FWRITE};
    unsigned opens=tty_open_count();uart_idle=false;on_sleep=disconnect_during_drain;
    assert(tty_mpx_write(t,(const uint8_t *)"x",1)==1);
    assert(!mx_endpoint_ioctl(&writer,&process.p_vm,NX_TIOCSETP,(uintptr_t)&settings));
    assert(!c->output_tty && tty_open_count()==opens-1);on_sleep=NULL;
    /* Completion capacity survives a sleeping connected terminal ioctl. */
    reset_mx();assert(!tty_open(NX_CONSOLE_DEV,&user,&t));
    terminal_file=file_alloc();assert(terminal_file);
    terminal_file->f_private=t;terminal_file->f_flag=V7_FREAD|V7_FWRITE;
    g=&mx_groups[0];c=mx_allocate(g);bridged=c;
    c->output_tty=t;c->output_file=terminal_file;
    writer.f_private=c;uart_idle=false;on_sleep=fill_during_ioctl;
    assert(!mx_endpoint_ioctl(&writer,&process.p_vm,NX_TIOCSETP,(uintptr_t)&settings));
    assert(c->event_count==NX_MX_EVENTS && !c->event_reserved);
    assert(c->events[NX_MX_EVENTS-1].size==sizeof(struct nx_mx_ioctl));
    on_sleep=NULL;mx_bridge_clear(c,0);
    /* Endpoint close during last-bridge drain cannot reap the detacher. */
    reset_mx();assert(!tty_open(NX_CONSOLE_DEV,&user,&t));
    terminal_file=file_alloc();assert(terminal_file);
    terminal_file->f_private=t;terminal_file->f_flag=V7_FREAD|V7_FWRITE;
    g=&mx_groups[0];c=mx_allocate(g);bridged=c;
    endpoint=(struct file){.f_private=c,.f_flag=V7_FMPY};c->endpoint=&endpoint;
    c->output_tty=t;c->output_file=terminal_file;
    uart_idle=false;on_sleep=close_during_detach;
    mx_detach((struct mx_entry){.channel=c},true);
    assert(!c->active && !g->entry[0].channel);on_sleep=NULL;
    puts("PASS: mpx record validation, queue bounds/wrap, low-water wakeups, control retention, nonblocking retry, interrupted writes and pinned detach lifetime, TTY data/EOF/signals/flow, ioctl replies and pinned bridge ioctls");return 0;
}
