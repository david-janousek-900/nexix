/* V7 mx2 data/control paths. IRQ serialization is inherited from syscall/fio.
 * Sleeping operations pin channels independently of group/descriptor lifetime. */
static void mx_group_wake(struct mx_group *g)
{ for(;g;g=g->parent)proc_wakeup(g); }
static void mx_notify(struct mx_channel *c)
{ proc_wakeup(c);mx_group_wake(c->parent); }
static void mx_reap(struct mx_channel *c)
{ if(!c->parent && !c->endpoint && !c->users)zero(c,sizeof(*c)); }
static void mx_release(struct mx_channel *c)
{ if(!c->users)panic("mpx channel pin underflow");--c->users;mx_reap(c); }
static bool mx_event(struct mx_channel *c,uint16_t code,uint16_t value,const char *name)
{
    if(c->event_count==NX_MX_EVENTS)return false;
    struct mx_event *e=&c->events[(c->event_head+c->event_count)%NX_MX_EVENTS];
    struct nx_mx_control h={code,value};
    for(unsigned i=0;i<sizeof(h);++i)e->bytes[i]=((uint8_t *)&h)[i];
    e->size=sizeof(h);
    if(name){
        unsigned n=0;while(n<NX_MX_NAME-1 && name[n]){e->bytes[e->size++]=name[n++];}
        e->bytes[e->size++]=0;
    }
    ++c->event_count;mx_notify(c);return true;
}
static bool mx_ready(struct mx_channel *c)
{ return c->event_count || c->close_event || c->block_event || c->unblock_event || c->x.size; }
/* Bounded scan replaces the historical readiness bitmap. Rotate at every
 * group level so a busy subtree cannot starve another branch. */
