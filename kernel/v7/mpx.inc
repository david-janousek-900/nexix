/* V7 dev/mx1.c and mx2.c ownership layer. Included by files.c so all
 * descriptor/inode mutations share fio's private tables and serialization.
 * No user pointers or unreferenced file/inode pointers escape this layer. */
struct mx_group;
struct mx_entry { struct mx_group *group; struct mx_channel *channel; };
struct mx_group {
    struct file *file;
    struct mx_group *parent;
    unsigned index, rotation;
    bool nonblocking;
    struct mx_entry entry[NX_MX_INDEX];
};
struct mx_queue {
    unsigned head, size;
    bool eot, blocked_notice;
    uint8_t bytes[NX_MX_QSIZE];
};
struct mx_event { unsigned size; uint8_t bytes[4+NX_MX_NAME]; };
struct mx_channel {
    bool active, closing, pending, close_event, unblock_event, block_event;
    bool port, tty_blocked, ioctl_pending, ioctl_abandoned, ioctl_busy;
    unsigned generation, ioctl_command;
    uint8_t ioctl_data[8];
    v7_pid_t process_group;
    struct tty *input_tty,*output_tty;
    struct file *input_file,*output_file,*port_file;
    uint32_t signals;
    unsigned users, event_head, event_count, event_reserved;
    struct mx_queue x, y;
    struct mx_event events[NX_MX_EVENTS];
    struct mx_group *parent;
    unsigned index;
    struct file *endpoint;
};
static struct mx_group mx_groups[NX_MX_GROUPS];
static struct mx_channel mx_channels[NX_MX_CHANNELS],mx_ports[NX_MX_PORTS];
static struct mx_group *mx_listener;
static bool mx_used(struct mx_entry e) { return e.group || e.channel; }
static struct mx_group *mx_fd(struct user *u,int fd)
{
    if(fd<0 || fd>=V7_NOFILE)return NULL;
    struct file *f=u->u_ofile[fd];
    return f && (f->f_flag&V7_FMP)==V7_FMP ? f->f_private : NULL;
}
static uint16_t mpx_mode(struct fs_node node,uint16_t mode)
{
    for(unsigned i=0;i<NX_MX_GROUPS;++i)
        if(mx_groups[i].file && node_equal(inode_node(mx_groups[i].file->f_inode),node))
            return (mode&~V7_IFMT)|V7_IFMPC;
    return mode;
}
static int mx_slot(struct mx_group *g)
{
    for(unsigned i=0;i<NX_MX_INDEX;++i)if(!mx_used(g->entry[i]))return i;
    return -NX_ENXIO;
}
static struct mx_entry mx_lookup(struct mx_group *g,int32_t id)
{
    struct mx_entry none={0};
    /* Accept historical signed shorts as well as the native positive result. */
    if(id<INT16_MIN || id>UINT16_MAX)return none;
    uint16_t path=(uint16_t)id;
    bool direct=path<NX_MX_INDEX;
    for(unsigned level=0;level<NX_MX_LEVELS;++level){
        unsigned i=path&15;path>>=4;
        if(i>=NX_MX_INDEX)return none;
        struct mx_entry e=g->entry[i];
        if(!mx_used(e))return none;
        if((direct && level==0 && e.channel) || (path&15)==15 || level==NX_MX_LEVELS-1)return e;
        if(!e.group)return none;
        g=e.group;
    }
    return none;
}
static unsigned mx_index(struct mx_channel *c)
{
    unsigned path=0xfff0|c->index;
    for(struct mx_group *g=c->parent;g->parent;g=g->parent)
        path=((path<<4)|g->index)&0xffff;
    return path;
}
static unsigned mx_height(struct mx_group *g)
{
    unsigned height=1;
    for(unsigned i=0;i<NX_MX_INDEX;++i)if(g->entry[i].group){
        unsigned h=1+mx_height(g->entry[i].group);if(h>height)height=h;
    }
    return height;
}
static int mx_duplicate(struct user *u,struct file *f)
{
    int fd=fd_slot(u,0);if(fd<0)return fd;
    ++f->f_count;u->u_ofile[fd]=f;u->u_pofile[fd]=0;return fd;
}
static struct mx_channel *mx_allocate(struct mx_group *g)
{
    int i=mx_slot(g);if(i<0)return NULL;
    for(unsigned j=0;j<NX_MX_CHANNELS;++j)if(!mx_channels[j].active){
        struct mx_channel *c=&mx_channels[j];
        c->active=true;c->parent=g;c->index=i;g->entry[i].channel=c;return c;
    }
    return NULL;
}
#include "mpx_tty.inc"
#include "mpx_io.inc"
static void mx_detach(struct mx_entry e,bool orphan)
{
    if(e.group){
        struct mx_group *g=e.group,*parent=g->parent;
        mx_group_wake(g);
        parent->entry[g->index]=(struct mx_entry){0};g->parent=NULL;
        (void)file_put(g->file); /* Release the join's canonical-file reference. */
    }else{
        struct mx_channel *c=e.channel;
        /* Keep a detached endpoint alive without retaining the parent group. */
        ++c->users;
        c->closing=true;mx_notify(c);mx_bridge_clear(c,0);
        if(c->parent && (orphan || !c->endpoint)){
            c->parent->entry[c->index]=(struct mx_entry){0};c->parent=NULL;
        }
        mx_release(c);
    }
}
static void mpx_close(struct file *f)
{
    if((f->f_flag&V7_FMP)==V7_FMP){
        struct mx_group *g=f->f_private;
        /* A joined group cannot reach zero until its parent releases it. */
        if(g->parent)panic("joined mpx group lost reference");
        for(unsigned i=0;i<NX_MX_INDEX;++i)if(mx_used(g->entry[i]))mx_detach(g->entry[i],true);
        if(mx_listener==g)mx_listener=NULL;
        zero(g,sizeof(*g));
    }else{
        struct mx_channel *c=f->f_private;c->endpoint=NULL;mx_bridge_clear(c,0);
        if(c->parent && c->closing){
            mx_notify(c);c->parent->entry[c->index]=(struct mx_entry){0};c->parent=NULL;
        }else if(c->parent){c->closing=true;c->close_event=true;mx_notify(c);}
        mx_reap(c);
    }
}
static int mx_create(struct user *u,struct nx_mx_args *a,bool named)
{
    if(!root)return -NX_ENODEV;
    int fd=fd_slot(u,0);if(fd<0)return fd;
    struct mx_group *g=NULL;unsigned slot=0;
    for(unsigned i=0;i<NX_MX_GROUPS;++i)if(!mx_groups[i].file){g=&mx_groups[i];slot=i;break;}
    if(!g)return -NX_ENXIO;
    bool available=false;
    for(unsigned i=0;i<V7_NINODE;++i)available|=!inodes[i].i_count;
    if(!available)return -NX_ENFILE;
    struct file *f=file_alloc();if(!f)return -NX_ENFILE;
    struct fs_node node={.dev=0};int error;
    uint16_t device=(NX_MX_MAJOR<<8)|slot;
    if(named){
        char path[256],name[V7_DIRSIZ];struct fs_node parent;
        error=path_copy((uintptr_t)a->name,path);
        if(!error)error=parent_path(u,path,&parent,name);
        if(!error){node.dev=parent.dev;error=v7fs_mknod(node_volume(parent),parent.ino,name,
            V7_IFCHR|(a->arg[0]&0777&~u->u_cmask),u->u_uid,u->u_gid,device,&node.ino);}
    }else error=v7fs_anonymous(root,V7_IFCHR|0666,u->u_uid,u->u_gid,device,&node.ino);
    struct v7fs_inode disk;
    if(!error)error=node_inode_get(node,&disk);
    if(!error){f->f_inode=inode_get(node,&disk);if(!f->f_inode)error=-NX_ENFILE;}
    if(error){(void)file_put(f);return error;}
    f->f_flag=V7_FREAD|V7_FWRITE|V7_FMP;f->f_open_flags=NX_O_RDWR;f->f_private=g;
    g->file=f;u->u_ofile[fd]=f;u->u_pofile[fd]=0;return fd;
}
static int mpx_call(struct user *u,uint64_t command,uintptr_t address)
{
    struct nx_mx_args a;
    int error=copyin(&proc_current()->p_vm,address,&a,sizeof(a));if(error)return error;
    if(command==NX_MX_MPX || command==NX_MX_MPXN)return mx_create(u,&a,command==NX_MX_MPX);
    if(command==NX_MX_CONNECT || command==NX_MX_DISCON)return mx_connect(u,&a,command==NX_MX_DISCON);
    if(command==NX_MX_DEBUG){
        static volatile int values[30];int index=a.arg[0];if(index<0 || index>=30)return -NX_EINVAL;
        if(index==27)for(unsigned i=0;i<30;++i)values[i]=a.arg[1];
        else values[index]=a.arg[1];
        (void)values[index];return 0; /* Source diagnostics knobs, no transport effect. */
    }
    if(command==NX_MX_NPGRP && a.arg[1]<0){
        if(a.arg[2]<0)return -NX_EINVAL;
        return u->u_procp->p_pgrp=a.arg[2]?a.arg[2]:u->u_procp->p_pid;
    }
    if(command!=NX_MX_CHAN && command!=NX_MX_JOIN && command!=NX_MX_EXTR &&
       command!=NX_MX_ATTACH && command!=NX_MX_DETACH && command!=NX_MX_NPGRP && command!=NX_MX_CSIG)return -NX_EINVAL;
    struct mx_group *g=mx_fd(u,a.arg[1]);if(!g)return -NX_ENXIO;
    if(command==NX_MX_JOIN){
        struct mx_group *child=mx_fd(u,a.arg[0]);if(!child)return mx_join_tty(u,g,a.arg[0]);
        if(child->parent)return -NX_ENXIO;
        unsigned depth=1;
        for(struct mx_group *p=g;p;p=p->parent){if(p==child)return -NX_EINVAL;if(p->parent)++depth;}
        if(depth+mx_height(child)>NX_MX_LEVELS)return -NX_EINVAL;
        int i=mx_slot(g);if(i<0)return i;
        g->entry[i].group=child;child->parent=g;child->index=i;++child->file->f_count;mx_group_wake(child);return i;
    }
    if(command==NX_MX_CHAN){
        struct mx_channel *c=mx_allocate(g);return c?(int)mx_index(c):-NX_ENXIO;
    }
    struct mx_entry e=mx_lookup(g,a.arg[0]);if(!mx_used(e))return -NX_ENXIO;
    if(command==NX_MX_NPGRP || command==NX_MX_CSIG){
        if(!e.channel || e.channel->closing || e.channel->pending)return -NX_ENXIO;
        if(command==NX_MX_NPGRP){
            if(a.arg[2]<0)return -NX_EINVAL;
            u->u_procp->p_pgrp=a.arg[2]?a.arg[2]:u->u_procp->p_pid;
            e.channel->process_group=u->u_procp->p_pgrp;return u->u_procp->p_pgrp;
        }
        if(a.arg[2]<=0 || a.arg[2]>=NX_NSIG)return -NX_EINVAL;
        if(!e.channel->port && e.channel->process_group){
            int error=signal_group_user(e.channel->process_group,a.arg[2]);if(error)return error;
        }
        return a.arg[2];
    }
    if(command==NX_MX_DETACH){mx_detach(e,false);return 0;}
    if(command==NX_MX_ATTACH){
        if(!e.channel || e.channel->closing)return -NX_ENXIO;
        e.channel->pending=false;mx_notify(e.channel);return mx_index(e.channel);
    }
    if(e.group)return mx_duplicate(u,e.group->file);
    struct mx_channel *c=e.channel;
    if(c->port)return mx_duplicate(u,c->port_file);
    if(c->closing || c->pending)return -NX_ENXIO;
    if(c->endpoint)return mx_duplicate(u,c->endpoint);
    int fd=fd_slot(u,0);if(fd<0)return fd;
    struct file *f=file_alloc();if(!f)return -NX_ENFILE;
    f->f_flag=V7_FREAD|V7_FWRITE|(a.arg[2]?V7_FMPY:V7_FMPX);
    f->f_open_flags=NX_O_RDWR;f->f_private=c;
    /* For a nested channel, use its immediate group's inode. */
    f->f_inode=c->parent->file->f_inode;++f->f_inode->i_count;c->endpoint=f;
    u->u_ofile[fd]=f;u->u_pofile[fd]=0;return fd;
}
