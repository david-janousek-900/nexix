/* V7 mx1 JOIN/CONNECT/NPGRP/CSIG and tty/kl readiness hooks. */
static void mx_notify(struct mx_channel *);
static void mx_release(struct mx_channel *);
static bool mx_event_payload(struct mx_channel *,const void *,unsigned);
static struct mx_channel *mx_channel_at(unsigned i)
{ return i<NX_MX_CHANNELS?&mx_channels[i]:&mx_ports[i-NX_MX_CHANNELS]; }
void mpx_tty_notify(void *owner,unsigned sig)
{
    struct mx_channel *c=owner;
    if(!c->active || c->closing)return;
    if(sig && sig<NX_NSIG)c->signals|=1U<<sig;
    mx_notify(c);
}
void mpx_tty_resume(struct tty *t)
{
    for(unsigned i=0;i<NX_MX_CHANNELS+NX_MX_PORTS;++i){
        struct mx_channel *c=mx_channel_at(i);
        if(c->active && c->output_tty==t){
            if(c->tty_blocked){c->tty_blocked=false;c->unblock_event=true;}
            mx_notify(c);
        }
    }
}
void mpx_tty_discipline(struct tty *t)
{
    for(unsigned i=0;i<NX_MX_CHANNELS+NX_MX_PORTS;++i){
        struct mx_channel *c=mx_channel_at(i);
        if(c->active && (c->input_tty==t || c->output_tty==t)){
            ++c->generation;mx_notify(c);
        }
    }
}
static void mx_bridge_clear(struct mx_channel *c,int side)
{
    struct file *input=NULL,*output=NULL,*port=NULL;
    if(side<=0){
        if(c->input_tty)tty_mpx_unbind(c->input_tty,c);
        c->input_tty=NULL;input=c->input_file;c->input_file=NULL;
    }
    if(side>=0){c->output_tty=NULL;output=c->output_file;c->output_file=NULL;c->tty_blocked=false;}
    if(c->port && !side){port=c->port_file;c->port_file=NULL;}
    ++c->generation;mx_notify(c);
    if(input)(void)file_put(input);
    if(output)(void)file_put(output);
    if(port)(void)file_put(port);
}
static struct file *mx_tty_file(struct user *u,int fd)
{
    if(fd<0 || fd>=V7_NOFILE)return NULL;
    struct file *f=u->u_ofile[fd];
    return f && f->f_private && !(f->f_flag&(V7_FMP|V7_FPIPE|FILE_STORAGE|FILE_TAPE)) &&
        tty_handle_valid(f->f_private)?f:NULL;
}
static int mx_join_tty(struct user *u,struct mx_group *g,int fd)
{
    struct file *f=mx_tty_file(u,fd);if(!f)return -NX_ENXIO;
    if((f->f_flag&(V7_FREAD|V7_FWRITE))!=(V7_FREAD|V7_FWRITE))return -NX_EBADF;
    int index=mx_slot(g);if(index<0)return index;
    struct mx_channel *c=NULL;
    for(unsigned i=0;i<NX_MX_PORTS;++i)if(!mx_ports[i].active){c=&mx_ports[i];break;}
    if(!c)return -NX_ENXIO;
    int error=tty_mpx_bind(f->f_private,c);if(error)return error;
    c->active=c->port=true;c->parent=g;c->index=index;c->port_file=f;++f->f_count;
    c->input_tty=c->output_tty=f->f_private;g->entry[index].channel=c;mx_notify(c);
    return mx_index(c);
}
static int mx_connect(struct user *u,const struct nx_mx_args *a,bool disconnect)
{
    struct file *f=mx_tty_file(u,a->arg[0]);if(!f)return -NX_ENXIO;
    int fd=a->arg[1];
    if(fd<0 || fd>=V7_NOFILE || !u->u_ofile[fd] ||
       (u->u_ofile[fd]->f_flag&V7_FMP)!=V7_FMPY)return -NX_ENXIO;
    struct mx_channel *c=u->u_ofile[fd]->f_private;
    if(c->port || c->pending || c->closing)return -NX_ENXIO;
    if(c->ioctl_busy)return -NX_EBUSY;
    struct tty *t=f->f_private;int side=a->arg[2];
    if(disconnect){
        if((side<=0 && c->input_tty!=t) || (side>=0 && c->output_tty!=t))return -NX_ENXIO;
        mx_bridge_clear(c,side);return 0;
    }
    if((side<=0 && !(f->f_flag&V7_FREAD)) || (side>=0 && !(f->f_flag&V7_FWRITE)))return -NX_EBADF;
    if(side<=0){int error=tty_mpx_bind(t,c);if(error)return error;}
    struct file *old_input=NULL,*old_output=NULL;
    if(side<=0){
        ++f->f_count;old_input=c->input_file;
        if(c->input_tty && c->input_tty!=t)tty_mpx_unbind(c->input_tty,c);
        c->input_tty=t;c->input_file=f;
    }
    if(side>=0){++f->f_count;old_output=c->output_file;c->output_file=f;c->output_tty=t;}
    ++c->generation;mx_notify(c);
    if(old_input)(void)file_put(old_input);
    if(old_output)(void)file_put(old_output);
    return 0;
}
static int64_t mx_output(struct mx_channel *c,uintptr_t address,size_t count,bool nonblocking);
/* tty_control may sleep, so pin the bridged terminal's file until the request ends. */
static struct file *mx_tty_pin(struct mx_channel *c,struct tty *t)
{
    struct file *f=c->port?c->port_file:t==c->output_tty?c->output_file:c->input_file;
    if(f)++f->f_count;
    return f;
}
static int mx_ioctl_control(struct mx_channel *c,const struct nx_mx_ioctl *control)
{
    if(c->closing)return -NX_ENXIO;
    bool get;int size=tty_control_size(control->command,&get);if(size<0)return size;
    if(size>8)return -NX_ENOTTY;
    uint64_t data=0;for(unsigned i=0;i<sizeof(control->data);++i)((uint8_t *)&data)[i]=control->data[i];
    struct tty *t=c->output_tty?c->output_tty:c->input_tty;
    if(t){
        unsigned generation=c->generation;
        /* Reserve room for a GET reply before touching terminal state. */
        if(get)while(c->event_count+c->event_reserved>=NX_MX_EVENTS){
            if(proc_sleep_interruptible(c,40))return -NX_EINTR;
            if(c->closing || generation!=c->generation)return -NX_ENXIO;
            if(t!=c->output_tty && t!=c->input_tty)return -NX_ENXIO;
        }
        /* Hold the reply slot while tty_control may sleep. */
        struct file *pin=mx_tty_pin(c,t);
        if(get)++c->event_reserved;
        int error=tty_control(t,control->command,&data);if(pin)(void)file_put(pin);
        if(get)--c->event_reserved;
        if(error)return error;
        if(get && c->closing)return -NX_ENXIO;
        if(get){
            struct nx_mx_ioctl response={.code=NX_M_IOCTL,.command=control->command};
            for(unsigned i=0;i<sizeof(response.data);++i)response.data[i]=((uint8_t *)&data)[i];
            if(!mx_event_payload(c,&response,sizeof(response)))panic("mpx ioctl reply reservation");
        }
        return 0;
    }
    if(!c->ioctl_pending || c->ioctl_command!=control->command)return -NX_ENXIO;
    for(unsigned i=0;i<sizeof(c->ioctl_data);++i)c->ioctl_data[i]=control->data[i];
    bool abandoned=c->ioctl_abandoned;c->ioctl_abandoned=false;
    if(abandoned)c->ioctl_busy=false;
    c->ioctl_pending=false;mx_notify(c);return abandoned?-NX_EINTR:0;
}
static int mx_endpoint_ioctl(struct file *f,struct address_space *vm,uint64_t request,uintptr_t address)
{
    struct mx_channel *c=f->f_private;
    if(c->closing)return -NX_ENXIO;
    bool get;int size=tty_control_size(request,&get);
    if(size<0)return size;
    if(size>8)return -NX_ENOTTY;
    /* Changing the server's terminal requires a writable endpoint. */
    if(!get && !(f->f_flag&V7_FWRITE))return -NX_EBADF;
    if(!user_access_ok(vm,address,size,get))return -NX_EFAULT;
    struct nx_mx_ioctl event={.code=NX_M_IOCTL,.command=request};
    if(!get && size){int error=copyin(vm,address,event.data,size);if(error)return error;}
    ++c->users;int error=0;bool acquired=false;
    while(c->ioctl_busy || c->event_count+c->event_reserved>=NX_MX_EVENTS){
        if(proc_sleep_interruptible(c,40)){error=-NX_EINTR;break;}
        if(c->closing){error=-NX_ENXIO;break;}
    }
    if(c->closing)error=-NX_ENXIO;
    if(!error){
        c->ioctl_busy=true;acquired=true;
        struct tty *t=c->output_tty?c->output_tty:c->input_tty;
        if(t){
            uint64_t data=0;for(unsigned i=0;i<sizeof(event.data);++i)((uint8_t *)&data)[i]=event.data[i];
            struct file *pin=mx_tty_pin(c,t);
            ++c->event_reserved;
            error=tty_control(t,request,&data);if(pin)(void)file_put(pin);
            --c->event_reserved;
            if(!error){
                for(unsigned i=0;i<sizeof(event.data);++i)event.data[i]=((uint8_t *)&data)[i];
                (void)mx_event_payload(c,&event,sizeof(event));
            }
        }else{
            c->ioctl_pending=true;c->ioctl_command=request;
            (void)mx_event_payload(c,&event,sizeof(event));
            while(c->ioctl_pending && !c->closing)
                if(proc_sleep_interruptible(c,40)){error=-NX_EINTR;break;}
            if(c->closing)error=-NX_ENXIO;
            if(!error)for(unsigned i=0;i<sizeof(event.data);++i)event.data[i]=c->ioctl_data[i];
            if(error && c->ioctl_pending && !c->closing)c->ioctl_abandoned=true;
            mx_notify(c);
        }
        if(!error && get)error=copyout(vm,address,event.data,size);
    }
    if(acquired){
        if(!c->ioctl_pending || c->closing)c->ioctl_busy=false;
        if(c->closing)c->ioctl_pending=c->ioctl_abandoned=false;
        mx_notify(c);
    }
    mx_release(c);return error;
}