static struct mx_channel *mx_next(struct mx_group *g)
{
    for(unsigned n=0;n<NX_MX_INDEX;++n){
        unsigned i=(g->rotation+n)%NX_MX_INDEX;struct mx_entry e=g->entry[i];
        struct mx_channel *c=e.group?mx_next(e.group):e.channel;
        if(c && mx_ready(c)){g->rotation=(i+1)%NX_MX_INDEX;return c;}
    }
    return NULL;
}
static void mx_drained(struct mx_channel *c,struct mx_queue *q)
{
    if(q->size<NX_MX_LOQ && !q->eot){
        if(q->blocked_notice){q->blocked_notice=false;c->unblock_event=true;}
        mx_notify(c);
    }
}
static void mx_peek(struct mx_queue *q,uint8_t *bytes,unsigned n)
{ for(unsigned i=0;i<n;++i)bytes[i]=q->bytes[(q->head+i)%NX_MX_QSIZE]; }
static void mx_drop(struct mx_channel *c,struct mx_queue *q,unsigned n)
{ q->head=(q->head+n)%NX_MX_QSIZE;q->size-=n;mx_drained(c,q); }
static int64_t mx_endpoint_read(struct file *f,uintptr_t address,size_t count)
{
    if(!count)return 0;
    struct mx_channel *c=f->f_private;
    struct mx_queue *q=(f->f_flag&V7_FMPX)?&c->x:&c->y;
    while(!q->size){
        if(q->eot){q->eot=false;mx_drained(c,q);return 0;}
        if(c->closing)return -NX_ENXIO;
        if(proc_sleep_interruptible(c,40))return -NX_EINTR;
    }
    unsigned n=count<q->size?count:q->size;uint8_t bytes[NX_MX_QSIZE];mx_peek(q,bytes,n);
    int error=copyout(&proc_current()->p_vm,address,bytes,n);if(error)return error;
    mx_drop(c,q,n);return n;
}
static int64_t mx_put(struct mx_channel *c,struct mx_queue *q,uintptr_t address,size_t count,bool nonblocking)
{
    size_t done=0;uint8_t bytes[NX_MX_HIQ];
    while(done<count){
        while(q->size>NX_MX_HIQ || q->eot){
            if(c->closing)return done?(int64_t)done:-NX_ENXIO;
            if(nonblocking){q->blocked_notice=true;return done;}
            if(proc_sleep_interruptible(c,40))return done?(int64_t)done:-NX_EINTR;
        }
        if(c->closing)return done?(int64_t)done:-NX_ENXIO;
        unsigned n=count-done<NX_MX_HIQ?count-done:NX_MX_HIQ;
        if(n>NX_MX_QSIZE-q->size)n=NX_MX_QSIZE-q->size;
        int error=copyin(&proc_current()->p_vm,address+done,bytes,n);
        if(error)return done?(int64_t)done:error;
        for(unsigned i=0;i<n;++i)q->bytes[(q->head+q->size+i)%NX_MX_QSIZE]=bytes[i];
        q->size+=n;done+=n;mx_notify(c);
    }
    return done;
}
static int64_t mx_endpoint_write(struct file *f,uintptr_t address,size_t count)
{
    struct mx_channel *c=f->f_private;
    if(c->closing){signal_post(proc_current(),NX_SIGPIPE);return -NX_EPIPE;}
    if(!count){
        while(!mx_event(c,NX_M_EOT,0,NULL)){
            if(proc_sleep_interruptible(c,40))return -NX_EINTR;
            if(c->closing){signal_post(proc_current(),NX_SIGPIPE);return -NX_EPIPE;}
        }
        return 0;
    }
    struct mx_queue *q=(f->f_flag&V7_FMPX)?&c->y:&c->x;
    int64_t result=mx_put(c,q,address,count,false);
    if(c->closing){signal_post(proc_current(),NX_SIGPIPE);if(result<=0)result=-NX_EPIPE;}
    return result;
}
static int64_t mx_group_read(struct mx_group *g,uintptr_t address,size_t count)
{
    if(address&1)return -NX_ENXIO;
    if(count<12)return 0;
    size_t done=0;
    while(count-done>=12){
        struct mx_channel *c=mx_next(g);
        if(!c){
            if(done)break;
            if(proc_sleep_interruptible(g,40))return -NX_EINTR;
            continue;
        }
        uint8_t bytes[sizeof(struct nx_mx_read)+4+NX_MX_NAME+NX_MX_HIQ+1]={0};
        struct nx_mx_read h={.index=mx_index(c)};
        unsigned n=0;bool control=true;unsigned kind=0;
        if(c->event_count){
            struct mx_event *e=&c->events[c->event_head];n=e->size;
            for(unsigned i=0;i<n;++i)bytes[sizeof(h)+i]=e->bytes[i];
        }else if(c->close_event || c->block_event || c->unblock_event){
            kind=c->close_event?NX_M_CLOSE:c->block_event?NX_M_BLK:NX_M_UBLK;
            struct nx_mx_control event={kind,0};n=sizeof(event);
            for(unsigned i=0;i<n;++i)bytes[sizeof(h)+i]=((uint8_t *)&event)[i];
        }else{
            control=false;n=c->x.size;if(n>NX_MX_HIQ)n=NX_MX_HIQ;
            if(n>count-done-sizeof(h))n=count-done-sizeof(h);
            mx_peek(&c->x,bytes+sizeof(h),n);
        }
        if(sizeof(h)+n>count-done)break; /* Complete control records only. */
        unsigned total=sizeof(h)+n;
        if((total&1) && total<count-done)++total;
        if(control)h.ccount=n;else h.count=n;
        for(unsigned i=0;i<sizeof(h);++i)bytes[i]=((uint8_t *)&h)[i];
        int error=copyout(&proc_current()->p_vm,address+done,bytes,total);
        if(error)return done?(int64_t)done:error;
        if(!control)mx_drop(c,&c->x,n);
        else if(!kind){c->event_head=(c->event_head+1)%NX_MX_EVENTS;--c->event_count;proc_wakeup(c);}
        else if(kind==NX_M_CLOSE)c->close_event=false;
        else if(kind==NX_M_BLK)c->block_event=false;
        else c->unblock_event=false;
        done+=total;
    }
    return done;
}
static int mx_eot(struct mx_group *g,struct mx_channel *c)
{
    while(c->y.eot){
        if(c->closing)return -NX_ENXIO;
        if(g->nonblocking){c->block_event=true;mx_notify(c);return 0;}
        if(proc_sleep_interruptible(c,40))return -NX_EINTR;
    }
    if(c->closing)return -NX_ENXIO;
    c->y.eot=true;mx_notify(c);return 0;
}
static int mx_validate_record(struct mx_group *g,struct address_space *vm,const struct nx_mx_write *h)
{
    if(h->reserved || h->count>INT16_MAX || h->ccount<0 ||
       (h->count && h->ccount) || (!h->count && h->ccount!=4))return -NX_EINVAL;
    unsigned n=h->count?h->count:h->ccount;
    if(!user_access_ok(vm,(uintptr_t)h->data,n,false))return -NX_EFAULT;
    if(!h->count){
        struct nx_mx_control event;int error=copyin(vm,(uintptr_t)h->data,&event,sizeof(event));
        if(error)return error;
        if(event.code!=NX_M_EOT)return -NX_ENXIO; /* TTY ioctl controls: task 67. */
    }
    struct mx_entry e=mx_lookup(g,h->index);if(!e.channel || e.channel->pending)return -NX_ENXIO;
    return 0;
}
static int64_t mx_group_write(struct mx_group *g,uintptr_t address,size_t count)
{
    if(!count)return 0;
    if(count%sizeof(struct nx_mx_write))return -NX_EINVAL;
    struct address_space *vm=&proc_current()->p_vm;
    /* Writable headers permit precise nonblocking/interrupted-write retries.
     * Preflight the full array without imposing an extra record-count limit. */
    if(!user_access_ok(vm,address,count,true))return -NX_EFAULT;
    size_t nr=count/sizeof(struct nx_mx_write);
    for(size_t i=0;i<nr;++i){
        struct nx_mx_write h;int error=copyin(vm,address+i*sizeof(h),&h,sizeof(h));if(error)return error;
        error=mx_validate_record(g,vm,&h);if(error)return error;
    }
    size_t consumed=0;unsigned truncated=0;bool nonblocking=g->nonblocking;
    for(size_t i=0;i<nr;++i){
        struct nx_mx_write h;int error=copyin(vm,address+i*sizeof(h),&h,sizeof(h));
        /* Shared physical aliases can change later descriptors while asleep. */
        if(!error)error=mx_validate_record(g,vm,&h);
        if(error)return consumed?(int64_t)(consumed-truncated):error;
        struct mx_entry e=mx_lookup(g,h.index);if(!e.channel || e.channel->pending)return consumed?(int64_t)(consumed-truncated):-NX_ENXIO;
        struct mx_channel *c=e.channel;++c->users;
        int64_t moved=h.count?mx_put(c,&c->y,(uintptr_t)h.data,h.count,nonblocking):mx_eot(g,c);
        bool partial=moved>=0 && h.count && moved<h.count;
        if(partial){
            h.count-=moved;h.data=(const uint8_t *)h.data+moved;h.ccount=-1;
            if(copyout(vm,address+i*sizeof(h),&h,sizeof(h)))panic("mpx header mapping changed");
            ++truncated;
        }
        mx_release(c);
        if(moved<0)return consumed?(int64_t)(consumed-truncated):moved;
        consumed+=sizeof(h);
        if(partial && !nonblocking)break;
    }
    /* Historical mxwrite leaves u_count equal to the number of blocked
     * records, not their byte lengths. Preserve that source-defined result. */
    return consumed-truncated;
}
static int64_t mpx_io(struct file *f,uintptr_t address,size_t count,bool write)
{
    if((f->f_flag&V7_FMP)==V7_FMP)
        return write?mx_group_write(f->f_private,address,count):mx_group_read(f->f_private,address,count);
    return write?mx_endpoint_write(f,address,count):mx_endpoint_read(f,address,count);
}
static int mpx_ioctl(struct file *f,uint64_t request)
{
    if((f->f_flag&V7_FMP)!=V7_FMP)return -NX_ENOTTY;
    struct mx_group *g=f->f_private;
    if(request==NX_MXNBLK){g->nonblocking=true;return 0;}
    if(request==NX_MXLSTN){if(mx_listener)return -NX_ENXIO;mx_listener=g;return 0;}
    return -NX_ENXIO;
}
static int mpx_open(struct file *f,const char *path,struct user *u)
{
    struct mx_group *g=NULL;
    for(unsigned i=0;i<NX_MX_GROUPS;++i)
        if(mx_groups[i].file && mx_groups[i].file->f_inode==f->f_inode){g=&mx_groups[i];break;}
    if(!g)return -NX_ENXIO;
    struct mx_channel *c=mx_allocate(g);if(!c)return -NX_ENXIO;
    c->pending=true;c->endpoint=f;f->f_private=c;f->f_flag=V7_FREAD|V7_FWRITE|V7_FMPY;
    (void)mx_event(c,g==mx_listener?NX_M_OPEN:NX_M_WATCH,u->u_uid,g==mx_listener?path:NULL);
    int error=0;
    while(c->pending && !c->closing){if(proc_sleep_interruptible(c,40)){error=-NX_EINTR;break;}}
    if(c->closing)error=-NX_ENXIO;
    if(error){
        mx_notify(c);
        if(c->parent){c->parent->entry[c->index]=(struct mx_entry){0};c->parent=NULL;}
        c->closing=true; /* fio's failed-open file_put owns final release. */
    }
    return error;
}
